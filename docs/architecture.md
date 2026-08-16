# PrintDrop Architecture

## Principle

PrintDrop is a native Windows receiver with a universal browser sender. The browser is an input surface, not the application core.

```text
Phone browser
HTML/CSS/JS
     |
     | session + file chunks
     v
Transport
     |
     v
PrintDrop protocol
     |
     v
Native Windows receiver
     |
     v
Local filesystem
```

## Module boundaries

### `protocol`

Defines transport-independent message vocabulary and, later, binary/wire encoding. It must not call Win32, libcurl, sockets, UI code, or filesystem APIs.

Initial vocabulary:

- `HELLO`
- `JOB`
- `FILE_BEGIN`
- `CHUNK`
- `FILE_END`
- `ACK`
- `ERROR`

### `session`

Owns transfer state and legal state transitions. The module must remain deterministic and easy to unit test.

```text
WAITING -> RECEIVING -> VERIFYING -> COMPLETE
   |           |            |
   +-----------+------------+-> FAILED
```

### platform layer

Win32-specific responsibilities will include:

- application/window lifecycle;
- local directories;
- file handles;
- high-resolution timing;
- worker-thread/event integration;
- platform networking bootstrap;
- compatibility shims where required.

### transport layer

A future C interface will isolate network transports from protocol/session logic.

Conceptually:

```c
typedef struct pd_transport_vtable {
    int (*connect)(void *context);
    int (*send)(void *context, const void *data, size_t length);
    int (*receive)(void *context, void *buffer, size_t capacity, size_t *received);
    void (*close)(void *context);
} pd_transport_vtable;
```

This interface is illustrative; it is intentionally not committed to the public headers until the first relay implementation exposes the real requirements.

## Planned V0.1 data path

```text
Customer phone
      |
      | HTTPS
      v
PrintDrop relay
      |
      | outbound receiver connection
      v
PrintDrop.exe
      |
      | stream/chunks
      v
Temporary job file
      |
      | verify + atomic completion
      v
Final local file
```

The relay should act as a rendezvous/streaming path rather than permanent file storage.

## Memory model

Large files must not be buffered in full. Transfer implementations should operate on bounded chunks and write incrementally to disk.

## Security model direction

Before public beta the receive path must include:

- cryptographically random, short-lived session capability/token;
- strict session expiry;
- maximum file and chunk bounds;
- filename sanitization and path traversal prevention;
- authenticated transport encryption;
- integrity verification before marking a job complete;
- no automatic execution/opening of received files;
- deterministic cleanup for incomplete transfers.

No custom cryptographic primitive will be implemented by PrintDrop.

## Compatibility strategy

The portable core intentionally has no Windows dependency. Win32 code is compiled only for Windows. This lets CI use Linux compilers and sanitizers against protocol/session logic while Windows jobs validate the actual application target.

Runtime support for Windows 7 remains a separate qualification task because hosted GitHub runners do not prove execution on Windows 7.