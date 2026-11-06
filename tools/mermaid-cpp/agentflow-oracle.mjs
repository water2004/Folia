import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './agentflow-cases.mjs';

const directory=resolve(process.argv[2]??'build/mermaid-cpp/agentflow-browser');mkdirSync(directory,{recursive:true});
const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
const captured=[];
function html(source,theme){return `<!doctype html><meta charset="utf-8"><body><script type="module">
 const {default:mermaid}=await import(${JSON.stringify(url)});
 try { mermaid.initialize({startOnLoad:false,theme:${JSON.stringify(theme)},fontFamily:'Arial',themeVariables:{fontFamily:'Arial'}});
 const {svg}=await mermaid.render('agentflow-probe',${JSON.stringify(source)});document.body.innerHTML=svg;document.body.setAttribute('data-agentflow-ready','true'); }
 catch(error){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('#error').textContent=error.stack;}
 </script>`;}
function inspect(svg){
 const translated=(el)=>{let x=0,y=0;for(let n=el;n&&n!==svg;n=n.parentElement){const t=n.getAttribute?.('transform')??'',v=t.match(/translate\(\s*(-?[\d.eE+-]+)(?:[ ,]+(-?[\d.eE+-]+))?\s*\)/);if(v){x+=Number(v[1]);y+=Number(v[2]??0);}}return{x,y};};
 const box=(el)=>{const b=el.getBBox(),t=translated(el);return{x:t.x+b.x,y:t.y+b.y,width:b.width,height:b.height};};
 const view=svg.viewBox.baseVal;
 return {view:{x:view.x,y:view.y,width:view.width,height:view.height},clusters:[...svg.querySelectorAll('g.cluster')].map(el=>({id:el.id,label:(el.querySelector('.cluster-label')?.textContent??'').trim().replace(/\s+/g,' '),box:box(el)})),nodes:[...svg.querySelectorAll('g.node')].map(el=>{const shape=el.querySelector('.label-container'),geometry=shape?.tagName.toLowerCase()==='g'?(shape.querySelector('path,polygon,rect')??shape):shape;const path=shape?.tagName.toLowerCase()==='path'?shape:shape?.querySelector('path'),length=path?.getTotalLength()??0,label=el.querySelector('foreignObject')??el.querySelector('.nodeLabel');let labelCenter=null;if(label){const b=box(label);labelCenter={x:b.x+b.width/2,y:b.y+b.height/2};}return{id:el.id,kind:el.getAttribute('class'),label:(el.querySelector('.nodeLabel')?.textContent??el.querySelector('.label')?.textContent??'').trim().replace(/\s+/g,' '),shape:shape?.tagName??'',box:geometry?box(geometry):null,shapeSamples:path?[0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1].map(t=>{const p=path.getPointAtLength(length*t),q=translated(path);return{x:p.x+q.x,y:p.y+q.y};}):[],labelCenter};}),edges:[...svg.querySelectorAll('path[data-edge="true"]')].map(el=>{const length=el.getTotalLength(),offset=(()=>{let x=0,y=0;for(let n=el;n&&n!==svg;n=n.parentElement){const v=(n.getAttribute?.('transform')??'').match(/translate\(\s*(-?[\d.eE+-]+)(?:[ ,]+(-?[\d.eE+-]+))?\s*\)/);if(v){x+=Number(v[1]);y+=Number(v[2]??0);}}return{x,y};})();return{id:el.getAttribute('data-id'),path:el.getAttribute('d')??'',points:JSON.parse(atob(el.getAttribute('data-points')??'W10=')),samples:[0,.25,.5,.75,1].map(t=>{const p=el.getPointAtLength(length*t);return{x:p.x+offset.x,y:p.y+offset.y};}),marker:el.hasAttribute('marker-end')};}),labels:[...svg.querySelectorAll('g.edgeLabels foreignObject')].map(el=>({text:(el.textContent??'').trim().replace(/\s+/g,' '),box:box(el)}))};
}
try{
 for(const [name,source] of cases)for(const theme of ['default','dark']){
  const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('console',m=>{if(m.type()==='error')errors.push(m.text());});
  const htmlPath=resolve(directory,'probe.html');writeFileSync(htmlPath,html(source,theme));
  try{await page.goto(pathToFileURL(htmlPath).href);await page.waitForSelector('body[data-agentflow-ready="true"],pre#error',{timeout:60000});if(await page.locator('#error').count())throw new Error(await page.locator('#error').textContent());
   const element=page.locator('svg').first(),svg=await element.evaluate(e=>e.outerHTML);assert(svg.includes('viewBox=')&&svg.includes('agentflowTitleText'),`${name}/${theme}: incomplete Mermaid Agentflow SVG (${errors.join('; ')})`);const observation=await element.evaluate(inspect);assert(observation.nodes.length>0,`${name}/${theme}: no rendered nodes (${errors.join('; ')})`);const row={name,theme,source,svg,...observation};captured.push(row);writeFileSync(resolve(directory,`agentflow-${name}-${theme}.svg`),svg);
  }finally{await page.close();}
 }
}finally{await browser.close();}
writeFileSync(resolve(directory,'original.json'),JSON.stringify(captured,null,2));console.log(`Captured ${captured.length} Mermaid 12.2.0 Agentflow SVG baselines with node and edge geometry.`);
