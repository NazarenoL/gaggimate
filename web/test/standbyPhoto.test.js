import assert from 'node:assert/strict';
import test from 'node:test';
import { encodeRgb565, PHOTO_BYTES, PHOTO_SIZE } from '../src/utils/standbyPhoto.js';

test('RGB565 uses device byte order and the correct primary colors', () => {
  const rgba = new Uint8Array(PHOTO_SIZE * PHOTO_SIZE * 4);
  rgba.set([255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255]);
  const bytes = encodeRgb565(rgba);
  assert.equal(bytes.length, PHOTO_BYTES);
  assert.deepEqual(Array.from(bytes.slice(0, 8)), [0, 248, 224, 7, 31, 0, 255, 255]);
  assert.equal(bytes.at(-1), 0);
});

test('rejects incomplete image buffers', () => {
  assert.throws(() => encodeRgb565(new Uint8Array(4)), /Invalid photo size/);
});
