package main

import (
	"context"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"sync"
	"time"

	"github.com/coder/websocket"
)

const (
	publicSessionHexChars     = 32
	receiverSecretHexChars    = 64
	maxWSMessageBytes         = 60 * 1024
	registerBodyLimit         = 1024
	peerWriteTimeout          = 30 * time.Second
	defaultSenderIdleTimeout  = 45 * time.Second
)

var (
	errSessionNotFound = errors.New("session not found")
	errSessionConflict = errors.New("session conflict")
	errReceiverBusy    = errors.New("receiver already connected")
	errSenderBusy      = errors.New("sender already connected")
	errInvalidSecret   = errors.New("invalid receiver secret")
)

type relayServer struct {
	mu                sync.Mutex
	sessions          map[string]*sessionRecord
	ttl               time.Duration
	senderIdleTimeout time.Duration
}

type sessionRecord struct {
	secretHash [sha256.Size]byte
	expiresAt  time.Time
	timer      *time.Timer
	active     *relayBridge
}

type relayBridge struct {
	ctx        context.Context
	cancel     context.CancelFunc
	toReceiver chan []byte
	toSender   chan []byte
	mu         sync.Mutex
	sender     bool
}

type registerSessionRequest struct {
	SessionID string `json:"session_id"`
}

type registerSessionResponse struct {
	SessionID string `json:"session_id"`
	ExpiresAt string `json:"expires_at"`
}

func newRelayServer(ttl time.Duration) *relayServer {
	return &relayServer{
		sessions:          make(map[string]*sessionRecord),
		ttl:               ttl,
		senderIdleTimeout: defaultSenderIdleTimeout,
	}
}

func (s *relayServer) routes() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/healthz", s.handleHealth)
	mux.HandleFunc("/v1/sessions", s.handleRegisterSession)
	mux.HandleFunc("/v1/receiver/", s.handleReceiver)
	mux.HandleFunc("/v1/sender/", s.handleSender)
	return mux
}

func (s *relayServer) handleHealth(w http.ResponseWriter, _ *http.Request) {
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte("ok\n"))
}

func (s *relayServer) handleRegisterSession(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		w.Header().Set("Allow", http.MethodPost)
		http.Error(w, http.StatusText(http.StatusMethodNotAllowed), http.StatusMethodNotAllowed)
		return
	}

	secret, ok := bearerSecret(r.Header.Get("Authorization"))
	if !ok {
		http.Error(w, http.StatusText(http.StatusUnauthorized), http.StatusUnauthorized)
		return
	}

	r.Body = http.MaxBytesReader(w, r.Body, registerBodyLimit)
	defer r.Body.Close()
	var request registerSessionRequest
	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&request); err != nil || !validLowerHex(request.SessionID, publicSessionHexChars) {
		http.Error(w, http.StatusText(http.StatusBadRequest), http.StatusBadRequest)
		return
	}
	var trailing any
	if err := decoder.Decode(&trailing); !errors.Is(err, io.EOF) {
		http.Error(w, http.StatusText(http.StatusBadRequest), http.StatusBadRequest)
		return
	}

	record, created, err := s.register(request.SessionID, secret)
	if err != nil {
		if errors.Is(err, errSessionConflict) {
			http.Error(w, http.StatusText(http.StatusConflict), http.StatusConflict)
			return
		}
		http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
		return
	}

	status := http.StatusOK
	if created {
		status = http.StatusCreated
	}
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(registerSessionResponse{
		SessionID: request.SessionID,
		ExpiresAt: record.expiresAt.UTC().Format(time.RFC3339),
	})
}

func (s *relayServer) handleReceiver(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, http.StatusText(http.StatusMethodNotAllowed), http.StatusMethodNotAllowed)
		return
	}
	id, ok := pathSessionID(r.URL.Path, "/v1/receiver/")
	if !ok {
		http.Error(w, http.StatusText(http.StatusNotFound), http.StatusNotFound)
		return
	}
	secret, ok := bearerSecret(r.Header.Get("Authorization"))
	if !ok {
		http.Error(w, http.StatusText(http.StatusUnauthorized), http.StatusUnauthorized)
		return
	}

	bridge, err := s.attachReceiver(id, secret)
	if err != nil {
		writeAttachError(w, err)
		return
	}

	conn, err := websocket.Accept(w, r, nil)
	if err != nil {
		s.detachReceiver(id, bridge)
		return
	}
	defer conn.CloseNow()
	conn.SetReadLimit(maxWSMessageBytes)
	defer s.detachReceiver(id, bridge)
	_ = servePeer(bridge.ctx, conn, bridge.toSender, bridge.toReceiver, 0)
}

func (s *relayServer) handleSender(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, http.StatusText(http.StatusMethodNotAllowed), http.StatusMethodNotAllowed)
		return
	}
	id, ok := pathSessionID(r.URL.Path, "/v1/sender/")
	if !ok {
		http.Error(w, http.StatusText(http.StatusNotFound), http.StatusNotFound)
		return
	}

	bridge, err := s.attachSender(id)
	if err != nil {
		writeAttachError(w, err)
		return
	}

	conn, err := websocket.Accept(w, r, nil)
	if err != nil {
		bridge.detachSender()
		return
	}
	defer conn.CloseNow()
	conn.SetReadLimit(maxWSMessageBytes)
	defer func() {
		bridge.detachSender()
		bridge.cancel()
	}()
	_ = servePeer(bridge.ctx, conn, bridge.toReceiver, bridge.toSender, s.senderIdleTimeout)
}

