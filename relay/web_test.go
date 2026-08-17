package main

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"testing/fstest"
	"time"
)

func TestSenderWebServesSessionPage(t *testing.T) {
	relay := newRelayServer(time.Minute)
	handler, err := newSenderWebHandler(relay.routes(), senderWebTestFS())
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewServer(handler)
	defer server.Close()

	for _, suffix := range []string{"", "/"} {
		response, err := http.Get(server.URL + "/s/" + testSession + suffix)
		if err != nil {
			t.Fatal(err)
		}
		body, readErr := io.ReadAll(response.Body)
		response.Body.Close()
		if readErr != nil {
			t.Fatal(readErr)
		}
		if response.StatusCode != http.StatusOK {
			t.Fatalf("session page status = %d, want %d", response.StatusCode, http.StatusOK)
		}
		if string(body) != "INDEX" {
			t.Fatalf("session page body = %q, want INDEX", body)
		}
		if response.Header.Get("Cache-Control") != "no-store" {
			t.Fatalf("session page Cache-Control = %q, want no-store", response.Header.Get("Cache-Control"))
		}
		if response.Header.Get("Content-Security-Policy") == "" {
			t.Fatal("session page is missing Content-Security-Policy")
		}
		if response.Header.Get("Referrer-Policy") != "no-referrer" {
			t.Fatalf("session page Referrer-Policy = %q, want no-referrer", response.Header.Get("Referrer-Policy"))
		}
	}
}

func TestSenderWebRejectsInvalidSessionPage(t *testing.T) {
	relay := newRelayServer(time.Minute)
	handler, err := newSenderWebHandler(relay.routes(), senderWebTestFS())
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewServer(handler)
	defer server.Close()

	response, err := http.Get(server.URL + "/s/not-a-session")
	if err != nil {
		t.Fatal(err)
	}
	response.Body.Close()
	if response.StatusCode != http.StatusNotFound {
		t.Fatalf("invalid session page status = %d, want %d", response.StatusCode, http.StatusNotFound)
	}

	request, err := http.NewRequest(http.MethodPost, server.URL+"/s/"+testSession, nil)
	if err != nil {
		t.Fatal(err)
	}
	response, err = http.DefaultClient.Do(request)
	if err != nil {
		t.Fatal(err)
	}
	response.Body.Close()
	if response.StatusCode != http.StatusMethodNotAllowed {
		t.Fatalf("POST session page status = %d, want %d", response.StatusCode, http.StatusMethodNotAllowed)
	}
	if response.Header.Get("Allow") != "GET, HEAD" {
		t.Fatalf("POST session page Allow = %q, want GET, HEAD", response.Header.Get("Allow"))
	}
}

func TestSenderWebServesOnlyRequiredAssets(t *testing.T) {
	relay := newRelayServer(time.Minute)
	handler, err := newSenderWebHandler(relay.routes(), senderWebTestFS())
	if err != nil {
		t.Fatal(err)
	}
	server := httptest.NewServer(handler)
	defer server.Close()

	response, err := http.Get(server.URL + "/app.js")
	if err != nil {
		t.Fatal(err)
	}
	body, readErr := io.ReadAll(response.Body)
	response.Body.Close()
	if readErr != nil {
		t.Fatal(readErr)
	}
	if response.StatusCode != http.StatusOK || string(body) != "APP" {
		t.Fatalf("app.js response status=%d body=%q", response.StatusCode, body)
	}
	if response.Header.Get("Content-Type") != "text/javascript; charset=utf-8" {
		t.Fatalf("app.js Content-Type = %q", response.Header.Get("Content-Type"))
	}
	if response.Header.Get("X-Content-Type-Options") != "nosniff" {
		t.Fatalf("app.js X-Content-Type-Options = %q", response.Header.Get("X-Content-Type-Options"))
	}

	response, err = http.Get(server.URL + "/package.json")
	if err != nil {
		t.Fatal(err)
	}
	response.Body.Close()
	if response.StatusCode != http.StatusNotFound {
		t.Fatalf("package.json status = %d, want %d", response.StatusCode, http.StatusNotFound)
	}
}

func TestSenderWebRequiresCompleteAssetSet(t *testing.T) {
	files := senderWebTestFS()
	delete(files, "app.js")

	relay := newRelayServer(time.Minute)
	_, err := newSenderWebHandler(relay.routes(), files)
	if err == nil || !strings.Contains(err.Error(), "app.js") {
		t.Fatalf("missing app.js error = %v", err)
	}
}

func senderWebTestFS() fstest.MapFS {
	return fstest.MapFS{
		"index.html":                           &fstest.MapFile{Data: []byte("INDEX")},
		"style.css":                            &fstest.MapFile{Data: []byte("STYLE")},
		"app.js":                               &fstest.MapFile{Data: []byte("APP")},
		"sender.js":                            &fstest.MapFile{Data: []byte("SENDER")},
		"protocol.js":                          &fstest.MapFile{Data: []byte("PROTOCOL")},
		"vendor/js-sha256/build/sha256.min.js": &fstest.MapFile{Data: []byte("SHA")},
	}
}
