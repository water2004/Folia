import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './state-cases.mjs';
import {createHash} from 'node:crypto';
const pins=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url))).stateBundle.files;
for(const pin of pins){let content;if(pin.sourceMap){const map=JSON.parse(readFileSync(new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.esm/'+pin.sourceMap,import.meta.url)));const i=map.sources.findIndex(s=>s===pin.path||s.endsWith('/'+pin.path));assert(i>=0,pin.path);content=map.sourcesContent[i];}else content=readFileSync(new URL('../../.reference/mermaid-js/packages/mermaid/'+pin.path,import.meta.url));assert.equal(createHash('sha256').update(content).digest('hex'),pin.sha256,pin.path);}
const writeFixture=process.argv.includes('--write');process.argv=process.argv.filter(arg=>arg!=='--write');
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');const {hex}=await import('./config-reference.mjs');const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const rows=['# Mermaid 12.2.0 State parser/DB, UTF-16 hex JSON.'];let success=0;
for(const [name,source,failure]of cases){const site={theme:'default',fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic',layout:'dagre'};let result=null;try{const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned);const data=db.getData();let divider=0;const originalNodes=data.nodes.filter(n=>n.shape!=='note'&&n.shape!=='noteGroup'),idMap=new Map(originalNodes.map(n=>[n.id,n.shape==='divider'?`divider-id-${++divider}`:n.id]));for(const node of originalNodes)if(node.shape==='stateStart'||node.shape==='stateEnd'){const suffix=node.shape==='stateStart'?'_start':'_end',parent=node.id.slice(0,-suffix.length);if(idMap.has(parent))idMap.set(node.id,idMap.get(parent)+suffix);}const normalizedId=id=>idMap.get(id)??id;const nodes=originalNodes.map(n=>({id:normalizedId(n.id),label:normalizedId(n.label),shape:n.shape,parent:normalizedId(n.parentId??''),group:n.isGroup,direction:n.dir??'',styles:n.cssStyles??[]}));const annotations=data.nodes.filter(n=>n.shape==='note').map(note=>{const edge=data.edges.find(e=>e.start===note.id||e.end===note.id);assert(edge,`${name}: note edge is missing`);return{state:normalizedId(edge.start===note.id?edge.end:edge.start),text:note.label,position:edge.start===note.id?'left':'right'};});result={direction:data.direction,nodes,edges:data.edges.filter(e=>!data.nodes.some(n=>n.shape==='note'&&(n.id===e.start||n.id===e.end))).map(e=>({start:normalizedId(e.start),end:normalizedId(e.end),label:e.label})),annotations,config:prepared.effective.state};assert.notEqual(failure,null);++success;}catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);}rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));}
const path = new URL('../../tests/mermaid_cpp/fixtures/state.tsv', import.meta.url);
const output = rows.join('\n') + '\n';
if (writeFixture) writeFileSync(path, output);
else assert.equal(readFileSync(path, 'utf8').replaceAll('\r\n', '\n'), output);
dom.window.close();
console.log(`State: ${success} DB observations and ${cases.length - success} syntax errors checked.`);
