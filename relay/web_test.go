package main

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

const testSessionID = "00112233445566778899aabbccddeeff"
const testReceiverSecret = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"

func makeWebRoot(t *testing.T) string {
	t.Helper()
	root := t.TempDir()
	files := map[string]string{
		"index.html": "<!doctype html><title>PrintDrop test sender</title>",
		"app.js":     "console.log('app')",
		"sender.js":  "export const sender = true;",
		"protocol.js": "export const protocol = true;",
		"style.css":   "body{margin:0}",
		filepath.Join("vendor", "js-sha256", "build", "sha256.min.js"): "window.sha256={};",
	}
	for name, content := range files {
		path := filepath.Join(root, name)
		if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	return root
}

func registeredPublicHandler(t *testing.T) (*relayServer, http.Handler) {
	t.Helper()
	relay := newRelayServer(5 * time.Minute)
	if _, _, err := relay.register(testSessionID, testReceiverSecret); err != nil {
		t.Fatal(err)
	}
	return relay, newPublicHandler(relay, makeWebRoot(t))
}

func TestSessionPageRequiresLiveCanonicalSession(t *testing.T) {
	_, handler := registeredPublicHandler(t)

	request := httptest.NewRequest(http.MethodGet, "/s/"+testSessionID, nil)
	response := httptest.NewRecorder()
	handler.ServeHTTP(response, request)
	if response.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", response.Code)
	}
	if response.Header().Get("Cache-Control") != "no-store" {
		t.Fatalf("unexpected cache policy: %q", response.Header().Get("Cache-Control"))
	}
	if response.Header().Get("Content-Type") != "text/html; charset=utf-8" {
		t.Fatalf("unexpected content type: %q", response.Header().Get("Content-Type"))
	}
	if !strings.Contains(response.Body.String(), "PrintDrop test sender") {
		t.Fatal("sender HTML missing")
	}

	for _, path := range []string{
		"/s/not-a-session",
		"/s/00112233445566778899AABBCCDDEEFF",
		"/s/ffffffffffffffffffffffffffffffff",
		"/s/" + testSessionID + "/extra",
	} {
		response = httptest.NewRecorder()
		handler.ServeHTTP(response, httptest.NewRequest(http.MethodGet, path, nil))
		if response.Code != http.StatusNotFound {
			t.Fatalf("expected 404 for %s, got %d", path, response.Code)
		}
	}
}

func TestSessionPageExpiresWithSession(t *testing.T) {
	relay := newRelayServer(time.Millisecond)
	if _, _, err := relay.register(testSessionID, testReceiverSecret); err != nil {
		t.Fatal(err)
	}
	handler := newPublicHandler(relay, makeWebRoot(t))
	time.Sleep(5 * time.Millisecond)

	response := httptest.NewRecorder()
	handler.ServeHTTP(response, httptest.NewRequest(http.MethodGet, "/s/"+testSessionID, nil))
	if response.Code != http.StatusNotFound {
		t.Fatalf("expected expired session 404, got %d", response.Code)
	}
}

func TestStaticAssetAllowlistAndHeaders(t *testing.T) {
	_, handler := registeredPublicHandler(t)

	cases := []struct {
		path        string
		contentType string
	}{
		{"/app.js", "text/javascript; charset=utf-8"},
		{"/sender.js", "text/javascript; charset=utf-8"},
		{"/protocol.js", "text/javascript; charset=utf-8"},
		{"/style.css", "text/css; charset=utf-8"},
		{"/vendor/js-sha256/build/sha256.min.js", "text/javascript; charset=utf-8"},
	}

	for _, tc := range cases {
		response := httptest.NewRecorder()
		handler.ServeHTTP(response, httptest.NewRequest(http.MethodGet, tc.path, nil))
		if response.Code != http.StatusOK {
			t.Fatalf("%s: expected 200, got %d", tc.path, response.Code)
		}
		if response.Header().Get("Content-Type") != tc.contentType {
			t.Fatalf("%s: wrong content type %q", tc.path, response.Header().Get("Content-Type"))
		}
		if response.Header().Get("X-Content-Type-Options") != "nosniff" {
			t.Fatalf("%s: missing nosniff", tc.path)
		}
	}

	for _, path := range []string{
		"/index.html",
		"/../relay/server.go",
		"/vendor/js-sha256/../../../../relay/server.go",
		"/vendor/js-sha256/package.json",
	} {
		response := httptest.NewRecorder()
		handler.ServeHTTP(response, httptest.NewRequest(http.MethodGet, path, nil))
		if response.Code != http.StatusNotFound {
			t.Fatalf("expected allowlist 404 for %s, got %d", path, response.Code)
		}
	}
}

func TestPublicHandlerPreservesRelayAPI(t *testing.T) {
	_, handler := registeredPublicHandler(t)
	response := httptest.NewRecorder()
	handler.ServeHTTP(response, httptest.NewRequest(http.MethodGet, "/healthz", nil))
	if response.Code != http.StatusOK || response.Body.String() != "ok\n" {
		t.Fatalf("health endpoint changed: code=%d body=%q", response.Code, response.Body.String())
	}
}
