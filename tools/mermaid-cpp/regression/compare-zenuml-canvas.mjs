// Compare the production native renderer with official ZenUML SVGs using the
// same real Skia text shaper. Browser acceptance remains a separate gate.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {createRequire} from 'node:module';
import {resolve} from 'node:path';
import {createInterface} from 'node:readline';
import {loadCorpus, hash} from './corpus.mjs';
const requireOracle = createRequire(new URL('../oracle/package.json', import.meta.url));
const {createCanvas, loadImage} = requireOracle('@napi-rs/canvas');
const {JSDOM} = requireOracle('jsdom');
assert.equal(requireOracle('@napi-rs/canvas/package.json').version, '1.1.1');
const argument = name => process.argv.find(value => value.startsWith(`--${name}=`))?.slice(name.length + 3);
const probe = argument('probe');
assert(probe, '--probe=/path/to/FoliaMermaidRegressionProbe required');
const directory = resolve(argument('official') ?? 'build/mermaid-regression/zenuml-canvas');
const output = resolve(argument('output') ?? 'build/mermaid-regression/zenuml-canvas-native');
mkdirSync(output, {recursive: true});
const official = JSON.parse(readFileSync(resolve(directory, 'official.json'), 'utf8'));
assert.equal(official.upstream.package, '@zenuml/core');
assert.equal(official.upstream.version, '3.49.2');
assert.equal(official.upstream.gitHead, '213c239587ffa1f3dbb54d8b64468c3038ced55d');
assert.equal(official.backend.name, '@napi-rs/canvas');
assert.equal(official.backend.version, '1.1.1');
assert.equal(official.backend.browser, false);
assert.equal(official.backend.estimatedWidths, false);
const corpus = loadCorpus(), cases = corpus.records.filter(item => item.family === 'zenuml');
assert.deepEqual(official.cases.map(item => item.id), cases.map(item => item.id),
  'Every current ZenUML regression must have official SVG evidence');
for (const [index, row] of official.cases.entries()) {
  assert.equal(row.source, cases[index].source, `${row.id}: outdated official source`);
  assert.equal(row.sourceSha256, hash(row.source), `${row.id}: source hash mismatch`);
  assert.equal(Object.keys(cases[index].config ?? {}).length, 0,
    `${row.id}: the standalone ZenUML renderer has no Mermaid configuration adapter`);
}
const context = createCanvas(1, 1).getContext('2d');
const ascii = value => JSON.stringify(value).replace(/[^\x00-\x7f]/g,
  character => `\\u${character.charCodeAt(0).toString(16).padStart(4, '0')}`);
