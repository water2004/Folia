import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';

const originalPath = resolve(process.argv[2]);
const nativeDirectory = resolve(process.argv[3]);
const baselineRows = JSON.parse(readFileSync(originalPath, 'utf8'));
const originals = process.env.FOLIA_TREE_VIEW_THEME === 'dark'
  ? baselineRows.filter((row) => row.name.endsWith('-dark'))
  : process.env.FOLIA_TREE_VIEW_THEME === 'default'
    ? baselineRows.filter((row) => !row.name.endsWith('-dark'))
    : baselineRows;
let maximumDifference = 0;

function compare(actual, expected, path, tolerance = 0) {
  if (typeof expected === 'number') {
    const difference = Math.abs(actual - expected);
    assert(Number.isFinite(actual) && difference <= tolerance,
      `${path}: ${actual} != ${expected} (tolerance ${tolerance})`);
    maximumDifference = Math.max(maximumDifference, difference);
    return;
  }
  if (Array.isArray(expected)) {
    assert.equal(actual.length, expected.length, path);
    expected.forEach((value, index) => compare(actual[index], value, `${path}.${index}`, tolerance));
    return;
  }
  if (expected && typeof expected === 'object') {
    for (const [key, value] of Object.entries(expected)) {
      compare(actual[key], value, `${path}.${key}`, tolerance);
    }
    return;
  }
  assert.equal(actual, expected, path);
}

function inspectTreeViewSvg(svg) {
  const view = svg.viewBox.baseVal;
  const inverse = svg.getScreenCTM().inverse();
  const box = (element) => {
    const bounds = element.getBBox();
    const matrix = inverse.multiply(element.getScreenCTM());
    const points = [
      [bounds.x, bounds.y],
      [bounds.x + bounds.width, bounds.y],
      [bounds.x, bounds.y + bounds.height],
      [bounds.x + bounds.width, bounds.y + bounds.height],
    ].map(([x, y]) => ({
      x: matrix.a * x + matrix.c * y + matrix.e,
      y: matrix.b * x + matrix.d * y + matrix.f,
    }));
    const xs = points.map((point) => point.x);
    const ys = points.map((point) => point.y);
    return {
      x: Math.min(...xs),
      y: Math.min(...ys),
      width: Math.max(...xs) - Math.min(...xs),
      height: Math.max(...ys) - Math.min(...ys),
    };
  };
  const labels = [...svg.querySelectorAll('.treeView-node-label, .treeView-node-description')]
    .map((element) => {
      const style = getComputedStyle(element);
      return {
        text: element.textContent,
        cls: element.classList.contains('treeView-node-description') ? 'description' : 'label',
        box: box(element),
        fill: style.fill,
        fontFamily: style.fontFamily,
        fontWeight: style.fontWeight,
        fontStyle: style.fontStyle,
      };
    });
  const lines = [...svg.querySelectorAll('.treeView-node-line')]
    .map((element) => ({
      ...Object.fromEntries(['x1', 'y1', 'x2', 'y2'].map((key) => [
        key, Number(element.getAttribute(key)),
      ])),
      stroke: getComputedStyle(element).stroke,
      strokeWidth: getComputedStyle(element).strokeWidth,
    }))
    .sort((left, right) => left.y1 - right.y1 || left.x1 - right.x1 ||
      left.y2 - right.y2 || left.x2 - right.x2);
  const highlights = [...svg.querySelectorAll('.treeView-highlight-bg')]
    .map((element) => {
      const style = getComputedStyle(element);
      return {
        box: box(element),
        fill: style.fill,
        stroke: style.stroke,
        strokeWidth: style.strokeWidth,
      };
    });
  const icons = [...svg.querySelectorAll('.treeView-node-icon')]
    .map((element) => ({
      color: getComputedStyle(element).color,
      body: [...element.querySelectorAll('path')].map((path) => ({
        d: path.getAttribute('d'),
        fill: getComputedStyle(path).fill,
        fillRule: getComputedStyle(path).fillRule,
        clipRule: getComputedStyle(path).clipRule,
      })),
    }));
  return {
    view: { x: view.x, y: view.y, width: view.width, height: view.height },
    labels,
    lines,
    highlights,
    icons,
  };
}

const browser = await chromium.launch({
  executablePath: process.env.FOLIA_BROWSER ??
    'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  headless: true,
});

try {
  const page = await browser.newPage();
  for (const original of originals) {
    const nativeSvg = readFileSync(
      resolve(nativeDirectory, `tree-view-${original.name}.svg`), 'utf8');
    const observations = {};
    for (const [name, source] of [['oracle', original.svg], ['native', nativeSvg]]) {
      await page.setContent(source);
      observations[name] = await page.locator('svg').first().evaluate(inspectTreeViewSvg);
    }

    const actual = observations.native;
    const expected = observations.oracle;
    const prefix = original.name;
    compare(actual.labels, expected.labels, `${prefix}.labels`, 1.5);
    compare(actual.lines, expected.lines, `${prefix}.lines`, 1.5);
    compare(actual.highlights, expected.highlights, `${prefix}.highlights`, 1.5);
    compare(actual.icons, expected.icons, `${prefix}.icons`);
    compare(actual.view, expected.view, `${prefix}.view`, 1.5);
  }
  await page.close();
} finally {
  await browser.close();
}

writeFileSync(resolve(dirname(originalPath), 'comparison.json'), JSON.stringify({
  checked: originals.length,
  measuredGeometryTolerance: 1.5,
  maximumDifference,
}, null, 2));
console.log(`Compared ${originals.length} native treeView SVGs with Mermaid 12.2.0 baselines (1.5 px).`);
