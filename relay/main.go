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

	webRoot := os.Getenv("PRINTDROP_WEB_ROOT")
	if webRoot == "" {
		webRoot = "web"
	}

	relay := newRelayServer(5 * time.Minute)
	server := &http.Server{
		Addr:              addr,
		Handler:           newPublicHandler(relay, webRoot),
		ReadHeaderTimeout: 5 * time.Second,
		IdleTimeout:       75 * time.Second,
	}

	log.Printf("PrintDrop relay listening on %s with web root %s", addr, webRoot)
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatal(err)
	}
}
