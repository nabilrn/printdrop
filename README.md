# PrintDrop

PrintDrop is a lightweight, local-first file receiver for print shops.

A shop operator should be able to launch one small Windows application, show a QR code, and receive files from any modern Android or iPhone browser without asking the customer to log in to WhatsApp, save a phone number, install a sender app, or plug in a cable.

## Product goal

**No login. No WhatsApp. No cable. Scan, send, print.**

PrintDrop is intentionally receiver-first:

1. `PrintDrop.exe` runs locally on the print-shop PC.
2. It creates a short-lived receive session and displays a QR code.
3. The customer scans the QR code and opens a tiny HTML/CSS/JavaScript sender.
4. The sender transfers the file through the selected transport.
5. The native receiver validates and writes the file to the local machine.

The first production transport will be an internet relay because it works across old routers, CGNAT, Ethernet-only shop PCs, Android, and iOS. Direct LAN and native peer-to-peer transports can be added later behind the same protocol boundary.

## Engineering constraints

- Native C11 core and Win32 receiver.
- Windows 7 SP1 through Windows 11 is the compatibility target.
- x86 and x64 builds.
- No Electron, browser engine, Node.js runtime, Docker, or local database requirement.
- Keep the customer sender to plain HTML, CSS, and JavaScript.
- Transport-independent protocol and transfer state machine.
- Logic changes require tests.
- Every push and pull request runs CI.
- Warnings are treated as errors.
- Linux CI exercises the portable core with GCC, Clang, ASan, and UBSan.
- Windows CI compiles and tests Win32 and x64 builds with MSVC.

> Windows 7 is a release target, not yet a proven compatibility claim. Hosted CI compile-gates Win32/x64; real Windows 7 execution qualification will be added as a release gate before the first beta.

## Current status

The repository is in foundation/bootstrap phase. The current code establishes the protocol vocabulary, transfer state machine, native Win32 shell, test harness, and CI contract. Networking and QR receiving are intentionally deferred to focused follow-up milestones.

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

## Repository map

```text
include/printdrop/   Public core interfaces
src/                 Portable core + Windows platform shell
tests/               Unit tests
web/                 Minimal customer sender shell
docs/                Architecture and engineering contracts
.github/workflows/    CI gates
```

See `docs/engineering-goals.md`, `docs/architecture.md`, and `CONTRIBUTING.md` before implementing a feature.