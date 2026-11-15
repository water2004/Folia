import assert from 'node:assert/strict';
import {readFileSync, writeFileSync, mkdirSync} from 'node:fs';
import {join, resolve} from 'node:path';
import {isDeepStrictEqual} from 'node:util';
import {JSDOM} from '../oracle/node_modules/jsdom/lib/api.js';
import sharp from 'sharp';
import {directory, fileHash, verifyLock} from './profile.mjs';

const [referencePath, nativePath, outputPath = 'build/mermaid-math/comparison'] = process.argv.slice(2);
if (!referencePath || !nativePath) throw new Error(
  'Usage: node compare-fragments.mjs <MathJax report.json> <native report.json> [output directory]');
verifyLock();
const expected = JSON.parse(readFileSync(referencePath, 'utf8'));
const actual = JSON.parse(readFileSync(nativePath, 'utf8'));
const corpusPath = join(directory, 'cases.json');
const corpus = JSON.parse(readFileSync(corpusPath, 'utf8'));
assert.equal(expected.schema, 1);
assert.equal(actual.schema, 1);
assert.equal(expected.corpusSha256, fileHash(corpusPath));
if (actual.corpusSha256) assert.equal(actual.corpusSha256, expected.corpusSha256);
assert.deepEqual(expected.records.map(item => item.name), corpus.cases.map(item => item.name));
assert.deepEqual(actual.records.map(item => item.name), expected.records.map(item => item.name));
const destination = resolve(outputPath);
mkdirSync(destination, {recursive: true});

const numericAttributes = new Set(['x', 'y', 'x1', 'x2', 'y1', 'y2', 'cx', 'cy', 'r',
  'rx', 'ry', 'width', 'height', 'stroke-width', 'font-size', 'opacity']);
const tokens = value => value.match(/[A-Za-z]+|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?|[^\s,]/g)
  ?.map(token => Number.isFinite(Number(token)) ? Number(token) : token) ?? [];

function svgTree(svg) {
  const dom = new JSDOM(svg, {contentType: 'image/svg+xml'});
  const root = dom.window.document.documentElement;
  assert.equal(root.localName, 'svg');
  const ids = new Map([...root.querySelectorAll('[id]')].map((element, index) =>
    [element.getAttribute('id'), `resource-${index}`]));
  const reference = value => {
    for (const [id, replacement] of ids) value = value.replaceAll(`#${id}`, `#${replacement}`);
    return value;
  };
  const walk = element => {
    const attrs = {};
    for (const {name, value} of [...element.attributes].sort((a, b) => a.name.localeCompare(b.name))) {
      // Accessibility and TeX annotations do not affect SVG geometry/paint.
      if (name === 'xmlns' || name === 'xmlns:xlink' || name === 'role' ||
          name === 'focusable' || name.startsWith('data-')) continue;
      if (element === root && (name === 'width' || name === 'height')) continue;
      if (element === root && name === 'style') {
        assert.match(value, /^\s*vertical-align:\s*[-.\d]+(?:ex|px)?;?\s*$/);
        continue;
      }
      if (name === 'id') attrs[name] = ids.get(value) ?? value;
      else if (['d', 'transform', 'viewBox', 'points'].includes(name)) attrs[name] = tokens(value);
      else if (numericAttributes.has(name) && Number.isFinite(Number(value))) attrs[name] = Number(value);
      else attrs[name] = reference(value);
    }
    const children = [...element.childNodes].flatMap(child => {
      if (child.nodeType === 1) {
        const value = walk(child);
        return value ? [value] : [];
      }
      return child.nodeType === 3 && child.textContent.trim() ? [child.textContent] : [];
    });
    // An empty MathML fence can leave an SVG group containing only a transform.
    // It has no paint or extent; an extra translation on it changes no geometry.
    if (element !== root && element.localName === 'g' && children.length === 0 &&
        Object.keys(attrs).every(name => name === 'transform')) return null;
    return {tag: element.localName, attrs, children};
  };
  const tree = walk(root);
  dom.window.close();
  return tree;
}

