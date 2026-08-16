# Contributing to PrintDrop

## Branch discipline

`main` represents integrated, reviewable work. Normal changes should land through a pull request.

Recommended branch prefixes:

- `feature/` - product capability
- `fix/` - bug fix
- `test/` - test-only work
- `infra/` - build, CI, release, tooling
- `docs/` - documentation-only work
- `agent/` - automated coding-agent work

The repository bootstrap commit is the unavoidable exception because an empty repository has no branch target from which to open the first pull request.

## Definition of done

For code changes:

- add/update tests for changed logic;
- run the relevant local build/tests;
- keep compiler warnings clean;
- do not weaken CI to make a change green;
- document meaningful architecture changes;
- keep platform-specific behavior out of the portable core unless necessary.

## CI contract

Every push and pull request runs:

- GCC core build + tests;
- Clang core build + tests;
- Clang AddressSanitizer + UndefinedBehaviorSanitizer build/tests;
- MSVC Win32 build + tests;
- MSVC x64 build + tests.

Warnings are errors in CI.

A PR with failing required behavior should not be merged.

## C style

- C11 is the language baseline.
- Prefer small modules and explicit ownership.
- Public symbols use the `pd_` prefix.
- Public types use the `pd_` prefix.
- Avoid global mutable state.
- Validate sizes before arithmetic or allocation.
- Handle partial I/O explicitly when networking arrives.
- Do not implement custom crypto, TLS, HTTP, or QR algorithms when a mature small library is appropriate.
- Keep dependency count low and pin/review dependencies before introducing them.

## Tests

Core behavior should be testable without a GUI, network connection, or Windows machine. Platform integration and real network E2E tests will be layered on top rather than replacing unit tests.