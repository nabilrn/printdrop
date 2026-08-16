# Third-party dependencies

## Project Nayuki QR-Code-generator

PrintDrop uses the C implementation of Project Nayuki's QR Code generator for converting short-lived receiver URLs into QR module matrices.

- Upstream: `nayuki/QR-Code-generator`
- Pinned commit: `2c9044de6b049ca25cb3cd1649ed7e27aa055138`
- License: MIT
- Integration: build-time CMake FetchContent; no runtime service or framework dependency

Release packaging must include the upstream MIT license notice alongside distributed binaries that incorporate the library.
