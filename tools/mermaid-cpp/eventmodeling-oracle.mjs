import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases,invalid} from './eventmodeling-cases.mjs';
import {edge} from './venn-browser-probe.mjs';
const dir=resolve(process.argv[2]??'build/mermaid-cpp/eventmodeling-browser');mkdirSync(dir,{recursive:true});
const mermaid=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
const html=resolve(dir,'probe.html');writeFileSync(html,'<!doctype html><meta charset="utf-8"><body></body>');
const browser=await chromium.launch({executablePath:edge,headless:true,args:['--allow-file-access-from-files']});
try{const page=await browser.newPage();await page.goto(pathToFileURL(html).href);await page.evaluate(async url=>{globalThis.mermaid=(await import(url)).default;},mermaid);const result=await page.evaluate(async({cases,invalid})=>{const out=[];for(const theme of ['default','dark']){mermaid.initialize({startOnLoad:false,fontFamily:'Arial',look:'classic',theme});for(const[name,source]of cases){const {svg}=await mermaid.render('eventmodeling-'+name+'-'+theme,source);out.push({name,theme,svg});}for(const source of invalid)if(await mermaid.parse(source,{suppressErrors:true}))throw new Error('Invalid source parsed: '+source);}return out;},{cases,invalid});assert.equal(result.length,cases.length*2);writeFileSync(resolve(dir,'original.json'),JSON.stringify(result,null,2));for(const r of result)writeFileSync(resolve(dir,`eventmodeling-${r.name}-${r.theme}.svg`),r.svg);console.log(`Captured ${result.length} Mermaid 12.2.0 Event Modeling SVGs.`);}finally{await browser.close();}