func writeAttachError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, errInvalidSecret):
		http.Error(w, http.StatusText(http.StatusUnauthorized), http.StatusUnauthorized)
	case errors.Is(err, errReceiverBusy), errors.Is(err, errSenderBusy), errors.Is(err, errSessionConflict):
		http.Error(w, http.StatusText(http.StatusConflict), http.StatusConflict)
	case errors.Is(err, errSessionNotFound):
		http.Error(w, http.StatusText(http.StatusNotFound), http.StatusNotFound)
	default:
		http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
	}
}

func (s *relayServer) register(id, secret string) (*sessionRecord, bool, error) {
	now := time.Now()
	hash := sha256.Sum256([]byte(secret))

	s.mu.Lock()
	defer s.mu.Unlock()

	if existing := s.sessions[id]; existing != nil {
		if !now.Before(existing.expiresAt) {
			s.expireLocked(id, existing)
		} else {
			if subtle.ConstantTimeCompare(existing.secretHash[:], hash[:]) != 1 {
				return nil, false, errSessionConflict
			}
			return existing, false, nil
		}
	}

	record := &sessionRecord{secretHash: hash, expiresAt: now.Add(s.ttl)}
	s.sessions[id] = record
	record.timer = time.AfterFunc(s.ttl, func() { s.expire(id, record) })
	return record, true, nil
}

func (s *relayServer) attachReceiver(id, secret string) (*relayBridge, error) {
	hash := sha256.Sum256([]byte(secret))

	s.mu.Lock()
	defer s.mu.Unlock()

	record := s.sessions[id]
	if record == nil || !time.Now().Before(record.expiresAt) {
		if record != nil {
			s.expireLocked(id, record)
		}
		return nil, errSessionNotFound
	}
	if subtle.ConstantTimeCompare(record.secretHash[:], hash[:]) != 1 {
		return nil, errInvalidSecret
	}
	if record.active != nil {
		return nil, errReceiverBusy
	}

	ctx, cancel := context.WithCancel(context.Background())
	bridge := &relayBridge{
		ctx:        ctx,
		cancel:     cancel,
		toReceiver: make(chan []byte, 1),
		toSender:   make(chan []byte, 1),
	}
	record.active = bridge
	return bridge, nil
}

func (s *relayServer) detachReceiver(id string, bridge *relayBridge) {
	s.mu.Lock()
	defer s.mu.Unlock()
	record := s.sessions[id]
	if record != nil && record.active == bridge {
		record.active = nil
	}
	bridge.cancel()
}

func (s *relayServer) attachSender(id string) (*relayBridge, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	record := s.sessions[id]
	if record == nil || !time.Now().Before(record.expiresAt) {
		if record != nil {
			s.expireLocked(id, record)
		}
		return nil, errSessionNotFound
	}
	if record.active == nil {
		return nil, errSessionNotFound
	}
	if !record.active.attachSender() {
		return nil, errSenderBusy
	}
	return record.active, nil
}

func (s *relayServer) expire(id string, expected *sessionRecord) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.sessions[id] == expected {
		s.expireLocked(id, expected)
	}
}

func (s *relayServer) expireLocked(id string, record *sessionRecord) {
	delete(s.sessions, id)
	if record.timer != nil {
		record.timer.Stop()
	}
	if record.active != nil {
		record.active.cancel()
		record.active = nil
	}
}

func (b *relayBridge) attachSender() bool {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.sender {
		return false
	}
	b.sender = true
	return true
}

func (b *relayBridge) detachSender() {
	b.mu.Lock()
	b.sender = false
	b.mu.Unlock()
}

func servePeer(ctx context.Context, conn *websocket.Conn, readDestination chan<- []byte, writes <-chan []byte, idleTimeout time.Duration) error {
	peerCtx, cancel := context.WithCancel(ctx)
	defer cancel()

	go func() {
		for {
			select {
			case <-peerCtx.Done():
				return
			case message := <-writes:
				writeCtx, writeCancel := context.WithTimeout(peerCtx, peerWriteTimeout)
				err := conn.Write(writeCtx, websocket.MessageBinary, message)
				writeCancel()
				if err != nil {
					cancel()
					return
				}
			}
		}
	}()

	for {
		readCtx := peerCtx
		readCancel := func() {}
		if idleTimeout > 0 {
			readCtx, readCancel = context.WithTimeout(peerCtx, idleTimeout)
		}
		messageType, data, err := conn.Read(readCtx)
		readCancel()
		if err != nil {
			cancel()
			return err
		}
		if messageType != websocket.MessageBinary || len(data) > maxWSMessageBytes {
			cancel()
			return fmt.Errorf("invalid relay message")
		}
		message := append([]byte(nil), data...)
		select {
		case <-peerCtx.Done():
			return peerCtx.Err()
		case readDestination <- message:
		}
	}
}

func pathSessionID(path, prefix string) (string, bool) {
	if !strings.HasPrefix(path, prefix) {
		return "", false
	}
	id := strings.TrimPrefix(path, prefix)
	if strings.Contains(id, "/") || !validLowerHex(id, publicSessionHexChars) {
		return "", false
	}
	return id, true
}

func bearerSecret(value string) (string, bool) {
	const prefix = "Bearer "
	if !strings.HasPrefix(value, prefix) {
		return "", false
	}
	secret := strings.TrimPrefix(value, prefix)
	if !validLowerHex(secret, receiverSecretHexChars) {
		return "", false
	}
	return secret, true
}

func validLowerHex(value string, length int) bool {
	if len(value) != length {
		return false
	}
	for _, character := range value {
		if (character < '0' || character > '9') && (character < 'a' || character > 'f') {
			return false
		}
	}
	return true
}
