// Parser and DB observations for the next XY family package, not native coverage.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,readdirSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './xy-cases.mjs';
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');
const {read,hex}=await import('./config-reference.mjs');
const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/',import.meta.url);
const map=JSON.parse(readFileSync(new URL(readdirSync(chunks).find(n=>n.endsWith('.mjs.map')&&readFileSync(new URL(n,chunks),'utf8').includes('/xychartDb.ts')),chunks)));
for(let i=0;i<map.sources.length;++i)if(map.sources[i].includes('/xychart/')&&map.sources[i].endsWith('.ts')) {
 const file='packages/mermaid/src/diagrams/xychart/'+map.sources[i].split('/xychart/')[1];
 assert.equal(map.sourcesContent[i].replaceAll('\r\n','\n'),read(file),file);
}
const rows=['# Mermaid 12.2.0 XY parser/DB observations for pending native port; UTF-16 hex JSON.'];let successes=0;
for(const [name,source,failure] of cases) {
 const site={theme:'default',fontFamily:'Arial'};let result=null;
 try {
  const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});
  const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});
  result={title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),data:db.getXYChartData(),config:db.getChartConfig()};
 } catch(e) {assert.equal(failure,null,`${name}: ${e.stack}`);}
 if(result!==null){assert.notEqual(failure,null,name+' unexpectedly parsed');++successes;}
 rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));
}
const output=new URL('../../tests/mermaid_cpp/fixtures/xy.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(output,expected);else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),expected);
dom.window.close();console.log(`XY: ${successes} parser/DB observations and ${cases.length-successes} common errors checked; native port pending.`);
