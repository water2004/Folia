// Released parser/DB/builder, matched to the pinned original source.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,readdirSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './quadrant-cases.mjs';
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');
const {read,hex}=await import('./config-reference.mjs');
const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/',import.meta.url);
const map=JSON.parse(readFileSync(new URL(readdirSync(chunks).find(n=>n.endsWith('.mjs.map')&&readFileSync(new URL(n,chunks),'utf8').includes('/quadrantBuilder.ts')),chunks)));
for(const file of ['quadrantBuilder.ts','quadrantDb.ts','quadrantRenderer.ts','utils.ts']) {
 const index=map.sources.findIndex(n=>n.endsWith('/quadrant-chart/'+file));assert(index>=0,file);
 assert.equal(map.sourcesContent[index].replaceAll('\r\n','\n'),read('packages/mermaid/src/diagrams/quadrant-chart/'+file));
}
const rows=['# Mermaid 12.2.0 Quadrant parser/DB/layout observations; UTF-16 hex JSON.'];let successes=0;
for(const [name,source,failure] of cases)for(const theme of ['default','dark']) {
 const site={theme,fontFamily:'Arial'};let result=null;
 try {
  const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});
  const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});
  result={title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),layout:db.getQuadrantData()};
  assert.notEqual(failure,null,name+' unexpectedly parsed');++successes;
 } catch(e) {assert.equal(failure,null,`${name}: ${e.stack}`);}
 rows.push([name+'-'+theme,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));
}
const output=new URL('../../tests/mermaid_cpp/fixtures/quadrant.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(output,expected);else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),expected);
dom.window.close();console.log(`Quadrant: ${successes} parser/layout observations and ${rows.length-1-successes} common errors checked.`);
