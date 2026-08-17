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

Initialize repository submodules first because the browser SHA-256 implementation is pinned as a Git submodule.

```bash
git submodule update --init --recursive
docker build -f relay/Dockerfile -t printdrop-relay .
```

Run the service on an unprivileged local port:

```bash
docker run --rm -p 127.0.0.1:8080:8080 printdrop-relay
```

The image contains both the Go relay binary and the audited browser sender assets. `PRINTDROP_WEB_DIR` defaults to `/web` in the image.

For a source checkout instead of the container:

```bash
cd relay
go run .
```

The source-mode default sender directory is `../web`. It can be overridden with `PRINTDROP_WEB_DIR`.

## TLS is required in production

The relay process deliberately listens with plain HTTP so TLS can be terminated by a reverse proxy. The public endpoint used by the Windows receiver must be HTTPS/WSS.

A minimal Caddy deployment is:

```caddyfile
send.printdrop.app {
    reverse_proxy 127.0.0.1:8080
}
```

The reverse proxy must preserve WebSocket upgrades for `/v1/receiver/*` and `/v1/sender/*`.

Do not expose the relay's plain HTTP port directly to the internet. Receiver credentials are sent only to the HTTPS registration and WSS receiver endpoints.

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
4. a transferred file appears under `Documents\PrintDrop` on the Windows receiver.
5. no `.part` staging file remains after a successful transfer or an intentionally failed checksum test.
