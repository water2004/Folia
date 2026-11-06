import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './swimlane-cases.mjs';

export function inspectSwimlane() {
  const svg = document.querySelector('svg');
  const view = svg.viewBox.baseVal;
  const inverse = svg.getScreenCTM().inverse();
  const box = (element) => {
    let target = element;
    while (target && typeof target.getBBox !== 'function') target = target.parentElement;
    if (!target) target = element.querySelector('rect, path');
    if (!target || typeof target.getBBox !== 'function') return { x: 0, y: 0, width: 0, height: 0 };
    const bounds = target.getBBox();
    const matrix = inverse.multiply(target.getScreenCTM());
    const points = [[bounds.x, bounds.y], [bounds.x + bounds.width, bounds.y],
      [bounds.x, bounds.y + bounds.height], [bounds.x + bounds.width, bounds.y + bounds.height]]
      .map(([x, y]) => ({ x: matrix.a * x + matrix.c * y + matrix.e,
        y: matrix.b * x + matrix.d * y + matrix.f }));
    const xs = points.map((point) => point.x), ys = points.map((point) => point.y);
    return { x: Math.min(...xs), y: Math.min(...ys),
      width: Math.max(...xs) - Math.min(...xs), height: Math.max(...ys) - Math.min(...ys) };
  };
  return {
    view: { x: view.x, y: view.y, width: view.width, height: view.height },
    labels: [...svg.querySelectorAll('.nodeLabel, .cluster-label, .edgeLabel')].map((element) => ({
      text: element.textContent.trim().replace(/\s+/g, ' '),
      class: element.getAttribute('class'), box: box(element),
      fontSize: getComputedStyle(element).fontSize,
    })),
    lanes: [...svg.querySelectorAll('.cluster')].map((element) => {
      const shape = element.querySelector('rect.swimlane-title, rect') ?? element;
      const style = getComputedStyle(shape);
      return { box: box(element), fill: style.fill, stroke: style.stroke };
    }),
    nodes: [...svg.querySelectorAll('.node')].map((element) => ({
      box: box(element), text: element.textContent.trim().replace(/\s+/g, ' '),
      fill: getComputedStyle(element.querySelector('rect, circle, ellipse, polygon, path') ?? element).fill,
      stroke: getComputedStyle(element.querySelector('rect, circle, ellipse, polygon, path') ?? element).stroke,
      strokeWidth: getComputedStyle(element.querySelector('rect, circle, ellipse, polygon, path') ?? element).strokeWidth,
    })),
    edges: [...svg.querySelectorAll('.edgePath path, path.flowchart-link')].map((element) => ({
      d: element.getAttribute('d'), stroke: getComputedStyle(element).stroke,
    })),
  };
}

if (!process.argv.includes('--library')) {
  const directory = resolve(process.argv[2] ?? 'build/mermaid-cpp/swimlane-browser');
  mkdirSync(directory, { recursive: true });
  const moduleUrl = pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const browser = await chromium.launch({
    executablePath: process.env.FOLIA_BROWSER ?? 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true, args: ['--allow-file-access-from-files'],
  });
  const rows = [];
  try {
    for (const theme of ['default', 'dark']) for (const [name, source] of cases) {
      const page = await browser.newPage();
      const html = resolve(directory, 'probe.html');
      writeFileSync(html, `<!doctype html><meta charset="utf-8"><body><script type="module">
        const {default:mermaid}=await import(${JSON.stringify(moduleUrl)});
        try { mermaid.initialize(${JSON.stringify({ startOnLoad: false, theme, fontFamily: 'Arial', themeVariables: { fontFamily: 'Arial' }, look: 'classic' })});
          const {svg}=await mermaid.render('mermaid-swimlane',${JSON.stringify(source)}); document.body.innerHTML=svg;
          const observation=(${inspectSwimlane.toString()})(); document.body.innerHTML='<pre id="result"></pre>';
          document.querySelector('pre').textContent=JSON.stringify({svg,...observation});
        } catch(error) { document.body.innerHTML='<pre id="error"></pre>'; document.querySelector('pre').textContent=error.message; }
      </script>`);
      await page.goto(pathToFileURL(html).href);
      await page.waitForSelector('pre#result,pre#error', { timeout: 60000 });
      assert.equal(await page.locator('#error').count(), 0, await page.locator('pre').textContent());
      const suffix = `${name}-${theme}`;
      const row = { name: suffix, source, theme, ...JSON.parse(await page.locator('pre').textContent()) };
      rows.push(row);
      writeFileSync(resolve(directory, `swimlane-${suffix}.svg`), row.svg);
      await page.close();
    }
  } finally { await browser.close(); }
  writeFileSync(resolve(directory, 'original.json'), JSON.stringify(rows, null, 2));
  console.log(`Captured ${rows.length} Mermaid 12.2.0 Swimlane SVG baselines.`);
}
