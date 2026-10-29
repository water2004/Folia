import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync,readFileSync,readdirSync} from 'node:fs';
import vm from 'node:vm';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases} from './xy-cases.mjs';
export function inspectXY() {
 const svg=document.querySelector('svg'),v=svg.viewBox.baseVal;
 return {view:{x:v.x,y:v.y,width:v.width,height:v.height},elements:[...svg.querySelectorAll('g.main rect,g.main path,g.main text')].map(e=>{
  const b=e.getBBox(),s=getComputedStyle(e),m=e.getCTM();
  return {type:e.tagName,groups:[e.parentElement.parentElement.getAttribute('class'),e.parentElement.getAttribute('class')],
   attrs:Object.fromEntries([...e.attributes].map(a=>[a.name,a.value])),text:e.textContent,
   box:{x:b.x,y:b.y,width:b.width,height:b.height},transform:[m.a,m.b,m.c,m.d,m.e,m.f],
   paint:{fill:s.fill,stroke:s.stroke,strokeWidth:s.strokeWidth,fontSize:s.fontSize,fontFamily:s.fontFamily}};
 })};
}
if(!process.argv.includes('--library')) {
 const directory=resolve(process.argv[2]??'build/mermaid-cpp/xy-browser');mkdirSync(directory,{recursive:true});
 const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
 const successes=cases.filter(c=>c[2]!==null);
 // The layout gate uses the same fixed cases and original font-box inputs.
 // Extract original d3 ticks rather than reproducing native scale code here.
 const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.esm/',import.meta.url);
 let tickSource;for(const name of readdirSync(chunks).filter(n=>n.endsWith('.mjs.map'))){const map=JSON.parse(readFileSync(new URL(name,chunks)));const index=map.sources.findIndex(s=>s.includes('/d3-array@3.2.4/')&&s.endsWith('/src/ticks.js'));if(index>=0){tickSource=map.sourcesContent[index];break;}}
 assert(tickSource);const ticks=vm.runInNewContext(tickSource.replace('export default function ticks','function ticks').replaceAll('export function','function')+'\nticks');
 const decode=s=>Buffer.from(s,'hex').swap16().toString('utf16le');
 const requests={};for(const line of readFileSync(new URL('../../tests/mermaid_cpp/fixtures/xy.tsv',import.meta.url),'utf8').trim().split('\n').filter(s=>!s.startsWith('#'))){const [name,,,encoded]=line.split('\t'),result=JSON.parse(decode(encoded));if(!result)continue;const {data,config:c}=result;const axis=(a,left,size)=>(a.type==='band'?a.categories:ticks(left?a.max:a.min,left?a.min:a.max,10).map(String)).map(text=>({text,font_size:size}));requests[name]=[{text:result.title,font_size:c.titleFontSize},{text:data.xAxis.title,font_size:c.xAxis.titleFontSize},{text:data.yAxis.title,font_size:c.yAxis.titleFontSize},...axis(data.xAxis,c.chartOrientation==='horizontal',c.xAxis.labelFontSize),...axis(data.yAxis,c.chartOrientation!=='horizontal',c.yAxis.labelFontSize),...data.plots.filter(p=>p.title).map(p=>({text:p.title,font_size:c.legendFontSize}))];}
 const html=`<!doctype html><meta charset="utf-8"><body><script type="module">
 const {default:mermaid}=await import(${JSON.stringify(url)});const inspect=${inspectXY.toString()},requests=${JSON.stringify(requests)};
 try{const rows=[];for(const [name,source] of ${JSON.stringify(successes)})for(const theme of ['default','dark']) {
 mermaid.initialize({startOnLoad:false,theme,fontFamily:'Arial'});
 const {svg}=await mermaid.render('xy'+rows.length,source);const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);
 const observation=inspect(),svgNode=host.querySelector('svg'),ns='http://www.w3.org/2000/svg';
 const measurements=requests[name].map(request=>{const g=document.createElementNS(ns,'g');g.setAttribute('font-size',request.font_size);g.setAttribute('visibility','hidden');svgNode.append(g);const text=document.createElementNS(ns,'text'),span=document.createElementNS(ns,'tspan');span.setAttribute('dy','1em');span.textContent=request.text;text.append(span);g.append(text);const box=span.getBoundingClientRect(),ctm=g.getScreenCTM();const result={width:box.width/ctm.a,height:box.height/ctm.d};g.remove();return result;});
 rows.push({name:name+'-'+theme,source,theme,svg,...observation,requests:requests[name],measurements});host.remove();}
 document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify(rows);
 }catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack;}</script>`;
 const file=resolve(directory,'probe.html');writeFileSync(file,html);
 const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
 let rows;
 try{const page=await browser.newPage();await page.goto(pathToFileURL(file).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());rows=JSON.parse(await page.locator('#result').textContent());}finally{await browser.close();}
 assert.equal(rows.length,successes.length*2);writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
 const hex=s=>Buffer.from(s,'utf16le').swap16().toString('hex');
 writeFileSync(resolve(directory,'measurements.tsv'),rows.map(row=>[row.name,hex(row.source),hex(row.theme),hex(JSON.stringify(row.requests)),hex(JSON.stringify(row.measurements))].join('\t')).join('\n')+'\n');
 for(const row of rows)writeFileSync(resolve(directory,row.name+'.svg'),row.svg);
 console.log(`Captured ${rows.length} original XY SVGs in ${directory}; native port pending.`);
}
