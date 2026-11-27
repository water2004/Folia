// Real SVG measurement, on the same Windows host as the native probe.
// No browser profile, JavaScript runtime, or browser dependency enters Folia.
import { browserExecutable } from './oracle/browser-executable.mjs';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import assert from 'node:assert/strict';

const directory = resolve(process.argv[2] ?? 'build/mermaid-cpp');
mkdirSync(directory, { recursive: true });
const cases = [];
for (const family of ['Arial', 'Segoe UI', 'Times New Roman']) {
  for (const size of [17, 25]) {
    for (const text of ['', 'Mermaid', 'AV fi ffi', '  A   B  ', 'A\tB\nC', 'Cafe\u0301', '\u4e2d\u6587', '\u{1f600}', '\u0645\u0631\u062d\u0628\u0627', 'italic fj',
      'Long title '.repeat(30), 'A\r\nB', 'A\u00a0 B', 'abc \u0645\u0631\u062d\u0628\u0627 123', '  \t\r\n  ', 'A & B < C > D']) {
      cases.push({ family, size, text });
    }
  }
}
const html = `<!doctype html><meta charset="utf-8"><body><script>
const cases=${JSON.stringify(cases)};
const ns='http://www.w3.org/2000/svg';
const svg=document.body.appendChild(document.createElementNS(ns,'svg'));
const result=cases.map(item=>{
 const text=svg.appendChild(document.createElementNS(ns,'text'));
 text.style.fontFamily=item.family;
 text.style.fontSize=item.size+'px';
 text.textContent=item.text;
 const width=text.getBoundingClientRect().width;
 const advance=text.getComputedTextLength();
 text.remove();
 return {...item,width,advance};
});
document.body.replaceChildren(Object.assign(document.createElement('pre'),
 {id:'result',textContent:encodeURIComponent(JSON.stringify(result))}));
</script>`;
const file = resolve(directory, 'browser-text-probe.html');
writeFileSync(file, html);
const browser = await chromium.launch({ executablePath: browserExecutable(), headless: true });
let rows;
try {
  const page = await browser.newPage();
  await page.goto(pathToFileURL(file).href);
  rows = JSON.parse(decodeURIComponent(await page.locator('#result').innerText()));
} finally {
  await browser.close();
}
assert.equal(rows.length, cases.length);
assert.deepEqual(rows.map(({family,size,text})=>({family,size,text})),cases,
  'Browser transport changed a measured display string');
const hex = text => Array.from({length:text.length},(_,i)=>text.charCodeAt(i).toString(16).padStart(4,'0')).join('') || '-';
writeFileSync(resolve(directory, 'browser-text.tsv'), rows.map(row =>
  [row.family,row.size,hex(row.text),row.width,row.advance].join('\t')).join('\n')+'\n');
writeFileSync(resolve(directory, 'browser-text.json'), JSON.stringify(rows,null,2)+'\n');
console.log(`Measured ${rows.length} real browser SVG strings; results in ${directory}`);
