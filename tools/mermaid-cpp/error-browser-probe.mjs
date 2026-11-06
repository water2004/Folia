import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';

export const executablePath = process.env.FOLIA_BROWSER ??
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';

export function observeErrorSvg() {
  const svg = document.querySelector('svg');
  const { x, y, width, height } = svg.viewBox.baseVal;
  const shape = (element) => {
    const paint = getComputedStyle(element);
    const box = element.getBBox();
    return {
      box: [box.x, box.y, box.width, box.height],
      fill: paint.fill,
      stroke: paint.stroke,
    };
  };
  return {
    view: [x, y, width, height],
    width: svg.getAttribute('width'),
    maxWidth: svg.style.maxWidth,
    paths: [...svg.querySelectorAll('.error-icon')].map((element) => ({
      ...shape(element),
      d: element.getAttribute('d'),
    })),
    labels: [...svg.querySelectorAll('.error-text')].map((element) => ({
      ...shape(element),
      text: element.textContent,
      anchor: getComputedStyle(element).textAnchor,
    })),
  };
}

if (import.meta.main) {
  const directory = resolve(process.argv[2] ?? 'build/mermaid-cpp/error-browser');
  mkdirSync(directory, { recursive: true });
  const url = pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const browser = await chromium.launch({ executablePath, args: ['--allow-file-access-from-files'] });
  const observations = [];
  try {
    for (const theme of ['default', 'dark']) {
      const file = resolve(directory, 'probe.html');
      writeFileSync(file, `<!doctype html><meta charset="utf-8"><body><script type="module">
        const { default: mermaid } = await import(${JSON.stringify(url)});
        try {
          mermaid.initialize({ startOnLoad: false, theme: ${JSON.stringify(theme)} });
          const { svg } = await mermaid.render('mermaid-error', 'error');
          document.body.innerHTML = svg;
          document.body.dataset.ready = 'true';
        } catch (error) {
          document.body.dataset.failure = String(error);
        }
      </script>`);
      const page = await browser.newPage();
      await page.goto(pathToFileURL(file).href);
      await page.waitForSelector('body[data-ready],body[data-failure]');
      assert.equal(await page.locator('body').getAttribute('data-failure'), null);
      const observation = await page.evaluate(observeErrorSvg);
      assert.equal(observation.paths.length, 6);
      assert.equal(observation.labels.length, 2);
      writeFileSync(resolve(directory, `error-${theme}.svg`),
        await page.locator('svg').evaluate((svg) => svg.outerHTML));
      observations.push({ theme, ...observation });
      await page.close();
    }
  } finally {
    await browser.close();
  }
  writeFileSync(resolve(directory, 'original.json'), JSON.stringify(observations, null, 2));
  console.log(`Captured ${observations.length} Mermaid error SVG baselines.`);
}
