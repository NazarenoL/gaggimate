export const PHOTO_SIZE = 480;
export const PHOTO_BYTES = PHOTO_SIZE * PHOTO_SIZE * 2;

export function encodeRgb565(rgba) {
  if (rgba.length !== PHOTO_SIZE * PHOTO_SIZE * 4) throw new Error('Invalid photo size');
  const bytes = new Uint8Array(PHOTO_BYTES);
  for (let i = 0, j = 0; i < rgba.length; i += 4, j += 2) {
    const pixel = ((rgba[i] >> 3) << 11) | ((rgba[i + 1] >> 2) << 5) | (rgba[i + 2] >> 3);
    bytes[j] = pixel & 255;
    bytes[j + 1] = pixel >> 8;
  }
  return bytes;
}

export function photoPreview(bytes) {
  if (bytes.length !== PHOTO_BYTES) throw new Error('Invalid saved photo');
  const canvas = document.createElement('canvas');
  canvas.width = canvas.height = PHOTO_SIZE;
  const ctx = canvas.getContext('2d');
  const image = ctx.createImageData(PHOTO_SIZE, PHOTO_SIZE);
  for (let j = 0, i = 0; j < bytes.length; j += 2, i += 4) {
    const pixel = bytes[j] | (bytes[j + 1] << 8);
    image.data[i] = Math.round((((pixel >> 11) & 31) * 255) / 31);
    image.data[i + 1] = Math.round((((pixel >> 5) & 63) * 255) / 63);
    image.data[i + 2] = Math.round(((pixel & 31) * 255) / 31);
    image.data[i + 3] = 255;
  }
  ctx.putImageData(image, 0, 0);
  return canvas.toDataURL();
}

export async function preparePhoto(file) {
  if (!file.type.startsWith('image/')) throw new Error('Choose an image file');
  if (file.size > 20 * 1024 * 1024) throw new Error('Choose a photo smaller than 20 MB');
  const bitmap = await createImageBitmap(file);
  try {
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = PHOTO_SIZE;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, PHOTO_SIZE, PHOTO_SIZE);
    const scale = Math.min(PHOTO_SIZE / bitmap.width, PHOTO_SIZE / bitmap.height);
    const width = bitmap.width * scale;
    const height = bitmap.height * scale;
    ctx.drawImage(bitmap, (PHOTO_SIZE - width) / 2, (PHOTO_SIZE - height) / 2, width, height);
    const bytes = encodeRgb565(ctx.getImageData(0, 0, PHOTO_SIZE, PHOTO_SIZE).data);
    return { bytes, preview: photoPreview(bytes) };
  } finally {
    bitmap.close();
  }
}
