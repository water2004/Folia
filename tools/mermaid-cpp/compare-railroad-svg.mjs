import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';

const nativeDirectory = resolve(process.argv[3] ?? 'build/mermaid-cpp/native-svg');
const browserPath = resolve(process.argv[2] ?? 'build/mermaid-cpp/railroad-browser/original.json');
const cases = JSON.parse(readFileSync(browserPath, 'utf8'));
assert(cases.length > 0, 'No Railroad browser baselines');
const tolerance = 1.5;
const browser = await chromium.launch({
  executablePath: process.env.FOLIA_BROWSER ??
    'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  headless: true,
});
const page = await browser.newPage();

async function inspect(svg) {
  await page.setContent(svg);
  return page.locator('svg').evaluate((root) => {
    const measure = (element) => {
      const matrix = root.getScreenCTM().inverse().multiply(element.getScreenCTM());
      const { x, y, width, height } = element.getBBox();
      return [matrix.e + x, matrix.f + y, width, height];
    };
    const shapes = [...root.querySelectorAll('.railroad-rule :is(path, rect, text, circle)')];
    const view = root.viewBox.baseVal;
    return {
      view: [view.x, view.y, view.width, view.height],
      sizing: root.getAttribute('width') === '100%'
        ? ['responsive', parseFloat(root.style.maxWidth)]
        : ['fixed', Number(root.getAttribute('width')), Number(root.getAttribute('height'))],
      accessibility: [
        root.getAttribute('role'), root.getAttribute('aria-roledescription'),
        ...['aria-labelledby', 'aria-describedby'].map((key) =>
          document.getElementById(root.getAttribute(key))?.textContent ?? null),
      ],
      shapes: shapes.map((element) => {
        const style = getComputedStyle(element);
        const type = element.localName;
        const context = [];
        for (let parent = element; parent !== root; parent = parent.parentElement) {
          const name = parent.getAttribute('class');
          if (name) context.push(name);
        }
        const result = {
          identity: [type, context, type === 'text' ? element.textContent : ''],
          bounds: measure(element),
          style: [style.fill, style.stroke, style.strokeDasharray],
          numeric: [parseFloat(style.strokeWidth)],
        };
        if (type === 'text') {
          result.style.push(style.fontFamily, style.fontWeight, style.textAnchor, style.dominantBaseline);
          result.numeric.push(parseFloat(style.fontSize), element.x.baseVal[0].value, element.y.baseVal[0].value);
        } else if (type === 'rect') {
          result.numeric.push(element.rx.baseVal.value, element.ry.baseVal.value);
        } else if (type === 'path') {
          let arcParameter = -1;
          result.commands = [];
          for (const token of element.getAttribute('d').trim().split(/\s+/)) {
            if (/^[MLA]$/.test(token)) {
              result.commands.push(token);
              arcParameter = token === 'A' ? 0 : -1;
            } else if (arcParameter >= 2 && arcParameter <= 4) {
              result.commands.push(token);
              ++arcParameter;
            } else {
              result.numeric.push(Number(token));
              if (arcParameter >= 0) ++arcParameter;
            }
          }
        }
        return result;
      }),
    };
  });
}

function near(actual, expected, label) {
  assert.equal(actual.length, expected.length, label);
  expected.forEach((value, index) => {
    assert(Math.abs(actual[index] - value) <= tolerance,
      `${label}[${index}]: native ${actual[index]}, browser ${value}`);
  });
}

let count = 0;
try {
  for (const { file, svg } of cases) {
    const actual = await inspect(readFileSync(resolve(nativeDirectory, file), 'utf8'));
    const expected = await inspect(svg);
    near(actual.view, expected.view, file + ' viewBox');
    assert.equal(actual.sizing[0], expected.sizing[0], file + ' size mode');
    near(actual.sizing.slice(1), expected.sizing.slice(1), file + ' intrinsic size');
    assert.deepEqual(actual.accessibility, expected.accessibility, file + ' accessibility');
    assert.equal(actual.shapes.length, expected.shapes.length, file + ' element count');
    actual.shapes.forEach((shape, index) => {
      const oracle = expected.shapes[index];
      const label = file + ' element ' + index;
      assert.deepEqual(shape.identity, oracle.identity, label + ' structure');
      assert.deepEqual(shape.style, oracle.style, label + ' styles');
      assert.deepEqual(shape.commands, oracle.commands, label + ' path commands and flags');
      near(shape.bounds, oracle.bounds, label + ' bounds');
      near(shape.numeric, oracle.numeric, label + ' coordinates');
    });
    ++count;
  }
} finally {
  await browser.close();
}
console.log(`Compared ${count} Railroad SVGs against Mermaid 12.2.0.`);
