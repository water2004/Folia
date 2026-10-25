import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases} from './radar-cases.mjs';
export function inspectRadar() {
 const svg=document.querySelector('svg'),v=svg.viewBox.baseVal;
 const matrix=e=>svg.getCTM().inverse().multiply(e.getCTM());
 const transform=e=>{const m=matrix(e);return [m.a,m.b,m.c,m.d,m.e,m.f];};
 const paint=e=>{const s=getComputedStyle(e);return {fill:s.fill,fillOpacity:s.fillOpacity,stroke:s.stroke,strokeWidth:s.strokeWidth};};
 const shapes=[...svg.querySelectorAll('circle,polygon,path,line,rect')].map(e=>{
  const data={kind:e.tagName,...paint(e),transform:transform(e)};
  if(e.tagName==='path'){data.samples=Array.from({length:25},(_,i)=>{const p=e.getPointAtLength(e.getTotalLength()*i/24);return [p.x,p.y];});}
  else if(e.tagName==='polygon')data.points=Array.from({length:e.points.numberOfItems},(_,i)=>{const p=e.points.getItem(i);return [p.x,p.y];});
  else for(const a of e.tagName==='circle'?['cx','cy','r']:e.tagName==='line'?['x1','y1','x2','y2']:['x','y','width','height'])data[a]=e[a].baseVal.value;
  return data;
 });
 const texts=[...svg.querySelectorAll('text')].map(e=>{const b=e.getBBox(),s=getComputedStyle(e);return {text:e.textContent,x:b.x,y:b.y,width:b.width,height:b.height,transform:transform(e),fill:s.fill,fontSize:s.fontSize,fontFamily:s.fontFamily,anchor:s.textAnchor,baseline:s.dominantBaseline};});
 return {view:{x:v.x,y:v.y,width:v.width,height:v.height},shapes,texts};
}
if(!process.argv.includes('--library')) {
 const directory=resolve(process.argv[2]??'build/mermaid-cpp/radar-browser');mkdirSync(directory,{recursive:true});
 const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href,successes=cases.filter(c=>c[2]!==null);
 const html=`<!doctype html><body><script type="module">
 const {default:mermaid}=await import(${JSON.stringify(url)});const inspect=${inspectRadar.toString()};
 try{const rows=[];for(const [name,source] of ${JSON.stringify(successes)})for(const theme of ['default','dark']) {
 mermaid.initialize({startOnLoad:false,theme,fontFamily:'Arial'});const {svg}=await mermaid.render('radar'+rows.length,source);
 const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);rows.push({name:name+'-'+theme,source,theme,svg,...inspect()});host.remove();}
 document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify(rows);
 }catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack;}</script>`;
 const file=resolve(directory,'probe.html');writeFileSync(file,html);
 const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});let rows;
 try{const page=await browser.newPage();await page.goto(pathToFileURL(file).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());rows=JSON.parse(await page.locator('#result').textContent());}finally{await browser.close();}
 assert.equal(rows.length,successes.length*2);writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));for(const row of rows)writeFileSync(resolve(directory,row.name+'.svg'),row.svg);
 console.log(`Captured ${rows.length} original Radar SVGs in ${directory}`);
}
