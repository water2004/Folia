// Measured Mindmap SVG observations. CoSE layout positions are independently
// verified against the isolated Cytoscape 3.34.0 consumer.
import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync,readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL,fileURLToPath} from 'node:url';
import {createRequire} from 'node:module';
import {createHash} from 'node:crypto';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases as allCases} from './mindmap-cases.mjs';
const cases=allCases.filter(([name])=>name==='shapes'||name==='labels');
const directory=resolve(process.argv[2]??'build/mermaid-cpp/mindmap-browser');
mkdirSync(directory,{recursive:true});
const require=createRequire(import.meta.url),isolated=new URL('../../build/mermaid-cpp/mindmap-layout-oracle/',import.meta.url);
assert.equal(createHash('sha512').update(readFileSync(new URL('cytoscape-3.34.0.tgz',isolated))).digest('base64'),'62rNSrioXw93uliKFBwjukeQyeWwH2PqDrTac31r2P6464u3AUvTk0xS4LVvT251g7IgkFunrI48ZEZGjywSOg==');
const cytoscape=require(fileURLToPath(new URL('package/dist/cytoscape.cjs.js',isolated)));
cytoscape.use(require('./oracle/node_modules/.pnpm/cytoscape-cose-bilkent@4.1.0_cytoscape@3.34.4/node_modules/cytoscape-cose-bilkent'));
assert.equal(JSON.parse(readFileSync('tools/mermaid-cpp/oracle/node_modules/mermaid/package.json')).version,'12.2.0');
const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
const html=`<!doctype html><meta charset="utf-8"><body><script type="module">
try {
const {default:mermaid}=await import(${JSON.stringify(url)}),result=[];
for(const [name,source] of ${JSON.stringify(cases)})for(const theme of ['default','dark']) {
 mermaid.initialize({startOnLoad:false,theme,look:'classic',fontFamily:'Arial',themeVariables:{fontFamily:'Arial'}});
 const {svg}=await mermaid.render('mindmap'+result.length,source);
 const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);
 const box=e=>{const b=e.getBBox();return {x:b.x,y:b.y,width:b.width,height:b.height};};
 const nodes=Array.from(host.querySelectorAll('g.mindmap-node')).map(n=>{
  const shape=n.querySelector('.label-container,.node-bkg'),label=n.querySelector('g.label'),foreign=label.querySelector('foreignObject');
  const transform=n.transform.baseVal.consolidate().matrix;
  return {id:n.id,position:{x:transform.e,y:transform.f},shape:shape.tagName,shapeBox:box(shape),nodeBox:box(n),labelBox:foreign?{width:+foreign.getAttribute('width'),height:+foreign.getAttribute('height')}:box(label),
   fill:getComputedStyle(shape).fill,labelText:label.textContent,textColor:getComputedStyle(label.querySelector('span')??label.querySelector('text')).color,
   path:shape.getAttribute('d'),shapeTransform:shape.getAttribute('transform'),labelTransform:label.getAttribute('transform')};
 });
 const edges=Array.from(host.querySelectorAll('.edgePaths path')).map(e=>({id:e.id,path:e.getAttribute('d'),stroke:getComputedStyle(e).stroke,width:parseFloat(getComputedStyle(e).strokeWidth)}));
 const view=host.querySelector('svg').viewBox.baseVal;
 const {db}=await mermaid.mermaidAPI.getDiagramFromText(source);
 const links=db.getData().edges.map(e=>({id:e.id,source:e.start,target:e.end}));
 result.push({name:name+'-'+theme,source,theme,nodes,edges,links,view:{x:view.x,y:view.y,width:view.width,height:view.height},svg});host.remove();
}
document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'result',textContent:JSON.stringify(result)}));
}catch(e){document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'error',textContent:String(e.stack)}));}
</script>`;
const file=resolve(directory,'probe.html');writeFileSync(file,html);
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
try {
 const page=await browser.newPage({viewport:{width:1600,height:1200}});await page.goto(pathToFileURL(file).href);
 await page.waitForSelector('pre#result,pre#error',{timeout:60000});
 assert.equal(await page.locator('pre#error').count(),0,await page.locator('body').textContent());
 const result=JSON.parse(await page.locator('pre#result').textContent());assert.equal(result.length,4);
 for(const row of result)assert.equal(row.nodes.length,row.name.startsWith('shapes')?7:3);
 for(const row of result) {
  const nodes=row.nodes.map((n,i)=>({group:'nodes',data:{id:String(i),width:n.nodeBox.width,height:n.nodeBox.height},position:{x:0,y:0}}));
  const cy=cytoscape({headless:true,styleEnabled:true,elements:[...nodes,...row.links.map(data=>({group:'edges',data}))]});
  cy.nodes().forEach(n=>{n.layoutDimensions=()=>({w:n.data('width'),h:n.data('height')});});
  const layout=cy.layout({name:'cose-bilkent',quality:'proof',styleEnabled:false,animate:false});layout.run();
  row.iterations=layout.layout.totalIterations;
  row.layoutPositions=cy.nodes().map(n=>({x:n.position('x'),y:n.position('y')}));
  cy.nodes().forEach((n,i)=>{assert(Math.abs(n.position('x')-row.nodes[i].position.x)<.0001,row.name+' pinned layout x');assert(Math.abs(n.position('y')-row.nodes[i].position.y)<.0001,row.name+' pinned layout y');});
  cy.destroy();
 }
 writeFileSync(resolve(directory,'shapes.json'),JSON.stringify(result,null,2));
 const encode=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
 writeFileSync(resolve(directory,'measurements.tsv'),result.map(row=>row.name+'\t'+encode(JSON.stringify(row))).join('\n')+'\n');
 for(const row of result)writeFileSync(resolve(directory,row.name+'.svg'),row.svg);
 if(process.argv.includes('--write-fixture')) {
  const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
  const rows=['# Original measured Mindmap labels and whole-node geometry; source, site, boxes as UTF-16 JSON.'];
  for(const row of result)rows.push([row.name,hex(row.source),hex(JSON.stringify({theme:row.theme,look:'classic'})),hex(JSON.stringify(row.nodes.map(n=>({label:n.labelBox,bounds:n.nodeBox}))))].join('\t'));
  writeFileSync('tests/mermaid_cpp/fixtures/mindmap-shapes.tsv',rows.join('\n')+'\n');
 }
 console.log('Mindmap: 2 representative shape/label diagrams in both themes measured.');
}finally{await browser.close();}
