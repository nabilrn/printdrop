package main

import (
	"log"
	"net/http"
	"os"
	"time"
)

func main() {
	addr := os.Getenv("PRINTDROP_RELAY_ADDR")
	if addr == "" {
		addr = ":8080"
	}
	webDir := os.Getenv("PRINTDROP_WEB_DIR")
	if webDir == "" {
		webDir = "../web"
	}

	relay := newRelayServer(5 * time.Minute)
	handler, err := newSenderWebHandler(relay.routes(), os.DirFS(webDir))
	if err != nil {
		log.Fatalf("PrintDrop sender web initialization failed: %v", err)
	}
	server := &http.Server{
		Addr:              addr,
		Handler:           handler,
		ReadHeaderTimeout: 5 * time.Second,
		IdleTimeout:       75 * time.Second,
	}

	log.Printf("PrintDrop relay listening on %s with sender web from %s", addr, webDir)
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatal(err)
	}
}
