# Third-party dependencies

## Project Nayuki QR-Code-generator

PrintDrop uses the C implementation of Project Nayuki's QR Code generator for converting short-lived receiver URLs into QR module matrices.

- Upstream: `nayuki/QR-Code-generator`
- Pinned commit: `2c9044de6b049ca25cb3cd1649ed7e27aa055138`
- License: MIT
- Integration: build-time CMake FetchContent; no runtime service or framework dependency

Release packaging must include the upstream MIT license notice alongside distributed binaries that incorporate the library.

## curl / libcurl

The Windows receiver uses libcurl for the outbound HTTPS/WebSocket relay transport.

- Upstream: `curl/curl`
- Release: `8.21.0`
- Pinned commit: `68720b4837284335b2d63cb358f8f6ce65f5bc55`
- TLS backend on Windows: Schannel
- Linkage: static libcurl and static MSVC runtime
- Target baseline: `_WIN32_WINNT=0x0601` / Windows 7
- Unneeded legacy application protocols and optional compression/SSH dependencies are disabled in the embedded build.

Release packaging must preserve curl's copyright/license notice.
