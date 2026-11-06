// The existing Architecture corpus through its original browser renderer.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync,readdirSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases as allCases} from './architecture-cases.mjs';
const cases=allCases.filter(row=>!row[3]),directory=resolve(process.argv[2]??'build/mermaid-cpp/architecture-browser');mkdirSync(directory,{recursive:true});
const chunks='tools/mermaid-cpp/oracle/node_modules/mermaid/dist/chunks/mermaid.core/';
const map=JSON.parse(readFileSync(chunks+readdirSync(chunks).find(n=>n.endsWith('.mjs.map')&&readFileSync(chunks+n,'utf8').includes('/architectureRenderer.ts'))));
for(const name of ['architectureRenderer.ts','svgDraw.ts','architectureIcons.ts','architectureStyles.ts']) {
 const i=map.sources.findIndex(n=>n.endsWith('/architecture/'+name));assert(i>=0,name);
 assert.equal(map.sourcesContent[i].replaceAll('\r\n','\n'),readFileSync('.reference/mermaid-js/packages/mermaid/src/diagrams/architecture/'+name,'utf8').replaceAll('\r\n','\n'),name);
}
const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
const html=`<!doctype html><meta charset="utf-8"><body><script type="module">
try {
 const {default:mermaid}=await import(${JSON.stringify(url)}),rows=[];
 const box=e=>{const b=e.getBBox();return {x:b.x,y:b.y,width:b.width,height:b.height};};
 const position=e=>{const m=e.transform.baseVal.consolidate()?.matrix;return m?{x:m.e,y:m.f}:{x:0,y:0};};
 for(const [name,source] of ${JSON.stringify(cases)})for(const theme of ['default','dark']) {
  mermaid.initialize({startOnLoad:false,theme,look:'classic',fontFamily:'Arial',themeVariables:{fontFamily:'Arial'}});
  const {svg}=await mermaid.render('architecture'+rows.length,source);const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);const root=host.querySelector('svg'),view=root.viewBox.baseVal;
  const services=[...root.querySelectorAll('.architecture-service,.architecture-junction')].map(n=>({id:n.id,position:position(n),box:box(n)}));
  const groups=[...root.querySelectorAll('.architecture-groups>rect')].map(n=>({id:n.id,box:box(n),stroke:getComputedStyle(n).stroke,width:parseFloat(getComputedStyle(n).strokeWidth)}));
  const edges=[...root.querySelectorAll('path.edge')].map(n=>({id:n.id,path:n.getAttribute('d'),stroke:getComputedStyle(n).stroke,width:parseFloat(getComputedStyle(n).strokeWidth)}));
  const arrows=[...root.querySelectorAll('polygon.arrow')].map(n=>({points:n.getAttribute('points'),transform:n.getAttribute('transform'),fill:getComputedStyle(n).fill}));
  const globalBox=e=>{const b=e.getBBox(),m=root.getCTM().inverse().multiply(e.getCTM()),p=new DOMPoint(b.x,b.y).matrixTransform(m);return {x:p.x,y:p.y,width:b.width,height:b.height};};
  const iconTexts=[...root.querySelectorAll('.node-icon-text')].map(n=>{const range=document.createRange();range.selectNodeContents(n);const b=range.getBoundingClientRect(),m=root.getScreenCTM().inverse(),p=new DOMPoint(b.x,b.y).matrixTransform(m),q=new DOMPoint(b.right,b.bottom).matrixTransform(m);return {text:n.textContent,fill:getComputedStyle(n.firstElementChild).color,global:{x:p.x,y:p.y,width:q.x-p.x,height:q.y-p.y}};});
  const labels=[...root.querySelectorAll('text')].map(n=>({text:n.textContent,box:box(n),global:globalBox(n),font:getComputedStyle(n).fontFamily,size:parseFloat(getComputedStyle(n).fontSize),fill:getComputedStyle(n).fill,html:n.outerHTML,parent:n.parentElement.getAttribute('transform')}));
  const {db}=await mermaid.mermaidAPI.getDiagramFromText(source),canvas=document.createElement('canvas').getContext('2d');
  const cyMeasure=(text,size)=>{canvas.font='normal normal '+size+'px Helvetica Neue, Helvetica, sans-serif';return {width:Math.max(0,...text.split('\\n').map(line=>Math.ceil(canvas.measureText(line).width))),height:text?size*text.split('\\n').length:0};};
  const cyLabels={nodes:db.getNodes().map(n=>cyMeasure(n.title??'',db.getConfigField('fontSize'))),edges:db.getEdges().map(e=>cyMeasure(e.title??'',16))};
  rows.push({name:name+'-'+theme,source,theme,services,groups,edges,arrows,labels,iconTexts,cyLabels,view:{x:view.x,y:view.y,width:view.width,height:view.height},svg});host.remove();
 }
 document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'result',textContent:JSON.stringify(rows)}));
}catch(e){document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'error',textContent:String(e.stack)}));}
</script>`;
const file=resolve(directory,'probe.html');writeFileSync(file,html);
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
try{const page=await browser.newPage({viewport:{width:1600,height:1200}});await page.goto(pathToFileURL(file).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});assert.equal(await page.locator('#error').count(),0,await page.locator('body').textContent());const result=JSON.parse(await page.locator('#result').textContent());assert.equal(result.length,12);writeFileSync(resolve(directory,'original.json'),JSON.stringify(result,null,2));const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');writeFileSync(resolve(directory,'measurements.tsv'),result.map(row=>row.name+'\t'+hex(JSON.stringify(row))).join('\n')+'\n');for(const row of result)writeFileSync(resolve(directory,row.name+'.svg'),row.svg);console.log('Architecture: original six-diagram SVG corpus in both themes recorded.');}finally{await browser.close();}
