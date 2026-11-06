import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { executablePath, observeErrorSvg } from './error-browser-probe.mjs';

const original = JSON.parse(readFileSync(process.argv[2], 'utf8'));
const browser = await chromium.launch({ executablePath });
try {
  const page = await browser.newPage();
  for (const { theme, paths, labels, ...view } of original) {
    const file = resolve(process.argv[3], `error-${theme}.svg`);
    await page.setContent(readFileSync(file, 'utf8'));
    const { paths: actualPaths, labels: actualLabels, ...actualView } = await page.evaluate(observeErrorSvg);
    assert.deepEqual(actualView, view, `${theme}: viewport`);
    for (const [kind, observed, expected] of [
      ['icon paths', actualPaths, paths],
      ['labels', actualLabels, labels],
    ]) {
      assert.equal(observed.length, expected.length, `${theme}: ${kind}`);
      for (const [index, item] of observed.entries()) {
        const { box, ...paint } = item;
        const { box: expectedBox, ...expectedPaint } = expected[index];
        assert.deepEqual(paint, expectedPaint, `${theme}: ${kind} ${index}`);
        box.forEach((value, i) => assert(
          Math.abs(value - expectedBox[i]) <= 1.5,
          `${theme}: ${kind} ${index} box ${i}: ${value} != ${expectedBox[i]}`,
        ));
      }
    }
  }
} finally {
  await browser.close();
}
console.log(`Compared ${original.length} native error SVGs with pinned Mermaid.`);
