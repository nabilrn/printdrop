import assert from 'node:assert/strict';
import fs from 'node:fs';
import test from 'node:test';

const html = fs.readFileSync(new URL('./index.html', import.meta.url), 'utf8');

function fileInputTag() {
  const match = html.match(/<input\b[^>]*\bid="file"[^>]*>/i);
  assert.ok(match, 'sender page must contain the file input');
  return match[0];
}

test('file picker is limited to print-friendly files', () => {
  const input = fileInputTag();
  assert.match(input, /\baccept="[^"]+"/i);
  assert.match(input, /\.pdf/i);
  assert.match(input, /\.docx/i);
  assert.match(input, /\.xlsx/i);
  assert.match(input, /\.pptx/i);
  assert.match(input, /\.jpg/i);
  assert.match(input, /\.png/i);
});

test('file picker never requests capture or audio sources', () => {
  const input = fileInputTag();
  assert.doesNotMatch(input, /\bcapture(?:\s|=|>)/i);
  assert.doesNotMatch(input, /audio\//i);
  assert.doesNotMatch(input, /video\//i);
});
