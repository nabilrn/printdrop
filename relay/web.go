package main

import (
	"fmt"
	"io/fs"
	"net/http"
	"strings"
)

type senderWebAsset struct {
	file        string
	contentType string
	cache       string
}

type senderWebHandler struct {
	next  http.Handler
	files fs.FS
}

var senderWebAssets = map[string]senderWebAsset{
	"/style.css": {
		file:        "style.css",
		contentType: "text/css; charset=utf-8",
		cache:       "public, max-age=300",
	},
	"/app.js": {
		file:        "app.js",
		contentType: "text/javascript; charset=utf-8",
		cache:       "public, max-age=300",
	},
	"/sender.js": {
		file:        "sender.js",
		contentType: "text/javascript; charset=utf-8",
		cache:       "public, max-age=300",
	},
	"/protocol.js": {
		file:        "protocol.js",
		contentType: "text/javascript; charset=utf-8",
		cache:       "public, max-age=300",
	},
	"/vendor/js-sha256/build/sha256.min.js": {
		file:        "vendor/js-sha256/build/sha256.min.js",
		contentType: "text/javascript; charset=utf-8",
		cache:       "public, max-age=300",
	},
}

func newSenderWebHandler(next http.Handler, files fs.FS) (*senderWebHandler, error) {
	if next == nil {
		return nil, fmt.Errorf("sender web handler requires a fallback handler")
	}
	if files == nil {
		return nil, fmt.Errorf("sender web handler requires a filesystem")
	}

	required := []string{"index.html"}
	for _, asset := range senderWebAssets {
		required = append(required, asset.file)
	}
	for _, path := range required {
		info, err := fs.Stat(files, path)
		if err != nil {
			return nil, fmt.Errorf("sender web asset %q: %w", path, err)
		}
		if info.IsDir() {
			return nil, fmt.Errorf("sender web asset %q is not a file", path)
		}
	}

	return &senderWebHandler{next: next, files: files}, nil
}

func (h *senderWebHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if strings.HasPrefix(r.URL.Path, "/s/") {
		h.serveSessionPage(w, r)
		return
	}
	if asset, ok := senderWebAssets[r.URL.Path]; ok {
		h.serveAsset(w, r, asset)
		return
	}
	h.next.ServeHTTP(w, r)
}

func (h *senderWebHandler) serveSessionPage(w http.ResponseWriter, r *http.Request) {
	if !senderWebMethodAllowed(w, r) {
		return
	}

	path := r.URL.Path
	if strings.HasSuffix(path, "/") {
		path = strings.TrimSuffix(path, "/")
	}
	if _, ok := pathSessionID(path, "/s/"); !ok {
		http.NotFound(w, r)
		return
	}

	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("Content-Security-Policy", "default-src 'self'; connect-src 'self' ws: wss:; img-src 'self'; script-src 'self'; style-src 'self'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'")
	w.Header().Set("Referrer-Policy", "no-referrer")
	h.serveFile(w, r, senderWebAsset{
		file:        "index.html",
		contentType: "text/html; charset=utf-8",
		cache:       "no-store",
	})
}

func (h *senderWebHandler) serveAsset(w http.ResponseWriter, r *http.Request, asset senderWebAsset) {
	if !senderWebMethodAllowed(w, r) {
		return
	}
	h.serveFile(w, r, asset)
}

func (h *senderWebHandler) serveFile(w http.ResponseWriter, r *http.Request, asset senderWebAsset) {
	data, err := fs.ReadFile(h.files, asset.file)
	if err != nil {
		http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", asset.contentType)
	w.Header().Set("Cache-Control", asset.cache)
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.WriteHeader(http.StatusOK)
	if r.Method != http.MethodHead {
		_, _ = w.Write(data)
	}
}

func senderWebMethodAllowed(w http.ResponseWriter, r *http.Request) bool {
	if r.Method == http.MethodGet || r.Method == http.MethodHead {
		return true
	}
	w.Header().Set("Allow", "GET, HEAD")
	http.Error(w, http.StatusText(http.StatusMethodNotAllowed), http.StatusMethodNotAllowed)
	return false
}
