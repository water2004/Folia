// Sample generator for the pinned real-font Canvas backend. This module never
// runs the native renderer and contains no native/reference comparison rules.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {mkdirSync, writeFileSync} from 'node:fs';
import {createRequire} from 'node:module';
import {resolve} from 'node:path';
import {createInterface} from 'node:readline';

const require = createRequire(new URL('./package.json', import.meta.url));
const {createCanvas, loadImage, GlobalFonts} = require('@napi-rs/canvas');
const version = require('@napi-rs/canvas/package.json').version;
assert.equal(version, '1.1.1', 'Regenerate samples with the locked Canvas package');
const destination = resolve(process.argv[2]);
mkdirSync(destination, {recursive: true});
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const backend = {name: '@napi-rs/canvas', version, engine: 'Skia', browser: false, estimatedWidths: false};
const context = createCanvas(1, 1).getContext('2d');

for await (const line of createInterface({input: process.stdin, crlfDelay: Infinity})) {
  const request = JSON.parse(line), inputSha256 = hash(line);
  let sample;
  if (request.kind === 'profile') {
    sample = {availableFonts: GlobalFonts.families};
  } else if (request.kind === 'text') {
    context.font = request.font;
    const metrics = context.measureText(request.text);
    sample = {font: context.font, text: request.text, metrics: {
      advance: metrics.width,
      ink_width: metrics.actualBoundingBoxLeft + metrics.actualBoundingBoxRight,
      bounds_width: Math.max(metrics.width, metrics.actualBoundingBoxRight) + Math.max(0, metrics.actualBoundingBoxLeft),
      line_height: metrics.fontBoundingBoxAscent + metrics.fontBoundingBoxDescent,
      baseline: metrics.fontBoundingBoxAscent,
    }};
  } else if (request.kind === 'svg') {
    const canvas = createCanvas(request.width, request.height), target = canvas.getContext('2d');
    target.drawImage(await loadImage(Buffer.from(request.source)), 0, 0);
    const pixels = target.getImageData(0, 0, request.width, request.height).data;
    const rgba = Buffer.from(pixels.buffer, pixels.byteOffset, pixels.byteLength);
    const png = canvas.toBuffer('image/png');
    writeFileSync(resolve(destination, inputSha256 + '.rgba'), rgba);
    writeFileSync(resolve(destination, inputSha256 + '.png'), png);
    sample = {width: request.width, height: request.height, channels: 4,
      sourceSha256: hash(request.source), rgba: inputSha256 + '.rgba', rgbaSha256: hash(rgba),
      png: inputSha256 + '.png', pngSha256: hash(png)};
  } else {
    throw new Error('Unknown sample request: ' + request.kind);
  }
  process.stdout.write(JSON.stringify({schema: 1, kind: request.kind, inputSha256, backend, ...sample}) + '\n');
}
