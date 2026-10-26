import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
const original=JSON.parse(readFileSync(process.argv[2]??'build/mermaid-cpp/mindmap-browser/shapes.json'));
const directory=resolve(process.argv[3]??'build/mermaid-cpp/pie-svg');
const cases=original.map(row=>({...row,native:readFileSync(resolve(directory,'mindmap-'+row.name+'.svg'),'utf8')}));
const html=`<!doctype html><meta charset="utf-8"><body><script>
const result=[];
for(const row of ${JSON.stringify(cases)}) {
 const host=document.createElement('div');host.innerHTML=row.native;document.body.append(host);
 const root=host.querySelector('svg'),view=root.viewBox.baseVal;
 const nodes=Array.from(root.querySelectorAll('g.mindmap-node')).map(n=>{
  const transform=n.transform.baseVal.consolidate().matrix,b=n.getBBox(),shape=n.querySelector('.label-container'),text=n.querySelector('text');
  return {position:{x:transform.e,y:transform.f},bounds:{x:b.x,y:b.y,width:b.width,height:b.height},fill:getComputedStyle(shape).fill,color:getComputedStyle(text).fill,text:text.textContent};
 });
 const edges=Array.from(root.querySelectorAll('.edgePaths path')).map(e=>({path:e.getAttribute('d'),stroke:getComputedStyle(e).stroke,width:parseFloat(getComputedStyle(e).strokeWidth)}));
 result.push({name:row.name,nodes,edges,view:{x:view.x,y:view.y,width:view.width,height:view.height}});host.remove();
}
document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'result',textContent:JSON.stringify(result)}));
</script>`;
const file=resolve(dirname(process.argv[2]??'build/mermaid-cpp/mindmap-browser/shapes.json'),'native-probe.html');writeFileSync(file,html);
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
let rows;
try{const page=await browser.newPage({viewport:{width:1600,height:1200}});await page.goto(pathToFileURL(file).href);await page.waitForSelector('#result');rows=JSON.parse(await page.locator('#result').textContent());}finally{await browser.close();}
const near=(a,b,name)=>assert(Math.abs(a-b)<=1.5,`${name}: ${a} != ${b}`);
const numbers=s=>(s.match(/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi)??[]).map(Number);
for(let i=0;i<rows.length;++i) {
 const a=rows[i],e=original[i];assert.equal(a.nodes.length,e.nodes.length,e.name);assert.equal(a.edges.length,e.edges.length,e.name);
 for(const key of ['x','y','width','height'])near(a.view[key],e.view[key],e.name+' view '+key);
 for(let j=0;j<a.nodes.length;++j) {
  const n=a.nodes[j],want=e.nodes[j];for(const key of ['x','y'])near(n.position[key],want.position[key],e.name+' position '+j+' '+key);
  for(const key of ['x','y','width','height'])near(n.bounds[key],want.nodeBox[key],e.name+' bounds '+j+' '+key);
  assert.equal(n.fill,want.fill,e.name+' node fill');assert.equal(n.color,want.textColor,e.name+' label fill');assert.equal(n.text,want.labelText,e.name+' label text');
 }
 for(let j=0;j<a.edges.length;++j) {
  assert.equal(a.edges[j].stroke,e.edges[j].stroke,e.name+' edge color');assert.equal(a.edges[j].width,e.edges[j].width,e.name+' edge width');
  const actual=numbers(a.edges[j].path),expected=numbers(e.edges[j].path);assert.equal(actual.length,expected.length,e.name+' edge path');
  for(let k=0;k<actual.length;++k)near(actual[k],expected[k],e.name+' edge point');
 }
}
console.log(`Mindmap: ${rows.length} measured native SVGs match original geometry, colors, labels and edge paths (1.5px native font policy).`);
