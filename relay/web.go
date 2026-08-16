package main

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"
)

type publicHandler struct {
	relay   *relayServer
	api     http.Handler
	webRoot string
}

type webAsset struct {
	path        string
	contentType string
	cache       string
}

var publicWebAssets = map[string]webAsset{
	"/app.js": {
		path:        "app.js",
		contentType: "text/javascript; charset=utf-8",
		cache:       "no-cache",
	},
	"/sender.js": {
		path:        "sender.js",
		contentType: "text/javascript; charset=utf-8",
		cache:       "no-cache",
	},
	"/protocol.js": {
		path:        "protocol.js",
		contentType: "text/javascript; charset=utf-8",
		cache:       "no-cache",
	},
	"/style.css": {
		path:        "style.css",
		contentType: "text/css; charset=utf-8",
		cache:       "no-cache",
	},
	"/vendor/js-sha256/build/sha256.min.js": {
		path:        filepath.Join("vendor", "js-sha256", "build", "sha256.min.js"),
		contentType: "text/javascript; charset=utf-8",
		cache:       "public, max-age=31536000, immutable",
	},
}

func newPublicHandler(relay *relayServer, webRoot string) http.Handler {
	if relay == nil {
		panic("nil relay server")
	}
	return &publicHandler{
		relay:   relay,
		api:     relay.routes(),
		webRoot: filepath.Clean(webRoot),
	}
}

func (h *publicHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if strings.HasPrefix(r.URL.Path, "/s/") {
		h.serveSessionPage(w, r)
		return
	}
	if asset, ok := publicWebAssets[r.URL.Path]; ok {
		h.serveAsset(w, r, asset)
		return
	}
	h.api.ServeHTTP(w, r)
}

func (h *publicHandler) serveSessionPage(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet && r.Method != http.MethodHead {
		w.Header().Set("Allow", "GET, HEAD")
		http.Error(w, http.StatusText(http.StatusMethodNotAllowed), http.StatusMethodNotAllowed)
		return
	}

	id, ok := pathSessionID(r.URL.Path, "/s/")
	if !ok || !h.relay.sessionAvailable(id, time.Now()) {
		http.NotFound(w, r)
		return
	}

	h.serveAsset(w, r, webAsset{
		path:        "index.html",
		contentType: "text/html; charset=utf-8",
		cache:       "no-store",
	})
}

func (h *publicHandler) serveAsset(w http.ResponseWriter, r *http.Request, asset webAsset) {
	if r.Method != http.MethodGet && r.Method != http.MethodHead {
		w.Header().Set("Allow", "GET, HEAD")
		http.Error(w, http.StatusText(http.StatusMethodNotAllowed), http.StatusMethodNotAllowed)
		return
	}

	path := filepath.Join(h.webRoot, asset.path)
	file, err := os.Open(path)
	if err != nil {
		http.NotFound(w, r)
		return
	}
	defer file.Close()

	info, err := file.Stat()
	if err != nil || !info.Mode().IsRegular() {
		http.NotFound(w, r)
		return
	}

	w.Header().Set("Content-Type", asset.contentType)
	w.Header().Set("Cache-Control", asset.cache)
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.Header().Set("Referrer-Policy", "no-referrer")
	w.Header().Set("Cross-Origin-Resource-Policy", "same-origin")
	http.ServeContent(w, r, info.Name(), info.ModTime(), file)
}

func (s *relayServer) sessionAvailable(id string, now time.Time) bool {
	s.mu.Lock()
	defer s.mu.Unlock()

	record := s.sessions[id]
	if record == nil {
		return false
	}
	if !now.Before(record.expiresAt) {
		s.expireLocked(id, record)
		return false
	}
	return true
}
