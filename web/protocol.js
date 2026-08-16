export const PROTOCOL_VERSION = 1;
export const FRAME_HEADER_SIZE = 12;
export const RELAY_WS_MESSAGE_MAX_SIZE = 60 * 1024;
export const RELAY_MAX_PAYLOAD = RELAY_WS_MESSAGE_MAX_SIZE - FRAME_HEADER_SIZE;
export const FILE_CHUNK_SIZE = 48 * 1024;
export const FILE_BEGIN_FIXED_SIZE = 42;
export const FILENAME_MAX_BYTES = 120;
export const SHA256_BYTES = 32;
export const ACK_PAYLOAD_SIZE = 12;

export const MessageType = Object.freeze({
  HELLO: 1,
  JOB: 2,
  FILE_BEGIN: 3,
  CHUNK: 4,
  FILE_END: 5,
  ACK: 6,
  ERROR: 7,
});

export const AckStatus = Object.freeze({
  OK: 0,
  PROTOCOL_ERROR: 1,
  STORAGE_ERROR: 2,
  INTEGRITY_ERROR: 3,
  REJECTED: 4,
});

const MAGIC = new Uint8Array([0x50, 0x44, 0x52, 0x50]);
const textEncoder = new TextEncoder();

function requireUint8Array(value, name) {
  if (!(value instanceof Uint8Array)) {
    throw new TypeError(`${name} must be a Uint8Array`);
  }
}

function requireMessageType(type) {
  if (!Number.isInteger(type) || type < MessageType.HELLO || type > MessageType.ERROR) {
    throw new RangeError("invalid PrintDrop message type");
  }
}

function requireUint64(value, name) {
  if (typeof value !== "bigint" || value < 0n || value > 0xffffffffffffffffn) {
    throw new RangeError(`${name} must be an unsigned 64-bit bigint`);
  }
}

export function encodeFrame(type, payload = new Uint8Array(0)) {
  requireMessageType(type);
  requireUint8Array(payload, "payload");
  if (payload.byteLength > RELAY_MAX_PAYLOAD) {
    throw new RangeError("payload exceeds PrintDrop relay message limit");
  }

  const message = new Uint8Array(FRAME_HEADER_SIZE + payload.byteLength);
  message.set(MAGIC, 0);
  message[4] = PROTOCOL_VERSION;
  message[5] = type;
  const view = new DataView(message.buffer, message.byteOffset, message.byteLength);
  view.setUint16(6, 0, false);
  view.setUint32(8, payload.byteLength, false);
  message.set(payload, FRAME_HEADER_SIZE);
  return message;
}

export function decodeFrame(message) {
  requireUint8Array(message, "message");
  if (message.byteLength < FRAME_HEADER_SIZE || message.byteLength > RELAY_WS_MESSAGE_MAX_SIZE) {
    throw new RangeError("invalid PrintDrop relay message size");
  }
  for (let index = 0; index < MAGIC.length; index += 1) {
    if (message[index] !== MAGIC[index]) {
      throw new Error("invalid PrintDrop frame magic");
    }
  }
  if (message[4] !== PROTOCOL_VERSION) {
    throw new Error("unsupported PrintDrop protocol version");
  }
  requireMessageType(message[5]);

  const view = new DataView(message.buffer, message.byteOffset, message.byteLength);
  if (view.getUint16(6, false) !== 0) {
    throw new Error("unsupported PrintDrop frame flags");
  }
  const payloadLength = view.getUint32(8, false);
  if (payloadLength > RELAY_MAX_PAYLOAD || FRAME_HEADER_SIZE + payloadLength !== message.byteLength) {
    throw new Error("invalid PrintDrop frame payload length");
  }

  return {
    type: message[5],
    payload: message.subarray(FRAME_HEADER_SIZE),
  };
}

export function encodeFileBegin({ fileSize, sha256, filename }) {
  requireUint64(fileSize, "fileSize");
  requireUint8Array(sha256, "sha256");
  if (sha256.byteLength !== SHA256_BYTES) {
    throw new RangeError("sha256 must contain exactly 32 bytes");
  }
  if (typeof filename !== "string" || filename.length === 0 || filename.includes("\0")) {
    throw new TypeError("filename must be a non-empty string without NUL bytes");
  }

  const filenameBytes = textEncoder.encode(filename);
  if (filenameBytes.byteLength === 0 || filenameBytes.byteLength > FILENAME_MAX_BYTES) {
    throw new RangeError("filename exceeds PrintDrop UTF-8 byte limit");
  }

  const payload = new Uint8Array(FILE_BEGIN_FIXED_SIZE + filenameBytes.byteLength);
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  view.setBigUint64(0, fileSize, false);
  payload.set(sha256, 8);
  view.setUint16(40, filenameBytes.byteLength, false);
  payload.set(filenameBytes, FILE_BEGIN_FIXED_SIZE);
  return payload;
}

export function decodeAck(message) {
  const frame = decodeFrame(message);
  if (frame.type !== MessageType.ACK || frame.payload.byteLength !== ACK_PAYLOAD_SIZE) {
    throw new Error("expected PrintDrop ACK frame");
  }

  const payload = frame.payload;
  const acknowledgedType = payload[0];
  if (![MessageType.FILE_BEGIN, MessageType.CHUNK, MessageType.FILE_END].includes(acknowledgedType)) {
    throw new Error("ACK references an unsupported message type");
  }
  const status = payload[1];
  if (!Number.isInteger(status) || status < AckStatus.OK || status > AckStatus.REJECTED) {
    throw new Error("invalid PrintDrop ACK status");
  }
  if (payload[2] !== 0 || payload[3] !== 0) {
    throw new Error("invalid PrintDrop ACK reserved bytes");
  }

  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  return {
    acknowledgedType,
    status,
    receivedBytes: view.getBigUint64(4, false),
  };
}
