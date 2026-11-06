import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './journey-cases.mjs';
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
for(const file of lock.files.filter(f=>f.path.includes('/user-journey/')||f.path.endsWith('/common/svgDrawCommon.ts')))assert.equal(createHash('sha256').update(readFileSync(new URL('../../.reference/mermaid-js/'+file.path,import.meta.url))).digest('hex'),file.sha256,file.path);
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');const {hex}=await import('./config-reference.mjs');const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const rows=['# Mermaid 12.2.0 Journey parser/DB observations; UTF-16 hex JSON.'];let success=0;
for(const [name,source,failure]of cases){const site={theme:'default',fontFamily:'Arial'};let result=null;try{const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});const tasks=db.getTasks();result={title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),sections:db.getSections(),tasks,actors:db.getActors(),config:db.getConfig()};assert.notEqual(failure,null);++success;}catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);}rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));}
const path=new URL('../../tests/mermaid_cpp/fixtures/journey.tsv',import.meta.url),output=rows.join('\n')+'\n';if(process.argv.includes('--write'))writeFileSync(path,output);else assert.equal(readFileSync(path,'utf8').replaceAll('\r\n','\n'),output);dom.window.close();console.log(`Journey: ${success} DB observations and ${cases.length-success} syntax errors checked.`);
