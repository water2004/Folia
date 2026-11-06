import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';

process.argv.push('--library');
const { inspectIshikawa } = await import('./ishikawa-browser-probe.mjs');
const originals = JSON.parse(readFileSync(process.argv[2]));
const nativeDirectory = process.argv[3];
const browser = await chromium.launch({
  executablePath: process.env.FOLIA_BROWSER ?? 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  headless: true,
});
let maximumDifference = 0;

function compare(actual, expected, path, tolerance = 0) {
  if (typeof expected === 'number') {
    const difference = Math.abs(actual - expected);
    assert(Number.isFinite(actual) && difference <= tolerance,
      `${path}: ${actual} != ${expected} (tol ${tolerance})`);
    maximumDifference = Math.max(maximumDifference, difference);
  } else if (Array.isArray(expected)) {
    assert.equal(actual.length, expected.length, path);
    expected.forEach((value, index) => compare(actual[index], value, `${path}[${index}]`, tolerance));
  } else if (expected && typeof expected === 'object') {
    for (const [key, value] of Object.entries(expected)) compare(actual[key], value, `${path}.${key}`, tolerance);
  } else {
    assert.equal(actual, expected, path);
  }
}

try {
  const page = await browser.newPage();
  for (const original of originals) {
    if (original.error) continue;
    const file = resolve(nativeDirectory, `ishikawa-${original.name}.svg`);
    await page.setContent(readFileSync(file, 'utf8'));
    const actual = await page.evaluate(inspectIshikawa);
    const prefix = original.name;
    compare(actual.view, original.view, `${prefix}.view`, 1.5);
    assert.equal(actual.lines.length, original.lines.length, `${prefix}.lines.length`);
    for (let index = 0; index < actual.lines.length; ++index) {
      const a = actual.lines[index], e = original.lines[index], path = `${prefix}.line${index}`;
      compare(a.cls, e.cls, `${path}.class`);
      for (const key of ['x1', 'y1', 'x2', 'y2']) compare(a[key], e[key], `${path}.${key}`, 1.5);
      compare(a.marker, e.marker, `${path}.marker`);
    }
    assert.equal(actual.labels.length, original.labels.length, `${prefix}.labels.length`);
    for (let index = 0; index < actual.labels.length; ++index) {
      const a = actual.labels[index], e = original.labels[index], path = `${prefix}.label${index}`;
      compare(a.text, e.text, `${path}.text`);
      compare(a.cls, e.cls, `${path}.class`);
      compare(a.box, e.box, `${path}.box`, 1.5);
      compare(a.fontSize, e.fontSize, `${path}.fontSize`);
    }
    assert.equal(actual.shapes.length, original.shapes.length, `${prefix}.shapes.length`);
    for (let index = 0; index < actual.shapes.length; ++index) {
      const a = actual.shapes[index], e = original.shapes[index], path = `${prefix}.shape${index}`;
      compare(a.cls, e.cls, `${path}.class`);
      compare(a.box, e.box, `${path}.box`, 1.5);
      compare(a.fill, e.fill, `${path}.fill`);
      compare(a.stroke, e.stroke, `${path}.stroke`);
    }
  }
} finally {
  await browser.close();
}

writeFileSync(resolve(dirname(process.argv[2]), 'ishikawa-comparison.json'), JSON.stringify({
  checked: originals.filter(item => !item.error).length,
  geometryTolerance: 1.5,
  maximumDifference,
}, null, 2));
console.log(`Compared ${originals.length} Ishikawa browser baselines at 1.5 px tolerance.`);
