# PrintDrop MVP Deployment

PrintDrop V0.1 uses one public origin for the browser sender, session registration, and WebSocket relay.

The Windows receiver defaults to `https://send.printdrop.app`. A custom deployment can point the receiver at another origin with the `PRINTDROP_BASE_URL` environment variable.

## Relay contract

The public origin must expose all of these paths through the same service:

- `GET /s/<session-id>` - browser sender page opened by the QR code.
- `POST /v1/sessions` - authenticated receiver session registration.
- `GET /v1/receiver/<session-id>` - authenticated receiver WebSocket upgrade.
- `GET /v1/sender/<session-id>` - browser sender WebSocket upgrade.
- `GET /healthz` - process health check.

The relay never stores transferred files. It keeps only short-lived in-memory session state and forwards bounded binary WebSocket messages between one sender and one receiver.

## Build the container

No Git submodules are required. The browser sender, including the incremental SHA-256 implementation, is tracked directly in this repository.

```bash
docker build -f relay/Dockerfile -t printdrop-relay .
```

Run the service on an unprivileged local port:

```bash
docker run --rm -p 127.0.0.1:8080:8080 printdrop-relay
```

The image contains both the Go relay binary and the browser sender assets. `PRINTDROP_WEB_DIR` defaults to `/web` in the image.

For a source checkout instead of the container:

```bash
cd relay
go run .
```

The source-mode default sender directory is `../web`. It can be overridden with `PRINTDROP_WEB_DIR`.

## Local phone-to-Windows E2E test

Production remains HTTPS/WSS-only by default. For development on a trusted LAN, the Windows receiver supports an explicit insecure opt-in so the real phone -> relay -> Windows flow can be exercised without provisioning a certificate.

This mode sends the receiver credential and file traffic in plaintext on the LAN. Use it only on a trusted development network, never on public Wi-Fi or an Internet-facing relay.

1. Put the Windows PC and phone on the same LAN and find the PC's LAN IPv4 address, for example `192.168.1.42`.
2. Start the relay so it is reachable from the LAN. From a source checkout:

```powershell
cd relay
$env:PRINTDROP_RELAY_ADDR = "0.0.0.0:8080"
go run .
```

3. From the phone, open `http://<PC-LAN-IP>:8080/healthz`. It must display `ok`. If it cannot connect, check the Windows firewall/network profile before testing PrintDrop.
4. In a second PowerShell window on the Windows PC, opt in to insecure development transport and launch the receiver:

```powershell
$env:PRINTDROP_ALLOW_INSECURE_HTTP = "1"
$env:PRINTDROP_BASE_URL = "http://192.168.1.42:8080"
.\build\Debug\printdrop.exe
```

Replace the example IP with the PC's actual LAN address. The generated QR will use the HTTP LAN origin, receiver registration will use HTTP, and the receiver WebSocket will use `ws://`.

5. Scan the QR from the phone, choose a print-friendly file, and send it. A successful file appears under the absolute receive directory shown by the Windows app.
6. Close the receiver and clear the development overrides when finished:

```powershell
Remove-Item Env:PRINTDROP_ALLOW_INSECURE_HTTP -ErrorAction SilentlyContinue
Remove-Item Env:PRINTDROP_BASE_URL -ErrorAction SilentlyContinue
Remove-Item Env:PRINTDROP_RELAY_ADDR -ErrorAction SilentlyContinue
```

If `PRINTDROP_ALLOW_INSECURE_HTTP` is absent or is anything other than exactly `1`, native registration and `ws://` receiver transport remain rejected.

## TLS is required in production

The relay process deliberately listens with plain HTTP so TLS can be terminated by a reverse proxy. The public endpoint used by the Windows receiver must be HTTPS/WSS unless the explicit insecure development opt-in above is active.

A minimal Caddy deployment is:

```caddyfile
send.printdrop.app {
    reverse_proxy 127.0.0.1:8080
}
```

The reverse proxy must preserve WebSocket upgrades for `/v1/receiver/*` and `/v1/sender/*`.

Do not expose the relay's plain HTTP port directly to the internet. Receiver credentials are sent only to the HTTPS registration and WSS receiver endpoints in normal operation.

## Health check

```bash
curl -fsS http://127.0.0.1:8080/healthz
```

Expected response:

```text
ok
```

## Custom public origin

Set the same public origin on the Windows machine before starting PrintDrop:

```powershell
$env:PRINTDROP_BASE_URL = "https://drop.example.com"
.\PrintDrop.exe
```

The QR code will then use `https://drop.example.com/s/<session-id>`, while the native receiver derives its registration and WSS endpoints from that same origin.

## MVP infrastructure gate

Before calling a deployment usable, verify from a separate phone/network that:

1. `/healthz` is reachable over the public HTTPS origin.
2. opening a generated `/s/<session-id>` URL loads the sender UI.
3. the browser can upgrade `/v1/sender/<session-id>` to WSS after the native receiver is connected.
4. a transferred file appears under the receive directory displayed by the Windows receiver.
5. no `.part` staging file remains after a successful transfer or an intentionally failed checksum test.
