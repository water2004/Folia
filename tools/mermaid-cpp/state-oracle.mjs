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
for(const [name,source,failure]of cases){const site={theme:'default',fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic',layout:'dagre'};let result=null;try{const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned);const data=db.getData();result={direction:data.direction,nodes:data.nodes.map(n=>({id:n.id,label:n.label,shape:n.shape,parent:n.parentId??'',group:n.isGroup,direction:n.dir??'',styles:n.cssStyles??[]})),edges:data.edges.map(e=>({start:e.start,end:e.end,label:e.label})),config:prepared.effective.state};assert.notEqual(failure,null);++success;}catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);}rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));}
const path=new URL('../../tests/mermaid_cpp/fixtures/state.tsv',import.meta.url),output=rows.join('\n')+'\n';if(writeFixture)writeFileSync(path,output);else assert.equal(readFileSync(path,'utf8').replaceAll('\r\n','\n'),output);dom.window.close();console.log(`State: ${success} DB observations and ${cases.length-success} syntax errors checked.`);
