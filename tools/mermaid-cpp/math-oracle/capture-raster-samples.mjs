// Generate actual 2x librsvg/Sharp raster observations. Python owns acceptance.
import {createHash} from 'node:crypto';
import {mkdirSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {createInterface} from 'node:readline';
import sharp from 'sharp';

const destination = resolve(process.argv[2]);
mkdirSync(destination, {recursive: true});
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
for await (const line of createInterface({input: process.stdin, crlfDelay: Infinity})) {
  const request = JSON.parse(line), inputSha256 = hash(line);
  if (request.kind !== 'svg') throw new Error('Expected an SVG sample request');
  const image = sharp(Buffer.from(request.source), {density: 144}).ensureAlpha();
  const {data, info} = await image.clone().raw().toBuffer({resolveWithObject: true});
  const png = await image.png().toBuffer();
  writeFileSync(resolve(destination, inputSha256 + '.rgba'), data);
  writeFileSync(resolve(destination, inputSha256 + '.png'), png);
  process.stdout.write(JSON.stringify({schema: 1, kind: request.kind, inputSha256,
    backend: {name: 'sharp', versions: sharp.versions}, ...info,
    sourceSha256: hash(request.source), rgba: inputSha256 + '.rgba', rgbaSha256: hash(data),
    png: inputSha256 + '.png', pngSha256: hash(png)}) + '\n');
}
