import assert from "node:assert/strict";
import test from "node:test";

import {
  AckStatus,
  FILE_BEGIN_FIXED_SIZE,
  FILE_CHUNK_SIZE,
  FRAME_HEADER_SIZE,
  MessageType,
  RELAY_MAX_PAYLOAD,
  RELAY_WS_MESSAGE_MAX_SIZE,
  decodeAck,
  decodeFrame,
  encodeFileBegin,
  encodeFrame,
} from "./protocol.js";

test("frame bytes match the C wire header", () => {
  const payload = new Uint8Array([1, 2, 3]);
  const message = encodeFrame(MessageType.CHUNK, payload);

  assert.deepEqual(
    Array.from(message),
    [
      0x50, 0x44, 0x52, 0x50,
      0x01, 0x04,
      0x00, 0x00,
      0x00, 0x00, 0x00, 0x03,
      0x01, 0x02, 0x03,
    ],
  );

  const decoded = decodeFrame(message);
  assert.equal(decoded.type, MessageType.CHUNK);
  assert.deepEqual(Array.from(decoded.payload), [1, 2, 3]);
});

test("FILE_BEGIN matches the C big-endian layout", () => {
  const sha256 = Uint8Array.from({ length: 32 }, (_, index) => index);
  const filename = "skripsi-final.pdf";
  const payload = encodeFileBegin({
    fileSize: 0x0102030405060708n,
    sha256,
    filename,
  });

  assert.equal(payload.byteLength, FILE_BEGIN_FIXED_SIZE + filename.length);
  assert.deepEqual(Array.from(payload.subarray(0, 8)), [1, 2, 3, 4, 5, 6, 7, 8]);
  assert.deepEqual(Array.from(payload.subarray(8, 40)), Array.from(sha256));
  assert.equal(payload[40], 0);
  assert.equal(payload[41], filename.length);
  assert.equal(new TextDecoder().decode(payload.subarray(FILE_BEGIN_FIXED_SIZE)), filename);
});

test("ACK parsing matches the C payload contract", () => {
  const payload = new Uint8Array(12);
  payload[0] = MessageType.CHUNK;
  payload[1] = AckStatus.OK;
  new DataView(payload.buffer).setBigUint64(4, 0x0102030405060708n, false);

  const ack = decodeAck(encodeFrame(MessageType.ACK, payload));
  assert.equal(ack.acknowledgedType, MessageType.CHUNK);
  assert.equal(ack.status, AckStatus.OK);
  assert.equal(ack.receivedBytes, 0x0102030405060708n);
});

test("relay limits remain stricter than the core frame ceiling", () => {
  assert.equal(RELAY_WS_MESSAGE_MAX_SIZE, 60 * 1024);
  assert.equal(RELAY_MAX_PAYLOAD, RELAY_WS_MESSAGE_MAX_SIZE - FRAME_HEADER_SIZE);
  assert.ok(FILE_CHUNK_SIZE < RELAY_MAX_PAYLOAD);

  assert.throws(
    () => encodeFrame(MessageType.CHUNK, new Uint8Array(RELAY_MAX_PAYLOAD + 1)),
    /relay message limit/,
  );
});

test("malformed frames and unsafe metadata are rejected", () => {
  const message = encodeFrame(MessageType.FILE_END);
  message[0] = 0;
  assert.throws(() => decodeFrame(message), /magic/);

  assert.throws(
    () => encodeFileBegin({ fileSize: 1n, sha256: new Uint8Array(31), filename: "a.pdf" }),
    /32 bytes/,
  );
  assert.throws(
    () => encodeFileBegin({ fileSize: -1n, sha256: new Uint8Array(32), filename: "a.pdf" }),
    /unsigned 64-bit/,
  );
  assert.throws(
    () => encodeFileBegin({ fileSize: 1n, sha256: new Uint8Array(32), filename: "a\0.pdf" }),
    /without NUL/,
  );
});
