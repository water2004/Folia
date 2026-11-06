import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { edge, inspectVenn } from './venn-browser-probe.mjs';

const [originalFile, nativeDirectory] = process.argv.slice(2);
assert(originalFile && nativeDirectory, 'Usage: node compare-venn-svg.mjs original.json nativeSvgDir');
const original = JSON.parse(readFileSync(originalFile, 'utf8'));
const tolerance = 1.5;
function compare(actual, expected, key) {
  if (typeof expected === 'number') {
    assert(Math.abs(actual - expected) <= tolerance, key + ': ' + actual + ' vs ' + expected);
  } else if (Array.isArray(expected)) {
    assert.equal(actual.length, expected.length, key + ' length');
    expected.forEach((value, i) => compare(actual[i], value, key + '.' + i));
  } else if (expected && typeof expected === 'object') {
    for (const [name, value] of Object.entries(expected)) compare(actual[name], value, key + '.' + name);
  } else {
    assert.equal(actual, expected, key);
  }
}
const browser = await chromium.launch({executablePath: edge, headless: true});
try {
  const page = await browser.newPage();
  await page.evaluate('globalThis.inspectVenn = ' + inspectVenn.toString());
  for (const {svg, name, ...expected} of original) {
    const nativeSvg = readFileSync(resolve(nativeDirectory, name + '.svg'), 'utf8');
    const actual = await page.evaluate(source => {
      document.body.innerHTML = source;
      return inspectVenn();
    }, nativeSvg);
    compare(actual, expected, name);
  }
} finally {
  await browser.close();
}
console.log('Compared ' + original.length + ' native Venn SVGs at ' + tolerance + ' px');
