import assert from 'node:assert/strict';
import { mkdirSync,writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './ishikawa-cases.mjs';

export function inspectIshikawa(){
 const svg=document.querySelector('svg'),view=svg.viewBox.baseVal,inv=svg.getScreenCTM().inverse();
 const box=e=>{const b=e.getBBox(),m=inv.multiply(e.getScreenCTM()),pts=[[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]].map(([x,y])=>({x:m.a*x+m.c*y+m.e,y:m.b*x+m.d*y+m.f})),xs=pts.map(p=>p.x),ys=pts.map(p=>p.y);return{x:Math.min(...xs),y:Math.min(...ys),width:Math.max(...xs)-Math.min(...xs),height:Math.max(...ys)-Math.min(...ys)};}; const lines=[...svg.querySelectorAll('.ishikawa-spine,.ishikawa-branch,.ishikawa-sub-branch')].map(e=>({cls:e.getAttribute('class'),x1:+e.getAttribute('x1'),y1:+e.getAttribute('y1'),x2:+e.getAttribute('x2'),y2:+e.getAttribute('y2'),marker:e.hasAttribute('marker-start')}));
 const labels=[...svg.querySelectorAll('text')].map(e=>({text:e.textContent.trim().replace(/\s+/g,' '),cls:e.getAttribute('class'),box:box(e),fontSize:getComputedStyle(e).fontSize,fill:getComputedStyle(e).fill}));
 const shapes=[...svg.querySelectorAll('.ishikawa-head,.ishikawa-label-box')].map(e=>({cls:e.getAttribute('class'),box:box(e),fill:getComputedStyle(e).fill,stroke:getComputedStyle(e).stroke}));
 return{view:{x:view.x,y:view.y,width:view.width,height:view.height},lines,labels,shapes};
}
if(!process.argv.includes('--library')){
 const dir=resolve(process.argv[2]??'build/mermaid-cpp/ishikawa-browser');mkdirSync(dir,{recursive:true});
 const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
 const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});const rows=[];
 try{for(const theme of ['default','dark'])for(const [name,source] of cases){const page=await browser.newPage();const config={startOnLoad:false,theme,fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic'};const html=resolve(dir,'probe.html');writeFileSync(html,`<!doctype html><meta charset="utf-8"><body><script type="module">const {default:mermaid}=await import(${JSON.stringify(url)});try{mermaid.initialize(${JSON.stringify(config)});const {svg}=await mermaid.render('mermaid-ishikawa',${JSON.stringify(source)});document.body.innerHTML=svg;const observation=(${inspectIshikawa.toString()})();document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify({svg,...observation});}catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.message;}</script>`);await page.goto(pathToFileURL(html).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});const suffix=`${name}-${theme}`;if(name.startsWith('invalid')){assert.equal(await page.locator('#error').count(),1,`${suffix} should fail in upstream parser`);rows.push({name:suffix,source,theme,error:await page.locator('pre').textContent()});}else{assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());const row={name:suffix,source,theme,...JSON.parse(await page.locator('#result').textContent())};rows.push(row);writeFileSync(resolve(dir,`ishikawa-${suffix}.svg`),row.svg);}await page.close();}}finally{await browser.close();}
 writeFileSync(resolve(dir,'original.json'),JSON.stringify(rows,null,2));console.log(`Captured ${rows.length} Ishikawa browser probes in both themes.`);
}
