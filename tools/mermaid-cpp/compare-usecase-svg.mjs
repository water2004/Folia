import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { inspectUsecase, edge } from './usecase-browser-probe.mjs';

const [originalFile, nativeDirectory] = process.argv.slice(2);
assert(originalFile && nativeDirectory, 'Usage: node compare-usecase-svg.mjs original.json nativeSvgDir');
const original = JSON.parse(readFileSync(originalFile, 'utf8'));
const tolerance = 1.5;
const numbers = /[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi;

function numeric(actual, expected, key) {
  assert(Number.isFinite(actual) && Math.abs(actual - expected) <= tolerance,
    `${key}: ${actual} vs ${expected}`);
}

function box(actual, expected, key) {
  for (const component of ['x', 'y', 'width', 'height'])
    numeric(actual[component], expected[component], `${key}.${component}`);
}

function path(actual, expected, key) {
  const commands = (value) => value.replace(numbers, '').match(/[A-Za-z]/g) ?? [];
  assert.deepEqual(commands(actual), commands(expected), `${key}.commands`);
  const actualValues = actual.match(numbers)?.map(Number) ?? [];
  const expectedValues = expected.match(numbers)?.map(Number) ?? [];
  assert.equal(actualValues.length, expectedValues.length, `${key}.coordinate count`);
  actualValues.forEach((value, i) => numeric(value, expectedValues[i], `${key}.coord.${i}`));
}

function items(actual, expected, key, compare) {
  assert.equal(actual.length, expected.length, `${key}.count`);
  expected.forEach((item, i) => compare(actual[i], item, `${key}.${i}`));
}

const browser = await chromium.launch({ executablePath: edge, headless: true });
let compared = 0;
try {
  const page = await browser.newPage();
  await page.evaluate('globalThis.inspectUsecase=' + inspectUsecase.toString());
  for (const { svg, name, ...expected } of original) {
    if (!svg) continue;
    const native = readFileSync(resolve(nativeDirectory, `usecase-${name}.svg`), 'utf8');
    const actual = await page.evaluate((source) => {
      document.body.innerHTML = source;
      return inspectUsecase();
    }, native);
    box(actual.view, expected.view, `${name}.view`);
    items(actual.labels, expected.labels, `${name}.labels`, (a, e, key) => {
      assert.equal(a.text, e.text, `${key}.text`);
      assert.equal(a.size, e.size, `${key}.size`);
      assert.equal(a.fill, e.fill, `${key}.fill`);
      box(a.box, e.box, key);
    });
    items(actual.nodes, expected.nodes, `${name}.nodes`, (a, e, key) => {
      assert.equal(a.id, e.id, `${key}.id`);
      assert.equal(a.shape, e.shape, `${key}.shape`);
      box(a.box, e.box, key);
      assert.deepEqual(a.paint, e.paint, `${key}.paint`);
    });
    items(actual.boundaries, expected.boundaries, `${name}.boundaries`, (a, e, key) => {
      assert.equal(a.id, e.id, `${key}.id`);
      box(a.box, e.box, key);
      items(a.parts, e.parts, `${key}.parts`, (part, reference, partKey) => {
        box(part.box, reference.box, partKey);
        assert.deepEqual(part.paint, reference.paint, `${partKey}.paint`);
      });
    });
    items(actual.edges, expected.edges, `${name}.edges`, (a, e, key) => {
      assert.equal(a.stroke, e.stroke, `${key}.stroke`);
      items(a.dash.match(numbers)?.map(Number) ?? [], e.dash.match(numbers)?.map(Number) ?? [],
        `${key}.dash`, numeric);
      path(a.d, e.d, key);
      for (const side of ['markerStart', 'markerEnd']) {
        assert.equal(!!a[side], !!e[side], `${key}.${side}`);
        if (!a[side]) continue;
        for (const field of ['shape', 'r'])
          assert.equal(a[side][field], e[side][field], `${key}.${side}.${field}`);
        for (const field of ['width', 'height', 'refX', 'refY'])
          numeric(a[side][field], e[side][field], `${key}.${side}.${field}`);
        path(a[side].d, e[side].d, `${key}.${side}`);
      }
    });
    ++compared;
  }
} finally {
  await browser.close();
}
console.log(`Compared ${compared} Usecase SVGs at ${tolerance} px.`);
