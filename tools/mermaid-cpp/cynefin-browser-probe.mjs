import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './cynefin-cases.mjs';

export function inspectCynefin() {
  const svg=document.querySelector('svg'),view=svg.viewBox.baseVal;
  const inverse=svg.getScreenCTM().inverse();
  const box=e=>{const b=e.getBBox(),m=inverse.multiply(e.getScreenCTM());const points=[[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]].map(([x,y])=>({x:m.a*x+m.c*y+m.e,y:m.b*x+m.d*y+m.f}));const left=Math.min(...points.map(p=>p.x)),top=Math.min(...points.map(p=>p.y)),right=Math.max(...points.map(p=>p.x)),bottom=Math.max(...points.map(p=>p.y));return{x:left,y:top,width:right-left,height:bottom-top}};
  const paths=[...svg.querySelectorAll('path')].filter(e=>!e.closest('defs')).map(e=>({
    cls:e.getAttribute('class')??'',d:e.getAttribute('d')??'',fill:getComputedStyle(e).fill,
    stroke:getComputedStyle(e).stroke,width:parseFloat(getComputedStyle(e).strokeWidth),
    box:box(e)
  }));
  const rects=[...svg.querySelectorAll('rect')].map(e=>({cls:e.getAttribute('class')??'',...box(e),fill:getComputedStyle(e).fill}));
  const labels=[...svg.querySelectorAll('text')].filter(e=>!e.closest('defs')).map(e=>{const b=box(e),s=getComputedStyle(e);return{text:e.textContent,...b,fontSize:s.fontSize,weight:s.fontWeight,fill:s.fill}});
  return{view:{x:view.x,y:view.y,width:view.width,height:view.height},paths,rects,labels};
}
if(!process.argv.includes('--library')) {
  const directory=resolve(process.argv[2]??'build/mermaid-cpp/cynefin-browser');mkdirSync(directory,{recursive:true});
  const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const valid=cases.filter(c=>c[2]===null);const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});const rows=[];
  try{for(const [name,source] of valid)for(const theme of ['default','dark']){
    const config={startOnLoad:false,theme,fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic',cynefin:{seed:31}};
    const file=resolve(directory,'probe.html');writeFileSync(file,`<!doctype html><meta charset="utf-8"><body><script type="module">const {default:mermaid}=await import(${JSON.stringify(url)});try{mermaid.initialize(${JSON.stringify(config)});const {svg}=await mermaid.render('mermaid-cynefin',${JSON.stringify(source)});document.body.innerHTML=svg;const observation=(${inspectCynefin.toString()})();document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify({svg,...observation})}catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack}</script>`);
    const page=await browser.newPage();await page.goto(pathToFileURL(file).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());const row={name:`${name}-${theme}`,source,theme,...JSON.parse(await page.locator('#result').textContent())};rows.push(row);writeFileSync(resolve(directory,`cynefin-${row.name}.svg`),row.svg);await page.close();
  }}finally{await browser.close()}
  writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));console.log(`Captured ${rows.length} Cynefin SVG baselines from pinned Mermaid 12.2.0.`);
}
