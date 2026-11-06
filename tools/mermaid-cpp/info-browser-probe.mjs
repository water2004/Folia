import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './info-cases.mjs';

const directory=resolve(process.argv[2]??'build/mermaid-cpp/info-browser');
mkdirSync(directory,{recursive:true});
const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
try {
  const rows=[];
  for(const [name,source] of cases) for(const theme of ['default','dark']) {
    const config={startOnLoad:false,theme};
    const file=resolve(directory,'probe.html');
    writeFileSync(file,`<!doctype html><meta charset="utf-8"><body><script type="module">const {default:mermaid}=await import(${JSON.stringify(url)});try{mermaid.initialize(${JSON.stringify(config)});const {svg}=await mermaid.render('mermaid-info',${JSON.stringify(source)});document.body.innerHTML=svg;const e=document.querySelector('text.version'),s=document.querySelector('svg'),b=e.getBBox(),c=getComputedStyle(e),r=s.getBoundingClientRect(),result={svg,view:{width:s.viewBox.baseVal.width,height:s.viewBox.baseVal.height},width:s.getAttribute('width'),height:s.getAttribute('height'),style:s.getAttribute('style'),viewport:{x:r.x,y:r.y,width:r.width,height:r.height},text:e.textContent,box:{x:b.x,y:b.y,width:b.width,height:b.height},font:{size:c.fontSize,anchor:c.textAnchor,fill:c.fill,family:c.fontFamily}};document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify(result);}catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack;}</script>`);
    const page=await browser.newPage(); await page.goto(pathToFileURL(file).href); await page.waitForSelector('pre#result,pre#error',{timeout:60000});
    assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());
    const row={name:`${name}-${theme}`,...JSON.parse(await page.locator('#result').textContent())};
    rows.push(row);writeFileSync(resolve(directory,`info-${row.name}.svg`),row.svg);await page.close();
  }
  writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
} finally { await browser.close(); }
console.log(`Captured ${cases.length*2} Mermaid Info SVGs.`);
