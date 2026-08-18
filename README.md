# PrintDrop

> **Status: Experimental / archived as a product experiment.**
>
> PrintDrop proved the core technical path end to end, but further product development is intentionally paused. The project is kept as an engineering case study rather than positioned as a production-ready print-shop product.

PrintDrop is a lightweight file-transfer experiment for receiving files on Windows PCs from a phone browser through short-lived QR sessions.

The original product idea was simple:

**No login. No WhatsApp. No cable. Scan, send, print.**

The repository explores whether that experience can work on old print-shop PCs without Electron, a browser runtime, or a heavy local stack.

## What was proven

The experiment reached a real end-to-end working state:

- native C11 + Win32 receiver;
- QR-based short-lived receive sessions;
- plain HTML/CSS/JavaScript sender for Android and iPhone browsers;
- WebSocket transfer with bounded chunks and ACK-driven flow control;
- incremental SHA-256 verification;
- staged writes and atomic file completion under `Documents\PrintDrop`;
- public relay mode with no cloud file storage;
- explicit trusted-LAN mode for direct local testing;
- automatic session rotation after a completed transfer;
- x86 and x64 Windows package builds;
- portable core CI on GCC, Clang, ASan, and UBSan;
- Windows CI on MSVC Win32 and x64;
- physical phone-to-Windows end-to-end transfer successfully tested.

## Architecture explored

```text
Phone browser
     |
     | QR session
     v
+-------------------+
| Browser sender    |
| HTML / CSS / JS   |
+-------------------+
     |
     | WebSocket frames
     v
+-------------------+       optional public path
| Go relay          |  <-------------------------->
| no file storage   |
+-------------------+
     |
     v
+-------------------+
| PrintDrop.exe     |
| native Win32/C11  |
+-------------------+
     |
     v
Documents\PrintDrop
```

A trusted-LAN mode was also added so the sender and receiver can communicate over the shop network without requiring a public tunnel during local qualification.

## Why product development stopped here

The technical problem turned out to be easier than the adoption problem.

The intended environment has several real-world constraints:

- many print shops still use old or low-spec Windows PCs;
- PCs may be Ethernet-only while customer phones are on Wi-Fi or mobile data;
- printers may support Wi-Fi but shops often do not configure or use those capabilities;
- customers and operators are already highly accustomed to WhatsApp for sending print files;
- replacing that familiar workflow would require PrintDrop to deliver significantly more convenience than file transfer alone;
- seamless network onboarding across arbitrary routers, old PCs, Android, iOS, browser security policies, and customer Wi-Fi behavior adds disproportionate complexity.

The experiment therefore reached an important product conclusion:

> A technically cleaner file-transfer path is not enough by itself to displace an established workflow such as WhatsApp.

A stronger future product would likely need to solve the **print-order workflow** itself — queueing, print settings, multiple files, job identity, cleanup, and operator workflow — rather than compete only on transport.

## Intentionally not pursued

The following ideas were discussed but are outside the completed experiment:

- multi-file customer sessions;
- explicit `SESSION_END` / batch job lifecycle;
- automatic Wi-Fi credential onboarding;
- customer hotspot orchestration;
- persistent `device_id` transport;
- persistent device WebSocket + rotating customer sessions;
- WebRTC / NAT traversal;
- production-scale relay infrastructure;
- print-order queue and structured print settings;
- polished production desktop UI;
- formal Windows 7 runtime qualification.

These are not listed as missing MVP work. They are deliberately left as possible follow-up research directions.

## Engineering constraints

The implementation was intentionally conservative:

- C11 portable core with a native Win32 receiver;
- Windows 7 SP1 through Windows 11 as the design compatibility target;
- x86 and x64 builds;
- no Electron;
- no embedded browser engine;
- no Node.js runtime on the receiver PC;
- no Docker or local database requirement on the receiver PC;
- plain web sender with zero build step;
- transport-independent protocol boundaries;
- warnings treated as errors;
- CI-backed logic changes.

> Windows 7 remains a design/release target rather than a proven runtime compatibility claim. The project was physically qualified on a modern Windows environment, while hosted CI compile-gates Win32/x64 builds.

## Windows packages

The `Windows Packages` workflow produces Release-mode Win32 and x64 artifacts containing:

- `PrintDrop.exe`;
- `SHA256SUMS.txt`;
- a short package README with runtime/source information.

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
docker build -f relay/Dockerfile -t printdrop-relay .
docker run --rm -p 127.0.0.1:8080:8080 printdrop-relay
```

For deployment and trusted-LAN qualification details, see `docs/deployment.md`.

## Repository map

```text
include/printdrop/   Public core interfaces
src/                 Portable core + Windows receiver
relay/               Short-lived authenticated relay service
web/                 Zero-build customer browser sender
tests/               Native unit/integration tests
docs/                Architecture, deployment, and engineering contracts
.github/workflows/    CI and Windows packaging gates
```

## Takeaway

PrintDrop succeeded as an engineering experiment: it demonstrated a small native Windows receiver, browser sender, QR session model, relay/local transport paths, integrity verification, packaging, and cross-platform CI under constraints inspired by real print-shop PCs.

It is intentionally paused here so the repository remains a clear record of the experiment instead of accumulating product complexity without sufficient user-value evidence.
