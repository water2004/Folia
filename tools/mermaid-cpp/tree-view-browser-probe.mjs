import assert from 'node:assert/strict';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './tree-view-cases.mjs';

const outputDirectory = resolve(process.argv[2] ?? 'build/mermaid-cpp/tree-view-browser');
mkdirSync(outputDirectory, { recursive: true });

const mermaidUrl = pathToFileURL(resolve(
  'tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs'
)).href;
const browser = await chromium.launch({
  executablePath: process.env.FOLIA_BROWSER ??
    'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  headless: true,
  args: ['--allow-file-access-from-files'],
});

const selectedCases = process.env.FOLIA_TREE_VIEW_CASE
  ? cases.filter(([name]) => name === process.env.FOLIA_TREE_VIEW_CASE)
  : cases;
const themes = process.env.FOLIA_TREE_VIEW_THEME
  ? [process.env.FOLIA_TREE_VIEW_THEME]
  : ['default', 'dark'];
const captured = [];

function outputNameFor(name, theme) {
  return theme === 'default' ? name : `${name}-${theme}`;
}

function probeHtml(source, theme, showIcons) {
  const config = {
    startOnLoad: false,
    theme,
    fontFamily: 'Arial',
    themeVariables: { fontFamily: 'Arial' },
    treeView: { showIcons },
  };
  return `<!doctype html><meta charset="utf-8"><body><script type="module">
    const { default: mermaid } = await import(${JSON.stringify(mermaidUrl)});
    try {
      mermaid.initialize(${JSON.stringify(config)});
      const { svg } = await mermaid.render('tree-view', ${JSON.stringify(source)});
      document.body.innerHTML = svg;
      document.body.setAttribute('data-tree-view-ready', 'true');
    } catch (error) {
      document.body.innerHTML = '<pre id="error"></pre>';
      document.querySelector('#error').textContent = error.stack;
    }
  </script>`;
}

async function inspectBrowserSvg(svgElement) {
  const box = (element) => {
    const bounds = element.getBBox();
    const matrix = svgElement.getCTM().inverse().multiply(element.getScreenCTM());
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

  const view = svgElement.viewBox.baseVal;
  return {
    view: { x: view.x, y: view.y, width: view.width, height: view.height },
    labels: [...svgElement.querySelectorAll('.treeView-node-label, .treeView-node-description')]
      .map((element) => ({
        text: element.textContent,
        cls: element.classList.contains('treeView-node-description') ? 'description' : 'label',
        box: box(element),
      })),
    lines: [...svgElement.querySelectorAll('.treeView-node-line')].map((element) => ({
      x1: Number(element.getAttribute('x1')),
      y1: Number(element.getAttribute('y1')),
      x2: Number(element.getAttribute('x2')),
      y2: Number(element.getAttribute('y2')),
    })),
  };
}

try {
  for (const [name, source] of selectedCases) {
    for (const theme of themes) {
      const outputName = outputNameFor(name, theme);
      const htmlPath = resolve(outputDirectory, 'probe.html');
      writeFileSync(htmlPath, probeHtml(source, theme, name === 'icons'));

      const page = await browser.newPage();
      const pageErrors = [];
      page.on('pageerror', (error) => pageErrors.push(error.message));
      page.on('console', (message) => {
        if (message.type() === 'error') pageErrors.push(message.text());
      });
      try {
        await page.goto(pathToFileURL(htmlPath).href);
        await page.waitForSelector('body[data-tree-view-ready="true"], pre#error', {
          timeout: 60_000,
        });
        if (await page.locator('#error').count()) {
          throw new Error(await page.locator('#error').textContent());
        }

        const svgElement = page.locator('svg').first();
        const svg = await svgElement.evaluate((element) => element.outerHTML);
        assert(svg.includes('viewBox=') && svg.includes('class="tree-view"'),
          `${outputName}: incomplete Mermaid SVG (${pageErrors.join('; ')})`);
        const observation = await svgElement.evaluate(inspectBrowserSvg);
        captured.push({ name: outputName, theme, source, svg, ...observation });
        writeFileSync(resolve(outputDirectory, `tree-view-${outputName}.svg`), svg);
      } finally {
        await page.close();
      }
    }
  }
} finally {
  await browser.close();
}

const originalPath = resolve(outputDirectory, 'original.json');
if (process.env.FOLIA_TREE_VIEW_CASE || process.env.FOLIA_TREE_VIEW_THEME) {
  const existing = JSON.parse(readFileSync(originalPath, 'utf8'));
  for (const row of captured) {
    const index = existing.findIndex((item) => item.name === row.name);
    if (index === -1) existing.push(row);
    else existing[index] = row;
  }
  writeFileSync(originalPath, JSON.stringify(existing, null, 2));
} else {
  writeFileSync(originalPath, JSON.stringify(captured, null, 2));
}

console.log(`Captured ${captured.length} Mermaid 12.2.0 treeView SVG baselines.`);
