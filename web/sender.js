import {
  AckStatus,
  FILE_CHUNK_SIZE,
  MessageType,
  decodeAck,
  encodeFileBegin,
  encodeFrame,
} from './protocol.js';

export const DEFAULT_ACK_TIMEOUT_MS = 30_000;
export const DEFAULT_CONNECT_TIMEOUT_MS = 10_000;
export const HASH_CHUNK_SIZE = 1024 * 1024;

const SESSION_PATTERN = /^\/s\/([0-9a-f]{32})\/?$/;

export function parseSessionId(pathname) {
  if (typeof pathname !== 'string') {
    return null;
  }
  const match = SESSION_PATTERN.exec(pathname);
  return match ? match[1] : null;
}

export function buildSenderWebSocketUrl(locationLike, sessionId) {
  if (!locationLike || typeof locationLike.protocol !== 'string' ||
      typeof locationLike.host !== 'string' || !/^[0-9a-f]{32}$/.test(sessionId)) {
    throw new TypeError('invalid sender WebSocket URL input');
  }
  let scheme;
  if (locationLike.protocol === 'https:') {
    scheme = 'wss:';
  } else if (locationLike.protocol === 'http:') {
    scheme = 'ws:';
  } else {
    throw new Error('PrintDrop sender requires HTTP or HTTPS');
  }
  return `${scheme}//${locationLike.host}/v1/sender/${sessionId}`;
}

function requireFileLike(file) {
  if (!file || typeof file.name !== 'string' || !Number.isSafeInteger(file.size) ||
      file.size < 0 || typeof file.slice !== 'function') {
    throw new TypeError('invalid file');
  }
}

export async function hashFileIncremental(file, hasherFactory, onProgress = null,
                                          chunkSize = HASH_CHUNK_SIZE) {
  requireFileLike(file);
  if (typeof hasherFactory !== 'function' || !Number.isSafeInteger(chunkSize) || chunkSize <= 0) {
    throw new TypeError('invalid hash configuration');
  }
  const hasher = hasherFactory();
  if (!hasher || typeof hasher.update !== 'function' || typeof hasher.array !== 'function') {
    throw new TypeError('invalid SHA-256 hasher');
  }

  let processed = 0;
  while (processed < file.size) {
    const end = Math.min(processed + chunkSize, file.size);
    const bytes = new Uint8Array(await file.slice(processed, end).arrayBuffer());
    hasher.update(bytes);
    processed = end;
    if (onProgress) {
      onProgress(processed, file.size);
    }
  }
  if (file.size === 0 && onProgress) {
    onProgress(0, 0);
  }

  const digest = new Uint8Array(hasher.array());
  if (digest.byteLength !== 32) {
    throw new Error('SHA-256 implementation returned an invalid digest');
  }
  return digest;
}

function withTimeout(setup, timeoutMs, timeoutMessage) {
  return new Promise((resolve, reject) => {
    let settled = false;
    const finish = (callback, value) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      cleanup();
      callback(value);
    };
    const handlers = setup(
      (value) => finish(resolve, value),
      (error) => finish(reject, error instanceof Error ? error : new Error(String(error))),
    );
    const cleanup = handlers.cleanup;
    const timer = setTimeout(() => finish(reject, new Error(timeoutMessage)), timeoutMs);
  });
}

export function waitForSocketOpen(socket, timeoutMs = DEFAULT_CONNECT_TIMEOUT_MS) {
  if (!socket || typeof socket.addEventListener !== 'function') {
    return Promise.reject(new TypeError('invalid WebSocket'));
  }
  if (socket.readyState === 1) {
    return Promise.resolve();
  }
  return withTimeout((resolve, reject) => {
    const onOpen = () => resolve();
    const onError = () => reject(new Error('Could not connect to this PrintDrop receiver'));
    const onClose = () => reject(new Error('Receiver is unavailable or the session expired'));
    socket.addEventListener('open', onOpen);
    socket.addEventListener('error', onError);
    socket.addEventListener('close', onClose);
    return {
      cleanup() {
        socket.removeEventListener('open', onOpen);
        socket.removeEventListener('error', onError);
        socket.removeEventListener('close', onClose);
      },
    };
  }, timeoutMs, 'Timed out connecting to the PrintDrop receiver');
}

async function messageBytes(data) {
  if (data instanceof Uint8Array) {
    return data;
  }
  if (data instanceof ArrayBuffer) {
    return new Uint8Array(data);
  }
  if (typeof Blob !== 'undefined' && data instanceof Blob) {
    return new Uint8Array(await data.arrayBuffer());
  }
  throw new Error('Receiver sent a non-binary WebSocket message');
}

function waitForAck(socket, expectedType, timeoutMs) {
  return withTimeout((resolve, reject) => {
    const onMessage = async (event) => {
      try {
        const ack = decodeAck(await messageBytes(event.data));
        if (ack.acknowledgedType !== expectedType) {
          reject(new Error('Receiver ACK does not match the sent frame'));
          return;
        }
        if (ack.status !== AckStatus.OK) {
          reject(new Error(`Receiver rejected transfer with status ${ack.status}`));
          return;
        }
        resolve(ack);
      } catch (error) {
        reject(error);
      }
    };
    const onError = () => reject(new Error('Network error while waiting for receiver ACK'));
    const onClose = () => reject(new Error('Receiver disconnected during transfer'));
    socket.addEventListener('message', onMessage);
    socket.addEventListener('error', onError);
    socket.addEventListener('close', onClose);
    return {
      cleanup() {
        socket.removeEventListener('message', onMessage);
        socket.removeEventListener('error', onError);
        socket.removeEventListener('close', onClose);
      },
    };
  }, timeoutMs, 'Timed out waiting for receiver ACK');
}

async function sendFrameAndWait(socket, type, payload, expectedReceivedBytes, timeoutMs) {
  if (socket.readyState !== 1) {
    throw new Error('WebSocket is not connected');
  }
  const ackPromise = waitForAck(socket, type, timeoutMs);
  socket.send(encodeFrame(type, payload));
  const ack = await ackPromise;
  if (ack.receivedBytes !== expectedReceivedBytes) {
    throw new Error('Receiver byte count does not match sender progress');
  }
  return ack;
}

export async function sendFile({
  file,
  socket,
  hasherFactory,
  onHashProgress = null,
  onUploadProgress = null,
  ackTimeoutMs = DEFAULT_ACK_TIMEOUT_MS,
}) {
  requireFileLike(file);
  if (!socket || socket.readyState !== 1) {
    throw new Error('WebSocket is not connected');
  }

  const digest = await hashFileIncremental(file, hasherFactory, onHashProgress);
  const fileSize = BigInt(file.size);
  const beginPayload = encodeFileBegin({
    fileSize,
    sha256: digest,
    filename: file.name,
  });
  await sendFrameAndWait(socket, MessageType.FILE_BEGIN, beginPayload, 0n, ackTimeoutMs);

  let offset = 0;
  while (offset < file.size) {
    const end = Math.min(offset + FILE_CHUNK_SIZE, file.size);
    const chunk = new Uint8Array(await file.slice(offset, end).arrayBuffer());
    await sendFrameAndWait(socket,
                           MessageType.CHUNK,
                           chunk,
                           BigInt(end),
                           ackTimeoutMs);
    offset = end;
    if (onUploadProgress) {
      onUploadProgress(offset, file.size);
    }
  }

  await sendFrameAndWait(socket,
                         MessageType.FILE_END,
                         new Uint8Array(0),
                         fileSize,
                         ackTimeoutMs);
  if (file.size === 0 && onUploadProgress) {
    onUploadProgress(0, 0);
  }
}
