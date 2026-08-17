import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import test from 'node:test';

import { createSha256 } from './sha256.js';

function hex(bytes) {
  return Buffer.from(bytes).toString('hex');
}

function expected(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}

function hashInChunks(bytes, chunkSize) {
  const hasher = createSha256();
  for (let offset = 0; offset < bytes.length; offset += chunkSize) {
    hasher.update(bytes.subarray(offset, Math.min(offset + chunkSize, bytes.length)));
  }
  return hex(hasher.array());
}

test('matches standard SHA-256 vectors', () => {
  const vectors = [
    ['', 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'],
    ['abc', 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'],
    ['The quick brown fox jumps over the lazy dog',
     'd7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592'],
  ];

  for (const [text, digest] of vectors) {
    assert.equal(hashInChunks(Buffer.from(text), 1), digest);
  }
});

test('matches Node crypto across block boundaries and chunk sizes', () => {
  const lengths = [0, 1, 55, 56, 57, 63, 64, 65, 127, 128, 129, 1024, 65537];
  const chunkSizes = [1, 3, 7, 31, 64, 257, 4096];

  for (const length of lengths) {
    const bytes = Buffer.alloc(length);
    for (let i = 0; i < bytes.length; i += 1) {
      bytes[i] = (i * 131 + 17) & 0xff;
    }
    const digest = expected(bytes);
    for (const chunkSize of chunkSizes) {
      assert.equal(hashInChunks(bytes, chunkSize), digest,
                   `length=${length} chunkSize=${chunkSize}`);
    }
  }
});

test('rejects updates after finalization', () => {
  const hasher = createSha256();
  hasher.update(new Uint8Array([1, 2, 3]));
  const first = hasher.array();
  assert.deepEqual(hasher.array(), first);
  assert.throws(() => hasher.update(new Uint8Array([4])), /finalized/);
});
