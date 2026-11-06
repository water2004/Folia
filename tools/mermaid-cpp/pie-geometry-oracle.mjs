// Observe real D3 Pie/Arc calls. Expected paths are never produced by copied
// C++ formulas. Only Mermaid's radius and donut configuration are supplied.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const require = createRequire(path.join(root, 'tools/mermaid-cpp/oracle/package.json'));
const shapeEntry = require.resolve('d3-shape');
const pathEntry = require.resolve('d3-path');
const shapeRoot = path.resolve(path.dirname(shapeEntry), '..');
const pathRoot = path.resolve(path.dirname(pathEntry), '..');
assert.equal(JSON.parse(readFileSync(path.join(shapeRoot, 'package.json'), 'utf8')).version, '3.2.0');
assert.equal(JSON.parse(readFileSync(path.join(pathRoot, 'package.json'), 'utf8')).version, '3.1.0');
assert.equal(createRequire(path.join(shapeRoot, 'package.json')).resolve('d3-path'), pathEntry,
  'd3-shape must execute the pinned d3-path dependency');
const sourceHashes = [
  [shapeRoot, 'src/pie.js', '5316912248e7e426fd41bab3af62719bada72fb0394a097c6cf251667f053c75'],
  [shapeRoot, 'src/arc.js', 'e657e8455af7c62613bde73cd45cea79709608d97759fe5e4c3e5ef6cdbd8deb'],
  [shapeRoot, 'src/math.js', '876190a6d1250f781fa8ca8a1bd0fba1a3db718fe2355c5a69b034aac6caf490'],
  [shapeRoot, 'src/path.js', '121bd97e99ead26d6bd84f20296c10040dc9caa9fceeca34d5ff917ef6e49065'],
  [shapeRoot, 'LICENSE', 'faa682e3e430941f958d26180458f5934a62f58dac4d70ccdd15608c15d0f884'],
  [pathRoot, 'src/path.js', '2c2d30e2c5279c8f6f42edc6731a4af029949ec83f47ffb48c3e35d21f877800'],
  [pathRoot, 'LICENSE', 'a09b0d6d76e768fca2085fad05743ee81ddd11946bcda160a3cf5e1081a479aa'],
];
for (const [directory, relative, hash] of sourceHashes) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(directory, relative))).digest('hex'),
    hash, `Changed D3 source: ${directory}/${relative}`);
}
const { pie, arc } = await import(pathToFileURL(shapeEntry));

