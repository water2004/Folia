import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './er-cases.mjs';
const writeFixture=process.argv.includes('--write');process.argv=process.argv.filter(arg=>arg!=='--write');
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');const {hex}=await import('./config-reference.mjs');const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const rows=['# Mermaid 12.2.0 ER parser/DB, UTF-16 hex JSON.'];let success=0;
for(const [name,source,failure]of cases){const site={theme:'default',fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic',layout:'dagre'};let result=null;try{const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned);result={direction:db.getDirection(),entities:[...db.getEntities()].map(([name,e])=>({name,id:e.id,alias:e.alias,attributes:e.attributes})),relationships:db.getRelationships(),config:prepared.effective.er};assert.notEqual(failure,null);++success;}catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);}rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));}
const path=new URL('../../tests/mermaid_cpp/fixtures/er.tsv',import.meta.url),output=rows.join('\n')+'\n';if(writeFixture)writeFileSync(path,output);else assert.equal(readFileSync(path,'utf8').replaceAll('\r\n','\n'),output);dom.window.close();console.log(`ER: ${success} DB observations and ${cases.length-success} syntax errors checked.`);
