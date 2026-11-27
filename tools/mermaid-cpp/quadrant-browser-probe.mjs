import { browserExecutable } from './oracle/browser-executable.mjs';
import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases} from './quadrant-cases.mjs';
import inspectQuadrant from './oracle/observations/quadrant.mjs';
export {inspectQuadrant};
if(!process.argv.includes('--library')) {
 const directory=resolve(process.argv[2]??'build/mermaid-cpp/quadrant-browser');mkdirSync(directory,{recursive:true});
 const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
 const successes=cases.filter(c=>c[2]!==null);
 const html=`<!doctype html><meta charset="utf-8"><body><script type="module">
 const {default:mermaid}=await import(${JSON.stringify(url)});const inspect=${inspectQuadrant.toString()};
 try{const rows=[];for(const [name,source] of ${JSON.stringify(successes)})for(const theme of ['default','dark']) {
 mermaid.initialize({startOnLoad:false,theme,fontFamily:'Arial'});
 const {svg}=await mermaid.render('quadrant'+rows.length,source);const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);
 rows.push({name:name+'-'+theme,source,theme,svg,...inspect()});host.remove();}
 document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify(rows);
 }catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack;}</script>`;
 const file=resolve(directory,'probe.html');writeFileSync(file,html);
 const browser=await chromium.launch({executablePath:browserExecutable(),headless:true,args:['--allow-file-access-from-files']});
 let rows;
 try{const page=await browser.newPage();await page.goto(pathToFileURL(file).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());rows=JSON.parse(await page.locator('#result').textContent());}finally{await browser.close();}
 assert.equal(rows.length,successes.length*2);writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
 for(const row of rows)writeFileSync(resolve(directory,row.name+'.svg'),row.svg);
 console.log(`Captured ${rows.length} original Quadrant SVGs in ${directory}`);
}
