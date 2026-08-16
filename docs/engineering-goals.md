# PrintDrop Engineering Goals

## Product thesis

File handoff at small print shops is unnecessarily account-centric. Customers commonly send documents through personal messaging accounts, which creates friction and privacy risk. PrintDrop replaces that handoff with a local receiver and a disposable QR session.

The operator experience is the primary design constraint:

> Launch PrintDrop, show the QR code, receive the file.

No account setup, router configuration, inbound port forwarding, container runtime, or local server administration should be required from the print-shop operator.

## V0.1 success criteria

V0.1 is successful when all of the following are true:

1. A Windows receiver launches as a native application.
2. A receive session can be represented by a short-lived QR/URL.
3. Android and iOS users can use a browser sender without installing an app.
4. A file is delivered to a deterministic local job directory.
5. Transfer progress and terminal success/failure are visible to the operator.
6. Unexpected disconnects do not corrupt a completed job.
7. Protocol and state-machine behavior are covered by automated tests.
8. x86 and x64 Windows builds stay green in CI.
9. The portable core stays green under GCC, Clang, AddressSanitizer, and UndefinedBehaviorSanitizer.

## Compatibility target

- Windows 7 SP1, 8.1, 10, and 11.
- x86 and x64.
- Old or low-spec shop PCs are first-class users.

Windows 7 execution must eventually be tested on a real or virtual Windows 7 environment. Merely defining `_WIN32_WINNT=0x0601` and compiling successfully is not sufficient evidence of runtime compatibility.

## Systems-engineering goals

PrintDrop should be a serious native-systems project rather than a desktop wrapper around a web stack. The codebase should demonstrate:

- explicit ownership and lifetime discipline in C;
- narrow, stable module APIs;
- Win32 application and filesystem integration;
- network protocol design;
- chunked/resumable transfer state machines;
- transport abstraction;
- defensive parsing of untrusted network input;
- bounded memory use while transferring large files;
- concurrency without blocking the UI thread;
- deterministic error handling and cleanup;
- testable platform-independent core logic;
- profiling and performance measurement before optimization;
- compatibility engineering across old and modern Windows releases.

## Architecture direction

The protocol must not be coupled to one transport.

```text
                   PrintDrop protocol/core
                            |
                      pd_transport
                            |
          +-----------------+-----------------+
          |                 |                 |
       Relay             Local LAN         Native P2P
       V0.1                Later              Later
```

V0.1 uses the relay path for universal compatibility. Later transports must reuse the same job/session semantics instead of creating parallel product logic.

## Explicit V0.1 non-goals

- Controlling printers or automatically printing documents.
- Customer accounts.
- Shop SaaS dashboards.
- Permanent cloud file storage.
- Native Android or iOS applications.
- Wi-Fi Direct / Wi-Fi Aware.
- Billing and payment.
- OCR and document conversion.
- Complex job history/database.

These are intentionally excluded until the basic receive path is proven reliable.

## Quality bar

A change is not complete because it compiles on one developer machine.

For core logic, completion means:

1. the behavior is implemented behind a narrow API;
2. relevant tests exist or are updated;
3. warnings remain clean;
4. Linux GCC and Clang tests pass;
5. sanitizer tests pass;
6. Windows x86 and x64 compile/tests pass;
7. failure paths are considered explicitly.

Network-facing code will additionally require malformed-input and boundary tests before beta.