import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {edge} from './venn-browser-probe.mjs';

const [originalFile, nativeDir] = process.argv.slice(2);
assert(originalFile && nativeDir, 'Usage: node compare-eventmodeling-svg.mjs original.json nativeSvgDir');

const cases = JSON.parse(readFileSync(originalFile, 'utf8'));
const browser = await chromium.launch({executablePath: edge, headless: true});

function inspect(source) {
  document.body.innerHTML = source;
  const root = document.querySelector('svg');
  const matrix = root.getScreenCTM().inverse();
  const box = element => {
    const {x, y, width, height} = element.getBBox();
    const transform = matrix.multiply(element.getScreenCTM());
    const a = new DOMPoint(x, y).matrixTransform(transform);
    const b = new DOMPoint(x + width, y + height).matrixTransform(transform);
    return [a.x, a.y, b.x - a.x, b.y - a.y];
  };
  const normalize = markup => markup.replaceAll(/;\s*/g, ';').replaceAll(/:\s*/g, ':').replaceAll(/;(?=")/g, '');
  const rects = [...root.querySelectorAll('rect')].map(element => {
    const {fill, stroke, rx} = getComputedStyle(element);
    return {box: box(element), style: {fill, stroke, rx}};
  });
  const routes = [...root.querySelectorAll('path')].map(element => {
    const {fill, stroke, strokeWidth} = getComputedStyle(element);
    const transform = matrix.multiply(element.getScreenCTM());
    const points = [0, element.getTotalLength()].flatMap(length => {
      const point = element.getPointAtLength(length).matrixTransform(transform);
      return [point.x, point.y];
    });
    return {points, style: {fill, stroke, strokeWidth}};
  });
  const labels = [...root.querySelectorAll('text')].map(element => {
    const {fontFamily, fontWeight, fontSize, fill} = getComputedStyle(element);
    return {box: box(element), text: element.textContent, style: {fontFamily, fontWeight, fontSize, fill}};
  });
  const payloads = [...root.querySelectorAll('foreignObject')].map(element => ({
    box: box(element),
    markup: normalize(element.querySelector('span').innerHTML),
  }));
  const {x, y, width, height} = root.viewBox.baseVal;
  const marker = root.querySelector('marker');
  return {
    view: [x, y, width, height], rects, routes, labels, payloads,
    arrowhead: getComputedStyle(marker.querySelector('polygon')).fill,
  };
}

try {
  const page = await browser.newPage();
  for (const sample of cases) {
    const id = `${sample.name}/${sample.theme}`;
    const native = readFileSync(resolve(nativeDir, `eventmodeling-${sample.name}-${sample.theme}.svg`), 'utf8');
    const actual = await page.evaluate(inspect, native);
    const upstream = await page.evaluate(inspect, sample.svg);
    const close = (numbers, expected, label) => {
      assert.equal(numbers.length, expected.length, `${id} ${label} count`);
      numbers.forEach((number, index) => {
        assert(Math.abs(number - expected[index]) <= 1.5,
          `${id} ${label}[${index}] ${number} vs ${expected[index]}`);
      });
    };
    close(actual.view, upstream.view, 'viewBox');
    assert.equal(actual.arrowhead, upstream.arrowhead, `${id} arrowhead fill`);
    for (const group of ['rects', 'routes', 'labels', 'payloads']) {
      assert.equal(actual[group].length, upstream[group].length, `${id} ${group} count`);
      actual[group].forEach(({box, points, ...rest}, i) => {
        const {box: expectedBox, points: expectedPoints, ...expected} = upstream[group][i];
        close(box ?? points, expectedBox ?? expectedPoints, `${group}[${i}]`);
        assert.deepEqual(rest, expected, `${id} ${group}[${i}]`);
      });
    }
  }
  console.log(`Compared ${cases.length} Event Modeling SVGs at 1.5 px geometry tolerance.`);
} finally {
  await browser.close();
}
