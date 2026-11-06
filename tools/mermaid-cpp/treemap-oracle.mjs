import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,readdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './treemap-cases.mjs';
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.esm/',import.meta.url),sources=new Map();
for(const file of readdirSync(chunks).filter(n=>n.endsWith('.mjs.map'))){const map=JSON.parse(readFileSync(new URL(file,chunks)));for(let i=0;i<map.sources.length;++i)for(const pkg of ['d3-hierarchy','d3-format']){const tail=map.sources[i].split('/'+pkg+'/')[1];if(tail)sources.set(pkg+'/'+tail,map.sourcesContent[i]);}}
for(const file of lock.treemapLayout.files){const source=file.source.replace(/@[^/]+/,'');assert.equal(createHash('sha256').update(sources.get(source)).digest('hex'),file.sha256,file.source);}
for(const file of lock.files.filter(f=>f.path.includes('/treemap/')))assert.equal(createHash('sha256').update(readFileSync(new URL('../../.reference/mermaid-js/'+file.path,import.meta.url))).digest('hex'),file.sha256,file.path);
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');const {hex}=await import('./config-reference.mjs');const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const rows=['# Mermaid 12.2.0 Treemap parser/DB observations; UTF-16 hex JSON.'];let success=0;
for(const [name,source,failure]of cases){const site={theme:'default',fontFamily:'Arial'};let result=null;try{const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});result={title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),root:db.getRoot(),config:db.getConfig()};assert.notEqual(failure,null);++success;}catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);}rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));}
const path=new URL('../../tests/mermaid_cpp/fixtures/treemap.tsv',import.meta.url),output=rows.join('\n')+'\n';if(process.argv.includes('--write'))writeFileSync(path,output);else assert.equal(readFileSync(path,'utf8').replaceAll('\r\n','\n'),output);dom.window.close();console.log(`Treemap: ${success} DB observations and ${cases.length-success} syntax errors checked.`);
