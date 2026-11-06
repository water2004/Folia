// Execute original Mindmap parser/DB without promoting default config to user input.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,readdirSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './mindmap-cases.mjs';
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
for(const file of lock.files.filter(f=>f.path.includes('/mindmap/')))assert.equal(createHash('sha256').update(readFileSync(new URL('../../.reference/mermaid-js/'+file.path,import.meta.url))).digest('hex'),file.sha256,file.path);
const dom=new JSDOM('<!doctype html><body></body>');
globalThis.window=dom.window;globalThis.document=dom.window.document;
const {read,hex}=await import('./config-reference.mjs');
const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/',import.meta.url);
const map=JSON.parse(readFileSync(new URL(readdirSync(chunks).find(n=>n.endsWith('.mjs.map')&&readFileSync(new URL(n,chunks),'utf8').includes('/mindmapDb.ts')),chunks)));
for(const name of ['mindmapDb.ts','mindmapRenderer.ts']) {
 const index=map.sources.findIndex(n=>n.endsWith('/mindmap/'+name));assert(index>=0,name);
 assert.equal(map.sourcesContent[index].replaceAll('\r\n','\n'),read('packages/mermaid/src/diagrams/mindmap/'+name));
}
const rows=['# Mermaid 12.2.0 Mindmap parser/DB; source, site and data as UTF-16 hex JSON.'];let successes=0;
for(const [name,source,options={},failure=false] of cases) {
 const site={theme:'default',look:'classic',...options};let result=null;
 try {
  mermaid.initialize({...site,startOnLoad:false});
  // parse applies source directives and appearance scope while retaining only
  // explicit initialize/directive values in getUserDefinedConfig().
  await mermaid.parse(source);
  const {db}=await mermaid.mermaidAPI.getDiagramFromText(source);
  const data=db.getData();
  result={layout:data.config.layout,padding:data.config.mindmap.padding,maxNodeWidth:data.config.mindmap.maxNodeWidth,
   colorBy:data.config.mindmap.colorBy,
   nodes:data.nodes.map(n=>({id:n.id,nodeId:n.nodeId,label:n.label,labelType:n.labelType,shape:n.shape,
    level:n.level,type:n.type,section:n.section??null,width:n.width,height:n.height,padding:n.padding,classes:n.cssClasses,icon:n.icon??''})),
   edges:data.edges.map(e=>({id:e.id,start:e.start,end:e.end,depth:e.depth,section:e.section??null,classes:e.classes}))};
  ++successes;assert.equal(failure,false,name+' unexpectedly parsed');
 } catch(e) {assert.equal(failure,true,`${name}: ${e.stack}`);}
 rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));
}
const path=new URL('../../tests/mermaid_cpp/fixtures/mindmap.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(path,expected);else assert.equal(readFileSync(path,'utf8').replaceAll('\r\n','\n'),expected);
dom.window.close();console.log(`Mindmap: ${successes} parser/DB successes and ${cases.length-successes} common errors checked; layout remains separate.`);
