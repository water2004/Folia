// Original Architecture grammar/DB and layout-constraint inputs, without fCoSE.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,readdirSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './architecture-cases.mjs';
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {read,hex}=await import('./config-reference.mjs');
const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/',import.meta.url);
const map=JSON.parse(readFileSync(new URL(readdirSync(chunks).find(n=>n.endsWith('.mjs.map')&&readFileSync(new URL(n,chunks),'utf8').includes('/architectureDb.ts')),chunks)));
for(const name of ['architectureDb.ts','architectureParser.ts','architectureTypes.ts']) {
 const index=map.sources.findIndex(n=>n.endsWith('/architecture/'+name));assert(index>=0,name);
 assert.equal(map.sourcesContent[index].replaceAll('\r\n','\n'),read('packages/mermaid/src/diagrams/architecture/'+name));
}
const rows=['# Mermaid 12.2.0 Architecture: source, site and parser/DB/constraint data as UTF-16 JSON.'];let successes=0;
for(const [name,source,options={},failure=false] of cases) {
 const site={theme:'default',look:'classic',...options};let result=null;
 try {
  mermaid.initialize({...site,startOnLoad:false});await mermaid.parse(source);
  const {db}=await mermaid.mermaidAPI.getDiagramFromText(source),structures=db.getDataStructures();
  const edges=db.getEdges().map(e=>({lhsId:e.lhsId,lhsDir:e.lhsDir,lhsInto:e.lhsInto??false,lhsGroup:e.lhsGroup??false,rhsId:e.rhsId,rhsDir:e.rhsDir,rhsInto:e.rhsInto??false,rhsGroup:e.rhsGroup??false,title:e.title??''}));
  result={title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescr:db.getAccDescription(),
   nodes:db.getNodes().map(n=>({id:n.id,type:n.type,icon:n.icon??'',iconText:n.iconText??'',title:n.title??'',parent:n.in??'',edges:n.edges.map(e=>db.getEdges().indexOf(e))})),
   groups:db.getGroups().map(g=>({id:g.id,icon:g.icon??'',title:g.title??'',parent:g.in??''})),edges,hints:db.getLayoutHints(),
   adjacency:[...structures.adjList].map(([id,neighbors])=>[id,[...neighbors]]),spatialMaps:structures.spatialMaps.map(m=>[...m]),groupAlignments:[...structures.groupAlignments],config:db.getConfig()};
  ++successes;assert.equal(failure,false,name+' unexpectedly succeeded');
 }catch(e){assert.equal(failure,true,`${name}: ${e.stack}`);}
 rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));
}
const file=new URL('../../tests/mermaid_cpp/fixtures/architecture.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(file,expected);else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),expected);
dom.window.close();console.log(`Architecture: ${successes} original parser/DB/constraint successes and ${cases.length-successes} common errors.`);
