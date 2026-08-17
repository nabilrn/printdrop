# PrintDrop

PrintDrop is a lightweight, local-first file receiver for print shops.

A shop operator should be able to launch one small Windows application, show a QR code, and receive files from any modern Android or iPhone browser without asking the customer to log in to WhatsApp, save a phone number, install a sender app, or plug in a cable.

## Product goal

**No login. No WhatsApp. No cable. Scan, send, print.**

PrintDrop is intentionally receiver-first:

1. `PrintDrop.exe` runs locally on the print-shop PC.
2. It creates a short-lived receive session and displays a QR code.
3. The customer scans the QR code and opens a tiny HTML/CSS/JavaScript sender.
4. The sender streams the file through the public relay with bounded, ACK-driven chunks.
5. The native receiver validates SHA-256 and atomically writes the completed file under `Documents\PrintDrop`.

The V0.1 transport is an internet relay because it works across old routers, CGNAT, Ethernet-only shop PCs, Android, and iOS. Direct LAN and native peer-to-peer transports can be added later behind the same protocol boundary.

## Engineering constraints

- Native C11 core and Win32 receiver.
- Windows 7 SP1 through Windows 11 is the compatibility target.
- x86 and x64 builds.
- No Electron, browser engine, Node.js runtime, Docker, or local database requirement on the print-shop PC.
- Keep the customer sender to plain HTML, CSS, and JavaScript.
- Transport-independent protocol and transfer state machine.
- Logic changes require tests.
- Every push and pull request runs CI.
- Warnings are treated as errors.
- Linux CI exercises the portable core with GCC, Clang, ASan, and UBSan.
- Windows CI compiles and tests Win32 and x64 builds with MSVC.

> Windows 7 is a release target, not yet a proven compatibility claim. Hosted CI compile-gates Win32/x64; real Windows 7 execution qualification remains a release gate before the first beta.

## Current status

The end-to-end V0.1 receive path is implemented in the repository:

- short-lived receiver sessions and QR URLs;
- browser sender with incremental SHA-256 and bounded chunking;
- authenticated HTTPS receiver registration;
- bounded WSS relay transport with no cloud file storage;
- native Win32 receive runtime on a worker thread;
- staged local writes, SHA-256 verification, and atomic completion;
- operator-visible ready, receiving, verifying, complete, and failed states;
- relay deployment that serves the QR sender page and API/WebSocket endpoints from one public origin.

Hosted CI remains the source-of-truth build gate. Public relay deployment, real phone-to-Windows transfer qualification, and real Windows 7 execution are deployment/release gates rather than missing protocol features.

## Build

### Windows

```powershell
cmake -S . -B build -A x64 -DPD_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

For a 32-bit build, replace `-A x64` with `-A Win32`.

### Linux core tests

```bash
cmake -S . -B build -DPD_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

### Relay

```bash
git submodule update --init --recursive
docker build -f relay/Dockerfile -t printdrop-relay .
docker run --rm -p 127.0.0.1:8080:8080 printdrop-relay
```

Production relay traffic must be exposed through an HTTPS/WSS reverse proxy. See `docs/deployment.md` for the deployment contract and qualification gates.

## Repository map

```text
include/printdrop/   Public core interfaces
src/                 Portable core + Windows receiver
relay/               Short-lived authenticated relay service
web/                 Zero-build customer browser sender
tests/               Native unit/integration tests
docs/                Architecture, deployment, and engineering contracts
.github/workflows/    CI gates
```

See `docs/engineering-goals.md`, `docs/architecture.md`, `docs/deployment.md`, and `CONTRIBUTING.md` before implementing a feature.