const cases = [];
const add = (name, values, hole = 0, position = 0.75) => cases.push({ name, values, hole, position });
add('empty', []);
add('single', [1]);
add('single-large', [1e308]);
add('single-small', [1e-300]);
add('two-halves', [1, 1]);
add('three-thirds', [1, 1, 1]);
add('four-quarters', [1, 1, 1, 1]);
add('unequal-preserve-order', [5, 1, 9, 2]);
add('unequal-reverse-order', [2, 9, 1, 5]);
add('fractions', [0.1, 0.2, 0.3, 0.4]);
add('large-magnitudes', [1e300, 2e300, 4e300]);
add('small-magnitudes', [1e-300, 2e-300, 4e-300]);
add('single-zero', [0]);
add('all-zero', [0, -0, 0]);
add('single-negative', [-1]);
add('all-negative', [-1, -3, -2]);
add('zero-before-positive', [0, 1]);
add('zero-between-positives', [1, 0, 1]);
add('zero-after-positive', [1, 0]);
add('negative-mixed', [-2, 3, 0, 1, -4]);
add('tiny-first', [1e-15, 1]);
add('tiny-last', [1, 1e-15]);
add('shape-epsilon-below', [1e-14, 1], 0.5);
add('shape-epsilon-above', [1e-12, 1], 0.5);
add('path-epsilon-below', [1e-8, 1], 0.5);
add('path-epsilon-above', [1e-6, 1], 0.5);
add('near-full-path-epsilon', [1, 1e-8], 0.5);
add('near-full-shape-epsilon', [1, 1e-14], 0.5);
add('near-half-less', [1, 1.0000001], 0.5);
add('near-half-more', [1.0000001, 1], 0.5);
add('donut-single', [1], 0.5);
add('donut-halves', [1, 1], 0.5);
add('donut-thirds', [1, 1, 1], 0.5);
add('donut-max', [3, 2, 1], 0.9);
add('donut-min-positive', [1, 2], Number.MIN_VALUE);
add('donut-negative-disabled', [1, 2], -0.2);
add('donut-over-limit-disabled', [1, 2], 0.9000000000000001);
add('donut-one-disabled', [1, 2], 1);
add('donut-large-disabled', [1, 2], 1e300);
add('donut-collapsed', [0, -1, 1], 0.5);
add('label-center', [1, 2, 3], 0.3, 0);
add('label-outer-edge', [1, 2, 3], 0.3, 1);
add('label-inner-position', [1, 2, 3], 0.7, 0.2);
// Geometry input is after configuration merging, with no JSON-schema runtime
// validator. Observe D3 centroid's actual finite behavior outside [0, 1].
add('label-negative-position', [1, 2], 0, -0.25);
add('label-outside-position', [1, 2], 0, 1.5);
add('label-large-position', [1, 2], 0, 1e100);
add('many-equal', Array(100).fill(1), 0.9);
add('unequal-donut', [1.23, 0.52, 2.59, 4.01, 8.12], 0.12345, 0.625);
// Radius half-millipoints exercise Math.round on both signs in real paths.
for (const millipoints of [0.5, 1.5, 2.5, 1234.5, 43210.5, 166499.5]) {
  add(`radius-half-${millipoints}`, [1, 1], millipoints / 1000 / 185);
}
// Deterministic varied angles, values, row counts and radii supplement named
// threshold cases without substituting a second implementation for D3.
let seed = 0x5f3759df;
const random = () => (seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0);
for (let index = 0; index < 32; ++index) {
  const values = Array.from({ length: 1 + random() % 24 }, () => (1 + random() % 100000) / 1000);
  add(`seeded-${index}`, values, (random() % 901) / 1000, (random() % 1001) / 1000);
}

const decimal = value => Object.is(value, -0) ? '-0' : String(value);
const lines = [
  '# Actual d3-shape@3.2.0 pie().sort(null), arc() and arc.centroid(); d3-path@3.1.0 default digits=3.',
  '# Geometry stage only: Mermaid section filtering, text, colors, legends and measurement are excluded.',
  '# name\tdonut_hole\ttext_position\tvalues(comma)\tsectors(value|start|end|label_x|label_y|path;...)',
];
let sectors = 0;
for (const item of cases) {
  const hole = item.hole > 0 && item.hole <= 0.9 ? item.hole : 0;
  const generator = arc().innerRadius(hole * 185).outerRadius(185);
  const label = arc().innerRadius(185 * item.position).outerRadius(185 * item.position);
  const data = pie().sort(null)(item.values);
  const expected = data.map(datum => {
    const centroid = label.centroid(datum);
    const pathData = generator(datum);
    assert.equal(typeof pathData, 'string');
    assert(!/NaN|Infinity/.test(pathData));
    assert(centroid.every(Number.isFinite));
    ++sectors;
    return [datum.value, datum.startAngle, datum.endAngle, ...centroid].map(decimal).join('|') + '|' + pathData;
  }).join(';');
  lines.push([item.name, decimal(item.hole), decimal(item.position),
    item.values.map(decimal).join(',') || '-', expected || '-'].join('\t'));
}
const content = lines.join('\n') + '\n';
const fixture = path.join(root, 'tests/mermaid_cpp/fixtures/pie-geometry.tsv');
const argumentsSet = new Set(process.argv.slice(2));
assert([...argumentsSet].every(argument => argument === '--write' || argument === '--check'),
  'Usage: node tools/mermaid-cpp/pie-geometry-oracle.mjs [--write|--check]');
assert(!(argumentsSet.has('--write') && argumentsSet.has('--check')), 'Choose --write or --check');
if (argumentsSet.has('--write')) {
  writeFileSync(fixture, content);
  console.log(`Wrote ${cases.length} real D3 Pie geometry cases (${sectors} sectors).`);
} else {
  assert.equal(readFileSync(fixture, 'utf8').replaceAll('\r\n', '\n'), content,
    'D3 Pie geometry fixture differs from actual pinned D3 output');
  console.log(`Verified ${cases.length} real D3 Pie geometry cases (${sectors} sectors).`);
}
