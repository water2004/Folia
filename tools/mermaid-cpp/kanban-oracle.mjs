import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './kanban-cases.mjs';
import {createHash} from 'node:crypto';
const pins=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url))).kanbanBundle.files;
for(const pin of pins){let content;if(pin.sourceMap){const map=JSON.parse(readFileSync(new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.esm/'+pin.sourceMap,import.meta.url)));const i=map.sources.findIndex(s=>s.endsWith('/'+pin.path));assert(i>=0,pin.path);content=map.sourcesContent[i];}else content=readFileSync(new URL('../../.reference/mermaid-js/packages/mermaid/'+pin.path,import.meta.url));assert.equal(createHash('sha256').update(content).digest('hex'),pin.sha256,pin.path);}
const writeFixture=process.argv.includes('--write');process.argv=process.argv.filter(arg=>arg!=='--write');
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');const {hex}=await import('./config-reference.mjs');const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const rows=['# Mermaid 12.2.0 Kanban parser/DB, UTF-16 hex JSON.'];let success=0;
for(const [name,source,failure]of cases){const site={theme:'default',fontFamily:'Arial'};let result=null;try{const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned);const data=db.getData();result={nodes:data.nodes.map(n=>({id:n.id,parentId:n.parentId??null,label:n.label,isGroup:n.isGroup,level:n.level,ticket:n.ticket??null,priority:n.priority??null,assigned:n.assigned??null,icon:n.icon??null})),config:prepared.effective.kanban};assert.notEqual(failure,null);++success;}catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);}rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));}
const path=new URL('../../tests/mermaid_cpp/fixtures/kanban.tsv',import.meta.url),output=rows.join('\n')+'\n';if(writeFixture)writeFileSync(path,output);else assert.equal(readFileSync(path,'utf8').replaceAll('\r\n','\n'),output);dom.window.close();console.log(`Kanban: ${success} DB observations and ${cases.length-success} syntax errors checked.`);
