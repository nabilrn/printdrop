import assert from 'node:assert/strict';
import fs from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';

import { AckStatus, MessageType, decodeFrame, encodeFrame } from './protocol.js';
import {
  buildSenderWebSocketUrl,
  hashFileIncremental,
  parseSessionId,
  sendFile,
} from './sender.js';

function loadBrowserSha256() {
  const source = fs.readFileSync(new URL('./vendor/js-sha256/build/sha256.min.js', import.meta.url),
                                 'utf8');
  const context = { ArrayBuffer, Uint8Array };
  context.globalThis = context;
  vm.runInNewContext(source, context, { filename: 'sha256.min.js' });
  if (!context.sha256 || typeof context.sha256.create !== 'function') {
    throw new Error('pinned browser SHA-256 build did not expose sha256.create');
  }
  return context.sha256;
}

const browserSha256 = loadBrowserSha256();

function ackFrame(type, receivedBytes, status = AckStatus.OK) {
  const payload = new Uint8Array(12);
  payload[0] = type;
  payload[1] = status;
  new DataView(payload.buffer).setBigUint64(4, receivedBytes, false);
  return encodeFrame(MessageType.ACK, payload);
}

class FakeSocket extends EventTarget {
  constructor() {
    super();
    this.readyState = 1;
    this.sent = [];
    this.receivedBytes = 0n;
  }

  send(data) {
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    const frame = decodeFrame(bytes);
    this.sent.push(frame.type);
    if (frame.type === MessageType.CHUNK) {
      this.receivedBytes += BigInt(frame.payload.byteLength);
    }
    const ack = ackFrame(frame.type, this.receivedBytes);
    queueMicrotask(() => this.dispatchEvent(new MessageEvent('message', { data: ack })));
  }
}

function fileLike(name, bytes) {
  const blob = new Blob([bytes]);
  return {
    name,
    size: blob.size,
    slice: blob.slice.bind(blob),
  };
}

test('parses only canonical session paths', () => {
  assert.equal(parseSessionId('/s/00112233445566778899aabbccddeeff'),
               '00112233445566778899aabbccddeeff');
  assert.equal(parseSessionId('/s/00112233445566778899AABBCCDDEEFF'), null);
  assert.equal(parseSessionId('/wrong/00112233445566778899aabbccddeeff'), null);
});

test('builds same-host websocket URLs', () => {
  const id = '00112233445566778899aabbccddeeff';
  assert.equal(buildSenderWebSocketUrl({ protocol: 'https:', host: 'send.printdrop.app' }, id),
               `wss://send.printdrop.app/v1/sender/${id}`);
  assert.equal(buildSenderWebSocketUrl({ protocol: 'http:', host: '127.0.0.1:8080' }, id),
               `ws://127.0.0.1:8080/v1/sender/${id}`);
});

test('pinned SHA-256 supports bounded incremental hashing', async () => {
  const file = fileLike('abc.txt', new TextEncoder().encode('abc'));
  const digest = await hashFileIncremental(file, () => browserSha256.create(), null, 1);
  assert.equal(Buffer.from(digest).toString('hex'),
               'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
});

test('sends begin, bounded chunks, and end with ACK backpressure', async () => {
  const bytes = new Uint8Array(100_000);
  for (let index = 0; index < bytes.length; index += 1) bytes[index] = index & 0xff;
  const file = fileLike('sample.bin', bytes);
  const socket = new FakeSocket();
  const progress = [];

  await sendFile({
    file,
    socket,
    hasherFactory: () => browserSha256.create(),
    onUploadProgress(current) { progress.push(current); },
    ackTimeoutMs: 1000,
  });

  assert.deepEqual(socket.sent,
                   [MessageType.FILE_BEGIN, MessageType.CHUNK, MessageType.CHUNK,
                    MessageType.CHUNK, MessageType.FILE_END]);
  assert.deepEqual(progress, [49_152, 98_304, 100_000]);
  assert.equal(socket.receivedBytes, 100_000n);
});
