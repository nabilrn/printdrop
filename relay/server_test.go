package main

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/coder/websocket"
)

const (
	testSession = "00112233445566778899aabbccddeeff"
	testSecret  = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
	otherSecret = "f00102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
)

func TestSessionRegistrationIsIdempotentForSameSecret(t *testing.T) {
	relay := newRelayServer(time.Minute)
	server := httptest.NewServer(relay.routes())
	defer server.Close()

	if status := registerForTest(t, server.URL, testSession, testSecret); status != http.StatusCreated {
		t.Fatalf("first registration status = %d, want %d", status, http.StatusCreated)
	}
	if status := registerForTest(t, server.URL, testSession, testSecret); status != http.StatusOK {
		t.Fatalf("idempotent registration status = %d, want %d", status, http.StatusOK)
	}
	if status := registerForTest(t, server.URL, testSession, otherSecret); status != http.StatusConflict {
		t.Fatalf("conflicting registration status = %d, want %d", status, http.StatusConflict)
	}
}

func TestReceiverRequiresRegisteredSecret(t *testing.T) {
	relay := newRelayServer(time.Minute)
	server := httptest.NewServer(relay.routes())
	defer server.Close()
	registerForTest(t, server.URL, testSession, testSecret)

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	_, response, err := websocket.Dial(ctx,
		wsURL(server.URL)+"/v1/receiver/"+testSession,
		&websocket.DialOptions{HTTPHeader: http.Header{"Authorization": []string{"Bearer " + otherSecret}}})
	if err == nil {
		t.Fatal("receiver dial with wrong secret unexpectedly succeeded")
	}
	if response == nil || response.StatusCode != http.StatusUnauthorized {
		t.Fatalf("wrong-secret response = %#v, want HTTP 401", response)
	}
}

func TestBinaryFramesBridgeBothDirections(t *testing.T) {
	relay := newRelayServer(time.Minute)
	server := httptest.NewServer(relay.routes())
	defer server.Close()
	registerForTest(t, server.URL, testSession, testSecret)

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	receiver, response, err := websocket.Dial(ctx,
		wsURL(server.URL)+"/v1/receiver/"+testSession,
		&websocket.DialOptions{HTTPHeader: http.Header{"Authorization": []string{"Bearer " + testSecret}}})
	if err != nil {
		t.Fatalf("receiver dial: %v (response=%v)", err, response)
	}
	defer receiver.CloseNow()

	sender, response, err := websocket.Dial(ctx, wsURL(server.URL)+"/v1/sender/"+testSession, nil)
	if err != nil {
		t.Fatalf("sender dial: %v (response=%v)", err, response)
	}
	defer sender.CloseNow()

	toReceiver := []byte{0x50, 0x44, 0x52, 0x50, 0x01, 0x04, 0x00, 0x00}
	if err := sender.Write(ctx, websocket.MessageBinary, toReceiver); err != nil {
		t.Fatalf("sender write: %v", err)
	}
	messageType, received, err := receiver.Read(ctx)
	if err != nil {
		t.Fatalf("receiver read: %v", err)
	}
	if messageType != websocket.MessageBinary || !bytes.Equal(received, toReceiver) {
		t.Fatalf("receiver got type=%v data=%x", messageType, received)
	}

	toSender := []byte{0x50, 0x44, 0x52, 0x50, 0x01, 0x06, 0x00, 0x00}
	if err := receiver.Write(ctx, websocket.MessageBinary, toSender); err != nil {
		t.Fatalf("receiver write: %v", err)
	}
	messageType, received, err = sender.Read(ctx)
	if err != nil {
		t.Fatalf("sender read: %v", err)
	}
	if messageType != websocket.MessageBinary || !bytes.Equal(received, toSender) {
		t.Fatalf("sender got type=%v data=%x", messageType, received)
	}
}

func TestSenderCannotAttachBeforeReceiver(t *testing.T) {
	relay := newRelayServer(time.Minute)
	server := httptest.NewServer(relay.routes())
	defer server.Close()
	registerForTest(t, server.URL, testSession, testSecret)

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	_, response, err := websocket.Dial(ctx, wsURL(server.URL)+"/v1/sender/"+testSession, nil)
	if err == nil {
		t.Fatal("sender dial without receiver unexpectedly succeeded")
	}
	if response == nil || response.StatusCode != http.StatusNotFound {
		t.Fatalf("sender response = %#v, want HTTP 404", response)
	}
}

func registerForTest(t *testing.T, baseURL, sessionID, secret string) int {
	t.Helper()
	body, err := json.Marshal(registerSessionRequest{SessionID: sessionID})
	if err != nil {
		t.Fatal(err)
	}
	request, err := http.NewRequest(http.MethodPost, baseURL+"/v1/sessions", bytes.NewReader(body))
	if err != nil {
		t.Fatal(err)
	}
	request.Header.Set("Authorization", "Bearer "+secret)
	request.Header.Set("Content-Type", "application/json")
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	return response.StatusCode
}

func wsURL(httpURL string) string {
	return "ws" + strings.TrimPrefix(httpURL, "http")
}
