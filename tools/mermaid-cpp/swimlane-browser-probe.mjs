import { browserExecutable } from './oracle/browser-executable.mjs';
import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './swimlane-cases.mjs';

import inspectSwimlane from './oracle/observations/swimlane.mjs';
export {inspectSwimlane};

if (!process.argv.includes('--library')) {
  const directory = resolve(process.argv[2] ?? 'build/mermaid-cpp/swimlane-browser');
  mkdirSync(directory, { recursive: true });
  const moduleUrl = pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const browser = await chromium.launch({
    executablePath: browserExecutable(),
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
