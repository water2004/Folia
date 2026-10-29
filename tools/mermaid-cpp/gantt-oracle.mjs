import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,readdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './gantt-cases.mjs';
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.esm/',import.meta.url),sources=new Map();
for(const file of readdirSync(chunks).filter(n=>n.endsWith('.mjs.map'))){const map=JSON.parse(readFileSync(new URL(file,chunks)));for(let i=0;i<map.sources.length;++i)sources.set(map.sources[i],map.sourcesContent[i]);}
for(const file of lock.ganttTime.files)assert.equal(createHash('sha256').update(sources.get(file.source)).digest('hex'),file.sha256,file.source);
for(const file of lock.files.filter(f=>f.path.includes('/gantt/')))assert.equal(createHash('sha256').update(readFileSync(new URL('../../.reference/mermaid-js/'+file.path,import.meta.url))).digest('hex'),file.sha256,file.path);
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');const {hex}=await import('./config-reference.mjs');const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const wall=d=>d?Date.UTC(d.getFullYear(),d.getMonth(),d.getDate(),d.getHours(),d.getMinutes(),d.getSeconds(),d.getMilliseconds()):null;
const rows=['# Mermaid 12.2.0 Gantt parser/DB; dates are local civil milliseconds, UTF-16 hex JSON.'];let success=0;
for(const [name,source,failure]of cases){const site={theme:'default',fontFamily:'Arial',gantt:{useWidth:800}};let result=null;try{const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});const tasks=db.getTasks().map(t=>({id:t.id,text:t.task,section:t.section,start:wall(t.startTime),end:wall(t.endTime),renderEnd:wall(t.renderEndTime),manual:t.manualEndTime,order:t.order,active:!!t.active,done:!!t.done,crit:!!t.crit,milestone:!!t.milestone,vert:!!t.vert}));result={title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),sections:db.getSections(),dateFormat:db.getDateFormat(),axisFormat:db.getAxisFormat(),tickInterval:db.getTickInterval()??'',inclusive:db.endDatesAreInclusive(),topAxis:db.topAxisEnabled(),excludes:db.getExcludes(),includes:db.getIncludes(),tasks,config:prepared.effective.gantt};assert.notEqual(failure,null);++success;}catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);}rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));}
const path=new URL('../../tests/mermaid_cpp/fixtures/gantt.tsv',import.meta.url),output=rows.join('\n')+'\n';if(process.argv.includes('--write'))writeFileSync(path,output);else assert.equal(readFileSync(path,'utf8').replaceAll('\r\n','\n'),output);dom.window.close();console.log(`Gantt: ${success} DB observations and ${cases.length-success} common errors checked.`);
