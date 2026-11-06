import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,readdirSync} from 'node:fs';
import vm from 'node:vm';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './sankey-cases.mjs';
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');const {hex}=await import('./config-reference.mjs');
const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.esm/',import.meta.url);
const sources=new Map();for(const file of readdirSync(chunks).filter(n=>n.endsWith('.mjs.map'))){const map=JSON.parse(readFileSync(new URL(file,chunks)));for(let i=0;i<map.sources.length;++i)if(map.sources[i].includes('/d3-sankey/src/'))sources.set(map.sources[i].split('/d3-sankey/src/')[1],map.sourcesContent[i]);}
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
for(const file of lock.sankeyLayout.files)assert.equal(createHash('sha256').update(sources.get(file.path.slice(4))).digest('hex'),file.sha256,file.path);
for(const file of lock.files.filter(f=>f.path.includes('/diagrams/sankey/')))assert.equal(createHash('sha256').update(readFileSync(new URL('../../.reference/mermaid-js/'+file.path,import.meta.url))).digest('hex'),file.sha256,file.path);
const plain=s=>s.replace(/^import .*?;\n/gm,'').replace(/^export default function Sankey/m,'function Sankey').replace(/^export /gm,'');
const scope={min:(a,f=x=>x)=>Math.min(...a.map(f)),max:(a,f=x=>x)=>Math.max(...a.map(f)),sum:(a,f=x=>x)=>a.reduce((sum,x)=>sum+(Number(f(x))||0),0)};vm.createContext(scope);vm.runInContext(plain(sources.get('align.js'))+'\n'+plain(sources.get('sankey.js')),scope);
const rows=['# Mermaid 12.2.0 Sankey parser/DB and original d3-sankey 0.12.3 layout; UTF-16 hex JSON.'];let ok=0,errors=0;
for(const [name,source,failure] of cases){const site={theme:'default',fontFamily:'Arial'};let result=null;try{const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});const graph=db.getGraph(),config=db.getConfig();let geometry=null;try{const layout=scope.Sankey().nodeId(d=>d.id).nodeWidth(config.nodeWidth).nodePadding(config.nodePadding+(config.showValues?15:0)).nodeAlign(scope[config.nodeAlignment]).extent([[0,0],[config.width,config.height]]);const g=layout(structuredClone(graph));geometry={nodes:g.nodes.map(n=>({id:n.id,value:n.value,depth:n.depth,height:n.height,layer:n.layer,x0:n.x0,x1:n.x1,y0:n.y0,y1:n.y1})),links:g.links.map(l=>({source:l.source.index,target:l.target.index,value:l.value,width:l.width,y0:l.y0,y1:l.y1}))};assert.notEqual(failure,'layout');}catch(e){assert.equal(failure,'layout',e.stack);}result={graph,config,geometry};assert.notEqual(failure,null);++ok;}catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);++errors;}rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));}
const file=new URL('../../tests/mermaid_cpp/fixtures/sankey.tsv',import.meta.url),output=rows.join('\n')+'\n';if(process.argv.includes('--write'))writeFileSync(file,output);else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),output);dom.window.close();console.log(`Sankey: ${ok} DB/layout observations and ${errors} CSV errors checked.`);
