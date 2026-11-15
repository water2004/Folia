// Optional, real-browser observations of stock Mermaid v12.2.0 (KaTeX).
// This is intentionally not described as an official v12 MathJax renderer.
import assert from 'node:assert/strict';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {join, resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from '../oracle/node_modules/playwright-core/index.mjs';
import {directory, fileHash, root} from './profile.mjs';

const destination = resolve(process.argv[2] ?? 'build/mermaid-math/stock-v12-browser');
mkdirSync(destination, {recursive: true});
const packageRoot = join(root, 'tools/mermaid-cpp/oracle/node_modules/mermaid');
assert.equal(JSON.parse(readFileSync(join(packageRoot, 'package.json'), 'utf8')).version, '12.2.0');
const corpusPath = join(directory, 'diagrams.json');
const cases = JSON.parse(readFileSync(corpusPath, 'utf8')).diagrams;
const htmlPath = join(destination, 'host.html');
writeFileSync(htmlPath, `<!doctype html><meta charset="utf-8"><body style="margin:0">` +
  `<script type="module">import mermaid from ${JSON.stringify(pathToFileURL(join(packageRoot,
    'dist/mermaid.esm.mjs')).href)};window.mermaid=mermaid;</script>`);
const executablePath = process.env.FOLIA_BROWSER ?? (process.platform === 'win32' ?
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe' : '/usr/bin/chromium');
const browser = await chromium.launch({executablePath, headless: true,
  args: ['--allow-file-access-from-files']});
const records = [];
try {
  const page = await browser.newPage({viewport: {width: 1600, height: 1200}});
  await page.goto(pathToFileURL(htmlPath).href);
  await page.waitForFunction(() => window.mermaid);
  for (const item of cases) {
    const record = await page.evaluate(async item => {
      document.querySelectorAll('body > div').forEach(node => node.remove());
      mermaid.initialize({startOnLoad: false, securityLevel: 'strict', fontFamily: 'Arial',
        deterministicIds: true, ...item.config});
      try {
        const {svg} = await mermaid.render('math_' + item.name, item.source);
        const host = document.createElement('div');
        host.id = 'host'; host.innerHTML = svg; document.body.append(host);
        const rect = element => {
          const box = element.getBoundingClientRect();
          return {x: box.x, y: box.y, width: box.width, height: box.height};
        };
        const css = element => {
          const style = getComputedStyle(element);
          return {font: style.font, lineHeight: style.lineHeight, color: style.color,
            display: style.display, alignItems: style.alignItems, whiteSpace: style.whiteSpace};
        };
        return {...item, svg, error: null, mathCount: host.querySelectorAll('math').length,
          labels: [...host.querySelectorAll('foreignObject')].map(element => ({
            box: rect(element), html: element.innerHTML,
            children: [...element.querySelectorAll('div,span,math')].map(child => ({
              tag: child.tagName, text: child.textContent, box: rect(child), css: css(child)}))}))};
      } catch (error) {
        return {...item, svg: null, error: {name: error.name, message: error.message}};
      }
    }, item);
    records.push(record);
    if (record.svg) {
      writeFileSync(join(destination, item.name + '.svg'), record.svg);
      await page.locator('#host').screenshot({path: join(destination, item.name + '.png')});
    }
  }
  writeFileSync(join(destination, 'report.json'), JSON.stringify({schema: 1,
    reference: 'Stock Mermaid 12.2.0 / KaTeX, actual browser DOM and screenshots',
    browser: browser.version(), executablePath, corpusSha256: fileHash(corpusPath), records}, null, 2) + '\n');
} finally {
  await browser.close();
}
console.log(JSON.stringify({destination, records: records.length}));
