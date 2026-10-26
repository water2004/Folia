// Shape/label measurements only. Layout positions are observed separately
// through the pinned Cytoscape 3.34.0 oracle, not this bundled dependency.
import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync,readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases as allCases} from './mindmap-cases.mjs';
const cases=allCases.filter(([name])=>name==='shapes'||name==='labels');
const directory=resolve(process.argv[2]??'build/mermaid-cpp/mindmap-browser');
mkdirSync(directory,{recursive:true});
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
  return {id:n.id,shape:shape.tagName,shapeBox:box(shape),nodeBox:box(n),labelBox:foreign?{width:+foreign.getAttribute('width'),height:+foreign.getAttribute('height')}:box(label),
   path:shape.getAttribute('d'),shapeTransform:shape.getAttribute('transform'),labelTransform:label.getAttribute('transform')};
 });
 result.push({name:name+'-'+theme,source,theme,nodes});host.remove();
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
 writeFileSync(resolve(directory,'shapes.json'),JSON.stringify(result,null,2));
 if(process.argv.includes('--write-fixture')) {
  const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
  const rows=['# Original measured Mindmap labels and whole-node geometry; source, site, boxes as UTF-16 JSON.'];
  for(const row of result)rows.push([row.name,hex(row.source),hex(JSON.stringify({theme:row.theme,look:'classic'})),hex(JSON.stringify(row.nodes.map(n=>({label:n.labelBox,bounds:n.nodeBox}))))].join('\t'));
  writeFileSync('tests/mermaid_cpp/fixtures/mindmap-shapes.tsv',rows.join('\n')+'\n');
 }
 console.log('Mindmap: 2 representative shape/label diagrams in both themes measured.');
}finally{await browser.close();}
