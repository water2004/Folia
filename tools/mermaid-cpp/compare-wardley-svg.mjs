import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {edge, inspectWardley} from './wardley-browser-probe.mjs';

const [sourceFile, nativeDir] = process.argv.slice(2);
assert(sourceFile && nativeDir,
  'Usage: node compare-wardley-svg.mjs original.json nativeSvgDir');

const rows = JSON.parse(readFileSync(sourceFile, 'utf8'));
const tolerance = 1.5;
const failures = [];

function compare(actual, expected, path) {
  const numericString = typeof expected === 'string' &&
    expected.trim() !== '' && Number.isFinite(Number(expected));

  if (typeof expected === 'number' || numericString) {
    const limit = path.includes('.rotation.') ? 1e-8 : tolerance;
    if (Math.abs(Number(actual) - Number(expected)) > limit) {
      failures.push(`${path}: actual ${actual}, expected ${expected} (tolerance ${limit})`);
    }
    return;
  }

  if (Array.isArray(expected)) {
    if (!Array.isArray(actual) || actual.length !== expected.length) {
      failures.push(`${path}: actual length ${actual?.length}, expected ${expected.length}`);
      return;
    }
    expected.forEach((value, index) => compare(actual[index], value, `${path}.${index}`));
    return;
  }

  if (expected && typeof expected === 'object') {
    for (const [key, value] of Object.entries(expected)) {
      compare(actual?.[key], value, `${path}.${key}`);
    }
    return;
  }

  if (actual !== expected) failures.push(`${path}: actual ${actual}, expected ${expected}`);
}

const browser = await chromium.launch({executablePath: edge, headless: true});
try {
  const page = await browser.newPage();
  await page.evaluate(
    'document.head.innerHTML="<style>svg{font-family:\\"trebuchet ms\\"}</style>"; ' +
    'globalThis.inspectWardley=' + inspectWardley.toString() + '; void 0');

  for (const {svg, name, ...expected} of rows) {
    try {
      const native = readFileSync(resolve(nativeDir, `${name}.svg`), 'utf8');
      const actual = JSON.parse(await page.evaluate(svg => {
        document.body.innerHTML = svg;
        return JSON.stringify(inspectWardley(document));
      }, native));
      compare(actual, expected, name);
    } catch (error) {
      failures.push(`${name}: ${error.message}`);
    }
  }
} finally {
  await browser.close();
}

if (failures.length) {
  console.error(failures.join('\n'));
  process.exitCode = 1;
} else {
  console.log(`Compared ${rows.length} Wardley browser SVGs at ${tolerance} px`);
}
