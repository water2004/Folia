import { browserExecutable } from './oracle/browser-executable.mjs';
import assert from 'node:assert/strict';
import { mkdirSync,writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './usecase-cases.mjs';
export const edge=browserExecutable();

import inspectUsecase from './oracle/observations/usecase.mjs';
export {inspectUsecase};
if (import.meta.main) {
 const dir=resolve(process.argv[2]??'build/mermaid-cpp/usecase-browser');mkdirSync(dir,{recursive:true});const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
 const browser=await chromium.launch({executablePath:edge,headless:true,args:['--allow-file-access-from-files']});const rows=[];
 try{for(const theme of ['default','dark'])for(const item of cases){const page=await browser.newPage();const config={startOnLoad:false,theme,fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic',layout:'dagre'};const html=resolve(dir,'probe.html');writeFileSync(html,`<!doctype html><meta charset="utf-8"><body><script type="module">const {default:mermaid}=await import(${JSON.stringify(url)});try{mermaid.initialize(${JSON.stringify(config)});const {svg}=await mermaid.render('mermaid-usecase',${JSON.stringify(item.source)});document.body.innerHTML=svg;const observation=(${inspectUsecase.toString()})();document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify({svg,...observation});}catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.message;}</script>`);await page.goto(pathToFileURL(html).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});const name=`${item.name}-${theme}`;if(item.failure){assert.equal(await page.locator('#error').count(),1,`${name} should fail`);rows.push({name,source:item.source,theme,error:await page.locator('pre').textContent()});}else{assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());const row={name,source:item.source,theme,...JSON.parse(await page.locator('pre').textContent())};rows.push(row);writeFileSync(resolve(dir,`usecase-${name}.svg`),row.svg);}await page.close();}}finally{await browser.close();}
 writeFileSync(resolve(dir,'original.json'),JSON.stringify(rows,null,2));console.log(`Captured ${rows.length} Usecase browser probes in both themes.`);
}
