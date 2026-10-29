import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {line,curveBasis} from './oracle/node_modules/d3-shape/src/index.js';
const original=JSON.parse(readFileSync(process.argv[2]??'build/mermaid-cpp/mindmap-browser/shapes.json'));
const directory=resolve(process.argv[3]??'build/mermaid-cpp/pie-svg');
const cases=original.flatMap(row=>['','measured-'].map(prefix=>({...row,measured:!!prefix,native:readFileSync(resolve(directory,'mindmap-'+prefix+row.name+'.svg'),'utf8')})));
const html=`<!doctype html><meta charset="utf-8"><body><script>
const result=[];
for(const row of ${JSON.stringify(cases)}) {
 const host=document.createElement('div');host.innerHTML=row.native;document.body.append(host);
 const root=host.querySelector('svg'),view=root.viewBox.baseVal;
 const nodes=Array.from(root.querySelectorAll('g.mindmap-node')).map(n=>{
  const transform=n.transform.baseVal.consolidate().matrix,b=n.getBBox(),shape=n.querySelector('.label-container'),text=n.querySelector('text');
  return {position:{x:transform.e,y:transform.f},bounds:{x:b.x,y:b.y,width:b.width,height:b.height},fill:getComputedStyle(shape).fill,color:getComputedStyle(text).fill,text:text.textContent};
 });
 const edges=Array.from(root.querySelectorAll('.edgePaths path')).map(e=>({id:e.id,path:e.getAttribute('d'),stroke:getComputedStyle(e).stroke,width:parseFloat(getComputedStyle(e).strokeWidth)}));
 const bounds=root.getBBox();
 result.push({name:row.name,measured:row.measured,nodes,edges,bounds:{x:bounds.x,y:bounds.y,width:bounds.width,height:bounds.height},view:{x:view.x,y:view.y,width:view.width,height:view.height}});host.remove();
}
document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'result',textContent:JSON.stringify(result)}));
</script>`;
const file=resolve(dirname(process.argv[2]??'build/mermaid-cpp/mindmap-browser/shapes.json'),'native-probe.html');writeFileSync(file,html);
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
let rows;
try{const page=await browser.newPage({viewport:{width:1600,height:1200}});await page.goto(pathToFileURL(file).href);await page.waitForSelector('#result');rows=JSON.parse(await page.locator('#result').textContent());}finally{await browser.close();}
const near=(a,b,name)=>assert(Math.abs(a-b)<=1.5,`${name}: ${a} != ${b}`);
const numbers=s=>(s.match(/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi)??[]).map(Number);
const strict=(a,b,name,tolerance=.0001)=>assert(Math.abs(a-b)<=tolerance,`${name}: ${a} != ${b}`);
for(let i=0;i<rows.length;++i) {
 const a=rows[i],e=original.find(row=>row.name===a.name);assert.equal(a.nodes.length,e.nodes.length,e.name);assert.equal(a.edges.length,e.edges.length,e.name);
 if(a.measured)for(const key of ['x','y','width','height'])strict(a.view[key],e.view[key],e.name+' measured view '+key);
 // The production measurement path must enclose its own measured geometry.
 // Force layout positions are compared strictly only with identical boxes.
 for(const key of ['x','y'])strict(a.view[key],a.bounds[key]-10,e.name+' viewport padding '+key);
 for(const key of ['width','height'])strict(a.view[key],a.bounds[key]+20,e.name+' viewport extent '+key);
 for(let j=0;j<a.nodes.length;++j) {
  const n=a.nodes[j],want=e.nodes[j];if(a.measured)for(const key of ['x','y'])strict(n.position[key],want.position[key],e.name+' measured position '+j+' '+key);
  for(const key of ['x','y','width','height'])near(n.bounds[key],want.nodeBox[key],e.name+' bounds '+j+' '+key);
  assert.equal(n.fill,want.fill,e.name+' node fill');assert.equal(n.color,want.textColor,e.name+' label fill');assert.equal(n.text,want.labelText,e.name+' label text');
 }
 for(let j=0;j<a.edges.length;++j) {
  assert.equal(a.edges[j].stroke,e.edges[j].stroke,e.name+' edge color');assert.equal(a.edges[j].width,e.edges[j].width,e.name+' edge width');
  const link=e.links[j];assert.equal(a.edges[j].id,link.id,e.name+' edge identity');
  const s=a.nodes[Number(link.source)].position,t=a.nodes[Number(link.target)].position;
  const dx=t.x-s.x,dy=t.y-s.y,length=Math.hypot(dx,dy);
  // The original consumer clips default Cytoscape 30px ellipse endpoints,
  // then passes the midpoint route through D3 curveBasis.
  const routed=line().curve(curveBasis)([[s.x+15*dx/length,s.y+15*dy/length],[(s.x+t.x)/2,(s.y+t.y)/2],[t.x-15*dx/length,t.y-15*dy/length]]);
  const actual=numbers(a.edges[j].path),expected=numbers(a.measured?e.edges[j].path:routed);assert.equal(actual.length,expected.length,e.name+' edge path');
  for(let k=0;k<actual.length;++k)strict(actual[k],expected[k],e.name+' edge point',.001);
 }
}
console.log(`Mindmap: ${original.length} identical-box SVG layouts/routes/viewports match original; ${original.length} DirectWrite SVGs match local shapes/labels (1.5px), paint, connected routes and viewport enclosure.`);