let nativeFontRequests = 0;
function measure(request) {
  if (request.method === 'family') return {family: request.family};
  assert.equal(request.method, 'text', 'ZenUML must request actual text widths only');
  assert.equal(request.styles.length, 0);
  ++nativeFontRequests;
  const font = request.font;
  context.font = `${font.italic ? 'italic ' : ''}${font.weight} ${font.size}px ${font.family}`;
  const text = request.preserve ? request.text : request.text.replace(/[ \t\r\n]+/g, ' ').trim();
  const metrics = context.measureText(text);
  assert(Number.isFinite(metrics.width) && metrics.width >= 0);
  return {advance: metrics.width,
    ink_width: metrics.actualBoundingBoxLeft + metrics.actualBoundingBoxRight,
    bounds_width: Math.max(metrics.width, metrics.actualBoundingBoxRight) + Math.max(0, metrics.actualBoundingBoxLeft),
    line_height: metrics.fontBoundingBoxAscent + metrics.fontBoundingBoxDescent,
    baseline: metrics.fontBoundingBoxAscent};
}
function labels(svg) {
  const dom = new JSDOM(svg, {contentType: 'image/svg+xml'});
  try {
    // Upstream emits empty condition placeholders for unqualified catches.
    // They contain no display text; their absence is not a missing label.
    return Array.from(dom.window.document.querySelectorAll('text'))
      .map(text => text.textContent.trim()).filter(text => text.length > 0).sort();
  } finally { dom.window.close(); }
}
async function raster(svg, path, width, height) {
  const canvas = createCanvas(width, height), target = canvas.getContext('2d');
  target.drawImage(await loadImage(Buffer.from(svg)), 0, 0);
  writeFileSync(path, canvas.toBuffer('image/png'));
  return target.getImageData(0, 0, width, height).data;
}
const child = spawn(resolve(probe), ['--remote-measurement'], {stdio: ['pipe', 'pipe', 'inherit']});
const lines = createInterface({input: child.stdout})[Symbol.asyncIterator]();
const exit = new Promise((resolveExit, reject) => {
  child.on('error', reject); child.on('close', (code, signal) => resolveExit({code, signal}));
});
const result = [];
try {
  for (const row of official.cases) {
    const initialFontRequests = nativeFontRequests;
    child.stdin.write(ascii({source: row.source, config: {theme: 'default', look: 'classic'}}) + '\n');
    let native;
    for (;;) {
      const next = await lines.next();
      assert(!next.done, `${row.id}: native probe terminated`);
      const response = JSON.parse(next.value);
      if (response.kind !== 'measure') { native = response; break; }
      child.stdin.write(ascii(measure(response)) + '\n');
    }
    if (!native.ok) {
      result.push({id: row.id, ok: false, nativeOk: false, error: native.error, identicalRaster: false});
      console.log(`${row.id}: FAIL ${native.error}`);
      continue;
    }
    assert(nativeFontRequests > initialFontRequests, `${row.id}: remote Canvas measurements are mandatory`);
    const originalSvg = readFileSync(resolve(directory, row.svg), 'utf8');
    assert.equal(hash(originalSvg), row.svgSha256, `${row.id}: official SVG evidence changed`);
    writeFileSync(resolve(output, `${row.id}.svg`), native.svg);
    const expectedLabels = labels(originalSvg), actualLabels = labels(native.svg);
    const widthDifference = Math.abs(row.width - native.width), heightDifference = Math.abs(row.height - native.height);
    const sameLabels = JSON.stringify(expectedLabels) === JSON.stringify(actualLabels);
    const pixelWidth = Math.ceil(Math.max(row.width, native.width)), pixelHeight = Math.ceil(Math.max(row.height, native.height));
    const expectedPixels = await raster(originalSvg, resolve(output, `${row.id}.official.png`), pixelWidth, pixelHeight);
    const actualPixels = await raster(native.svg, resolve(output, `${row.id}.native.png`), pixelWidth, pixelHeight);
    let differentPixels = 0, absoluteChannelDifference = 0;
    for (let i = 0; i < expectedPixels.length; i += 4) {
      let different = false;
      for (let channel = 0; channel < 4; ++channel) {
        const delta = Math.abs(expectedPixels[i + channel] - actualPixels[i + channel]);
        absoluteChannelDifference += delta;
        different ||= delta !== 0;
      }
      differentPixels += different;
    }
    const ok = widthDifference <= 1.5 && heightDifference <= 1.5 && sameLabels && differentPixels === 0;
    result.push({id: row.id, ok, sourceSha256: row.sourceSha256,
      officialSvgSha256: row.svgSha256, nativeSvgSha256: hash(native.svg),
      native: {width: native.width, height: native.height},
      official: {width: row.width, height: row.height}, widthDifference, heightDifference,
      sameLabels, expectedLabels, actualLabels, differentPixels,
      totalPixels: pixelWidth * pixelHeight, absoluteChannelDifference,
      // A basic viewport/content check cannot stand in for full SVG acceptance.
      identicalRaster: differentPixels === 0});
    console.log(`${row.id}: viewport delta ${widthDifference},${heightDifference}; labels ${sameLabels}; differing pixels ${differentPixels}`);
  }
} finally { child.stdin.end(); }
const status = await exit;
assert.equal(status.code, 0, 'Native probe must exit normally');
const report = {stage: 'zenuml-complete-canvas-svg', upstream: official.upstream,
  corpusSha256: corpus.summary.find(item => item.family === 'zenuml').sha256,
  nativeProbeSha256: hash(readFileSync(resolve(probe))),
  backend: '@napi-rs/canvas 1.1.1 (Skia)', browser: false,
  measurementSubstitution: false, nativeFontRequests, geometryTolerance: 1.5, rasterBackground: 'transparent', comparedChannels: 'RGBA',
  identicalRaster: result.filter(row => row.identicalRaster).length,
  passed: result.filter(row => row.ok).length,
  total: result.length, results: result};
writeFileSync(resolve(output, 'comparison.json'), JSON.stringify(report, null, 2) + '\n');
if (result.some(row => !row.ok)) process.exitCode = 1;
