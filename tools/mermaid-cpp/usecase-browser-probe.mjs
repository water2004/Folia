import assert from 'node:assert/strict';
import { mkdirSync,writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './usecase-cases.mjs';
export const edge=process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';

export function inspectUsecase(){
 const svg=document.querySelector('svg'),v=svg.viewBox.baseVal,inv=svg.getScreenCTM().inverse();
 const box=e=>{const b=e.getBBox(),m=inv.multiply(e.getScreenCTM()),p=[[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]].map(([x,y])=>({x:m.a*x+m.c*y+m.e,y:m.b*x+m.d*y+m.f})),xs=p.map(a=>a.x),ys=p.map(a=>a.y);return{x:Math.min(...xs),y:Math.min(...ys),width:Math.max(...xs)-Math.min(...xs),height:Math.max(...ys)-Math.min(...ys)};};
 const labels=[...svg.querySelectorAll('text,foreignObject')].filter(e=>e.textContent.trim()).map(e=>{
   const label=e.querySelector('span')??e,style=getComputedStyle(label);
   const runs=[];
   const walker=document.createTreeWalker(label,NodeFilter.SHOW_TEXT);
   let node;
   while(node=walker.nextNode()) {
     const {fontWeight:weight,fontStyle:slant}=getComputedStyle(node.parentElement);
     const last=runs.at(-1);
     if(last?.weight===weight&&last?.slant===slant) last.text+=node.textContent;
     else runs.push({text:node.textContent,weight,slant});
   }
   return {text:(label.innerText??e.textContent).trim().replace(/\s+/g,' '),box:box(e),size:style.fontSize,
     fill:e.tagName==='text'?style.fill:style.color,tag:e.tagName,
     runs:runs.map(run=>({...run,text:run.text.trim().replace(/\s+/g,' ')})).filter(run=>run.text)};
 });
 const paint=e=>{const style=getComputedStyle(e);return {fill:style.fill,stroke:style.stroke,
   strokeWidth:style.strokeWidth,shadow:style.filter!=='none'};};
 const roughPaths=e=>[...e.querySelectorAll('path')].map(path=>({d:path.getAttribute('d'),paint:paint(path)}));
 const nodes=[...svg.querySelectorAll('g.node,g.rough-node')].map(e=>{
   const kind=e.getAttribute('data-usecase-kind');
   const rough=e.classList.contains('rough-node');
   const shapes=rough?[]:kind==='actor'?[...e.querySelectorAll('.usecase-actor-glyph > path:not(.usecase-business-marker), .usecase-actor-glyph > circle, .usecase-actor-glyph > rect')]:
     kind==='note'?[]:[e.querySelector(':scope > ellipse, :scope > rect')].filter(Boolean);
   return {id:e.getAttribute('data-usecase-id'),box:box(e),shape:e.querySelector('ellipse,rect,path')?.tagName,
     paint:shapes.map(paint),paths:rough?roughPaths(e):[]};
 });
 const boundaries=[...svg.querySelectorAll('g.system-boundary')].map(e=>({id:e.getAttribute('data-usecase-id'),box:box(e),
   parts:[...e.querySelectorAll(':scope > .boundary-tab, :scope > .boundary-body')].map(shape=>({
     box:box(shape),paint:shape.tagName==='rect'?paint(shape):null,paths:roughPaths(shape),
   }))}));
 const marker=ref=>{const id=(ref||'').match(/#([^)]*)/)?.[1];if(!id)return null;const m=svg.querySelector(`#${CSS.escape(id)}`);if(!m)return null;const s=m.querySelector('path,circle,polygon');return{width:+m.getAttribute('markerWidth'),height:+m.getAttribute('markerHeight'),refX:+m.getAttribute('refX'),refY:+m.getAttribute('refY'),shape:s?.tagName,d:s?.getAttribute('d')||s?.getAttribute('points')||'',r:s?.getAttribute('r')||''};};
 const edges=[...svg.querySelectorAll('path[data-et="edge"]')].map(e=>({d:e.getAttribute('d'),stroke:getComputedStyle(e).stroke,dash:getComputedStyle(e).strokeDasharray,markerStart:marker(e.getAttribute('marker-start')),markerEnd:marker(e.getAttribute('marker-end'))}));
 return{view:{x:v.x,y:v.y,width:v.width,height:v.height},labels,nodes,boundaries,edges};
}
if (import.meta.main) {
 const dir=resolve(process.argv[2]??'build/mermaid-cpp/usecase-browser');mkdirSync(dir,{recursive:true});const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
 const browser=await chromium.launch({executablePath:edge,headless:true,args:['--allow-file-access-from-files']});const rows=[];
 try{for(const theme of ['default','dark'])for(const item of cases){const page=await browser.newPage();const config={startOnLoad:false,theme,fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic',layout:'dagre'};const html=resolve(dir,'probe.html');writeFileSync(html,`<!doctype html><meta charset="utf-8"><body><script type="module">const {default:mermaid}=await import(${JSON.stringify(url)});try{mermaid.initialize(${JSON.stringify(config)});const {svg}=await mermaid.render('mermaid-usecase',${JSON.stringify(item.source)});document.body.innerHTML=svg;const observation=(${inspectUsecase.toString()})();document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify({svg,...observation});}catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.message;}</script>`);await page.goto(pathToFileURL(html).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});const name=`${item.name}-${theme}`;if(item.failure){assert.equal(await page.locator('#error').count(),1,`${name} should fail`);rows.push({name,source:item.source,theme,error:await page.locator('pre').textContent()});}else{assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());const row={name,source:item.source,theme,...JSON.parse(await page.locator('pre').textContent())};rows.push(row);writeFileSync(resolve(dir,`usecase-${name}.svg`),row.svg);}await page.close();}}finally{await browser.close();}
 writeFileSync(resolve(dir,'original.json'),JSON.stringify(rows,null,2));console.log(`Captured ${rows.length} Usecase browser probes in both themes.`);
}
