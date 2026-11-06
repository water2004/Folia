import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './railroad-cases.mjs';

const outputDirectory = resolve(process.argv[2] ?? 'build/mermaid-cpp/railroad-browser');
mkdirSync(outputDirectory, { recursive: true });
const mermaidUrl = pathToFileURL(resolve(
  'tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs',
)).href;
const browser = await chromium.launch({
  executablePath: process.env.FOLIA_BROWSER ??
    'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  headless: true,
  args: ['--allow-file-access-from-files'],
});
const page = await browser.newPage();
const captured = [];
const htmlPath = resolve(outputDirectory, 'probe.html');
writeFileSync(htmlPath, '<!doctype html><meta charset="utf-8"><body>');
try {
  await page.goto(pathToFileURL(htmlPath).href);
  for (const [name, source] of cases) {
    for (const theme of ['default', 'dark']) {
      const { svg, error } = await page.evaluate(async ({ mermaidUrl, theme, source }) => {
        const { default: mermaid } = await import(mermaidUrl);
        mermaid.initialize({
          startOnLoad: false, theme, fontFamily: 'Arial', themeVariables: { fontFamily: 'Arial' },
        });
        try {
          return { svg: (await mermaid.render('railroad', source)).svg };
        } catch (error) {
          return { error: String(error) };
        }
      }, { mermaidUrl, theme, source });
      const outputName = theme === 'default' ? name : name + '-dark';
      assert(!error, outputName + ': ' + error);
      assert(svg.includes('class="railroad-diagram"') && svg.includes('viewBox='), outputName);
      const file = 'railroad-' + outputName + '.svg';
      captured.push({ file, svg });
      writeFileSync(resolve(outputDirectory, file), svg);
    }
  }
} finally {
  await browser.close();
}
writeFileSync(resolve(outputDirectory, 'original.json'), JSON.stringify(captured, null, 2));
console.log(`Captured ${cases.length * 2} Mermaid 12.2.0 Railroad SVG baselines.`);
