import assert from 'node:assert/strict';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';

process.argv.push('--library');
const { inspectSwimlane } = await import('./swimlane-browser-probe.mjs');
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
    assert.equal(actual.length, expected.length, `${path}.length`);
    expected.forEach((value, index) => compare(actual[index], value, `${path}[${index}]`, tolerance));
  } else if (expected && typeof expected === 'object') {
    for (const [key, value] of Object.entries(expected)) compare(actual[key], value, `${path}.${key}`, tolerance);
  } else assert.equal(actual, expected, path);
}
try {
  const page = await browser.newPage();
  for (const original of originals) {
    let file = resolve(nativeDirectory, `swimlane-${original.name}.svg`);
    if (!existsSync(file) && original.name.endsWith('-default'))
      file = resolve(nativeDirectory, `swimlane-${original.name.slice(0, -8)}.svg`);
    const nativeSvg = readFileSync(file, 'utf8');
    assert(!nativeSvg.includes('<foreignObject'), `${original.name}: native SVG contains foreignObject`);
    await page.setContent(nativeSvg);
    const actual = await page.evaluate(inspectSwimlane);
    compare(actual.view, original.view, `${original.name}.view`, 1.5);
    assert.equal(actual.labels.length, original.labels.length, `${original.name}.labels.length`);
    const actualLabels = [...actual.labels].sort((a, b) => a.text.localeCompare(b.text));
    const originalLabels = [...original.labels].sort((a, b) => a.text.localeCompare(b.text));
    for (let index = 0; index < actualLabels.length; ++index) {
      const a = actualLabels[index], e = originalLabels[index], path = `${original.name}.labels[${index}]`;
      compare(a.text, e.text, `${path}.text`);
      compare(a.box, e.box, `${path}.box`, 1.5);
      compare(a.glyphBox, e.glyphBox, `${path}.glyphBox`, 1.5);
      compare(a.fontSize, e.fontSize, `${path}.fontSize`);
      compare(a.fontFamily, e.fontFamily, `${path}.fontFamily`);
      compare(a.color, e.color, `${path}.color`);
      compare(a.fontWeight, e.fontWeight, `${path}.fontWeight`);
    }
    for (const key of ['lanes', 'nodes'])
      compare(actual[key], original[key], `${original.name}.${key}`, 1.5);
    compare(actual.edgeLabelBackgrounds, original.edgeLabelBackgrounds,
      `${original.name}.edgeLabelBackgrounds`);
    assert.equal(actual.edges.length, original.edges.length, `${original.name}.edges.length`);
    for (let index = 0; index < actual.edges.length; ++index) {
      const a = actual.edges[index], e = original.edges[index], path = `${original.name}.edges[${index}]`;
      assert.equal(a.stroke, e.stroke, `${path}.stroke`);
      compare(a.strokeWidth, e.strokeWidth, `${path}.strokeWidth`);
      const canonicalDash = (value) => value === 'none' || value === '0' || value === '0px'
        ? 'solid' : value;
      compare(canonicalDash(a.dasharray), canonicalDash(e.dasharray), `${path}.dasharray`);
      compare(a.markerFill, e.markerFill, `${path}.markerFill`);
      const aCommands = a.d.match(/[MmZzLlHhVvCcSsQqTtAa]/g) ?? [];
      const eCommands = e.d.match(/[MmZzLlHhVvCcSsQqTtAa]/g) ?? [];
      assert.deepEqual(aCommands, eCommands, `${path}.commands`);
      const numbers = (value) => (value.match(/-?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?/gi) ?? []).map(Number);
      compare(numbers(a.d), numbers(e.d), `${path}.geometry`, 1.5);
    }
  }
} finally { await browser.close(); }
writeFileSync(resolve(dirname(process.argv[2]), 'swimlane-comparison.json'), JSON.stringify({
  checked: originals.length, geometryTolerance: 1.5, maximumDifference,
}, null, 2));
console.log(`Compared ${originals.length} Swimlane browser SVG baselines at 1.5 px tolerance.`);