function firstDifference(expected, actual, path = '$') {
  if (isDeepStrictEqual(expected, actual)) return null;
  if (expected === null || actual === null || typeof expected !== 'object' || typeof actual !== 'object')
    return {path, expected, actual};
  if (Array.isArray(expected) !== Array.isArray(actual)) return {path, reason: 'container type differs'};
  if (!isDeepStrictEqual(Object.keys(expected), Object.keys(actual)))
    return {path, expectedKeys: Object.keys(expected), actualKeys: Object.keys(actual)};
  for (const key of Object.keys(expected)) {
    const difference = firstDifference(expected[key], actual[key], `${path}.${key}`);
    if (difference) return difference;
  }
  return {path, reason: 'value differs'};
}

// The common viewport is only a test canvas, like an SVG diagram embedding a
// formula. Every glyph/path/transform/paint comes unchanged from each renderer.
function canvas(svg, width, height) {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" ` +
    `viewBox="0 0 ${width} ${height}"><g transform="translate(1,1)">${svg}</g></svg>`;
}

const records = [];
for (let index = 0; index < expected.records.length; ++index) {
  const reference = expected.records[index], native = actual.records[index];
  if (reference.error || native.error) {
    records.push({name: reference.name, accepted: false,
      referenceError: reference.error, nativeError: native.error});
    continue;
  }
  const dimensions = {};
  for (const key of ['width', 'height', 'baseline']) {
    assert(Number.isFinite(reference[key]) && Number.isFinite(native[key]), `${native.name}/${key}`);
    dimensions[key] = {expected: reference[key], actual: native[key],
      difference: Math.abs(reference[key] - native[key])};
  }
  const metricsEqual = Object.values(dimensions).every(item => item.difference <= 1e-9);
  const treeDifference = firstDifference(svgTree(reference.svg), svgTree(native.svg));
  const width = Math.max(1, Math.ceil(Math.max(reference.width, native.width))) + 2;
  const height = Math.max(1, Math.ceil(Math.max(reference.height, native.height))) + 2;
  const source = sharp(Buffer.from(canvas(reference.svg, width, height)), {density: 144}).ensureAlpha();
  const target = sharp(Buffer.from(canvas(native.svg, width, height)), {density: 144}).ensureAlpha();
  const a = await source.clone().raw().toBuffer({resolveWithObject: true});
  const b = await target.clone().raw().toBuffer({resolveWithObject: true});
  assert.deepEqual(a.info, b.info);
  let differentPixels = 0, maximumChannelDifference = 0;
  for (let pixel = 0; pixel < a.data.length; pixel += 4) {
    let differs = false;
    for (let channel = 0; channel < 4; ++channel) {
      const delta = Math.abs(a.data[pixel + channel] - b.data[pixel + channel]);
      differs ||= delta !== 0;
      maximumChannelDifference = Math.max(maximumChannelDifference, delta);
    }
    differentPixels += Number(differs);
  }
  const sourcePng = join(destination, `${reference.name}-official.png`);
  const nativePng = join(destination, `${reference.name}-native.png`);
  await source.png().toFile(sourcePng);
  await target.png().toFile(nativePng);
  writeFileSync(join(destination, `${reference.name}-native.svg`), native.svg);
  records.push({name: reference.name, accepted: metricsEqual && !treeDifference && differentPixels === 0,
    metricsEqual, dimensions, vectorEqual: !treeDifference, treeDifference,
    raster: {width: a.info.width, height: a.info.height, differentPixels, maximumChannelDifference,
      referenceSha256: fileHash(sourcePng), nativeSha256: fileHash(nativePng)}});
}
const report = {schema: 1, referenceSha256: fileHash(referencePath), nativeSha256: fileHash(nativePath),
  corpusSha256: expected.corpusSha256, rasterizer: sharp.versions,
  metricsToleranceCssPixels: 1e-9, vectorComparison: 'Exact ordered geometry and paint tree after serialization-only normalization',
  rasterComparison: 'Exact RGBA pixels, common 2x SVG canvas, no image alignment or filtering',
  total: records.length, accepted: records.filter(record => record.accepted).length, records};
writeFileSync(join(destination, 'report.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({destination, total: report.total, accepted: report.accepted,
  failures: records.filter(record => !record.accepted).map(({name, metricsEqual, treeDifference, raster}) =>
    ({name, metricsEqual, treeDifference, raster}))}));
process.exitCode = report.accepted === report.total ? 0 : 1;
