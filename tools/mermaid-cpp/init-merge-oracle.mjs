// Original detectInit/cleanAndMerge bodies; diagram dispatch is injected, not ported here.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
import vm from 'node:vm';
import { merge } from './oracle/node_modules/es-toolkit/dist/compat/object/merge.js';
import { root, lock, src, extract, strip, sanitizeDirective, assign, log, hex, snapshot } from './config-reference.mjs';

const packageRoot=path.join(root,'tools/mermaid-cpp/oracle/node_modules/es-toolkit');
for(const file of lock.packages.find(p=>p.name==='es-toolkit').files)
  assert.equal(createHash('sha256').update(readFileSync(path.join(packageRoot,file.path))).digest('hex'),file.sha256,file.path);
assert.equal(JSON.parse(readFileSync(path.join(packageRoot,'package.json'))).version,'1.45.1');
const utils=src('utils.ts');
const cleanAndMerge=vm.runInNewContext(strip(extract(utils,'export function cleanAndMerge','\nexport default'))+'\ncleanAndMerge',{merge});
const chunks=path.join(root,'tools/mermaid-cpp/oracle/node_modules/mermaid/dist/chunks/mermaid.esm');
const bundleFile=readdirSync(chunks).find(f=>f.endsWith('.mjs')&&readFileSync(path.join(chunks,f),'utf8').includes('function cleanAndMerge('));
const sourceMap=JSON.parse(readFileSync(path.join(chunks,bundleFile+'.map')));
const utilsIndex=sourceMap.sources.findIndex(s=>s.endsWith('/src/utils.ts'));
assert(utilsIndex>=0);
assert.equal(sourceMap.sourcesContent[utilsIndex].replaceAll('\r\n','\n'),utils);
assert(sourceMap.sources.some(s=>s.includes('es-toolkit@1.45.1/')));
const bundle=await import(pathToFileURL(path.join(chunks,bundleFile)));
const {directiveRegex}=await import(pathToFileURL(path.join(root,'.reference/mermaid-js/packages/mermaid/src/diagram-api/regexes.ts')));
const initProgram=strip(extract(utils,'const directiveWithoutOpen =','\n/**')+'\n'+
  extract(utils,'export const detectDirective =','\nexport const removeDirectives')+'\n'+
  extract(utils,'export const detectInit =','\ninterface Directive'))+'\ndetectInit';
const encode=x=>hex(JSON.stringify(x));
const clone=x=>x===undefined?undefined:JSON.parse(JSON.stringify(x));
// Preserve array custom properties and undefined in observations instead of JSON-normalizing them away.
function observed(x) {
  if(!x || typeof x!=='object') return snapshot(x);
  if(Array.isArray(x)) {
    const extras=Object.keys(x).filter(k=>!(/^(0|[1-9][0-9]*)$/.test(k)&&Number(k)<x.length));
    return '['+Array.from({length:x.length},(_,i)=>observed(x[i])).join('')+']'+
      (extras.length?'@'+extras.sort().map(k=>snapshot(k)+observed(x[k])).join(''):'');
  }
  return '{'+Object.keys(x).sort().map(k=>snapshot(k)+observed(x[k])).join('')+'}';
}
function dense(x) {
  if(x===undefined) return false;
  if(!x || typeof x!=='object') return true;
  if(Array.isArray(x)&&(Object.keys(x).length!==x.length ||
    !Array.from({length:x.length},(_,i)=>Object.hasOwn(x,i)).every(Boolean))) return false;
  return Object.values(x).every(dense);
}
const rows=[];
function addClean(a,b) {
  const before=observed([a,b]);
  let result,other,error,otherError;
  try { result=cleanAndMerge(clone(a),clone(b)); } catch(e) { if(!['TypeError','RangeError'].includes(e.name)) throw e; error=e.name; }
  try { other=bundle.cleanAndMerge(clone(a),clone(b)); } catch(e) { otherError=e.name; }
  assert.equal(error,otherError);
  if(!error) assert.equal(observed(result),observed(other));
  assert.equal(observed([a,b]),before);
  rows.push(['clean',String(rows.length),encode(a),b===undefined?'U':encode(b),error?'E':observed(result),
    !error&&!dense(result)?'dense-output-only':'-','0'].join('\t'));
}
const atoms=[null,false,true,0,-0,3,'','word','中文\ud800',{},[],[1,2],[null,2],[{x:1}],{x:1},{x:null},{x:[1,2]},{x:{a:1}},
  {length:2,0:'a',1:'b'},JSON.parse('{"__proto__":{"bad":1},"constructor":{"prototype":{"x":1}},"toString":{"__proto__":{"x":1}}}')];
for(const a of atoms) for(const b of [...atoms,undefined]) { addClean(a,b); addClean({v:a},{v:b}); }
for(const a of [[1,2],[null,2],[{x:1}],[]]) for(const b of [{0:9},{1:8},{2:7},{length:0},{length:4},{length:-1},{custom:1},{constructor:3},{__defineGetter__:{x:1}}]) addClean({v:a},{v:b});
for(const length of [null,false,true,'',' 0 ','\ufeff1\u2003','0x1','0b1','0o1','+1','+-0','-0','-1',
  '.0','1.','1e-999','1e999','0e999','0x','-0x0','NaN','Infinity','word','\ud800',[],[null],[1],[1,2],{},
  {toString:1},'9'.repeat(400)+'e-999','0'.repeat(400)+'1',1.5,3]) addClean({v:[1,2]},{v:{length}});
let seed=0x1927;
const random=()=>seed=(Math.imul(seed,1664525)+1013904223)>>>0;
function tree(depth=0) {
  if(depth===3 || random()%3===0) return atoms[random()%9];
  if(random()%2) return Array.from({length:random()%4},()=>tree(depth+1));
  const obj={}; for(let i=0,n=random()%4;i<n;i++) obj[['a','b','0','length','constructor','__proto__'][random()%6]]=tree(depth+1);
  return obj;
}
for(let i=0;i<300;i++) addClean(tree(),tree());

const texts=['','pie','%%{init}%%','%%{init:{bad}}%%','%%{init: false}%%','%%{init: ""}%%','%%{init: []}%%',
  '%%{wrap}%%','%%{init:{"unknown":1,"config":{"curve":"basis"}}}%%',
  '%%{init:{"config":null,"flowchart":{"old":true}}}%%','%%{init:{"config":false}}%%',
  '%%{init:{"config":[1,null]}}%%','%%{init:{"config":{"x":1},"pie":{"x":2}}}%%',
  '%%{init:{"config":{"curve":"basis"},"unknown":1}}%%\n%%{initialize:{"theme":"dark"}}%%',
  '%%{init:{"config":{"curve":"basis","unknown":1}}}%%\n%%{initialize:{"config":{"curve":"linear"}}}%%',
  '%%{init:{"theme":"dark"}}%%\n%%{init:{bad}}%%','%%{init: false}%% %%{init:{"theme":"dark"}}%%',
  '%%{init:{"themeVariables":"<"}}%% %%{initialize:{}}%%',
  '%%{init:{"__proto__":3,"config":1}}%%','%%{init:{"config":1,"constructor":9}}%%'];
for(const line of ['\n','\r','\r\n','\u2028']) for(const body of ['{}','[]','null','0','"word"','{"config":{"fontFamily":"中文\\ud800"}}'])
  texts.push(`%%{initialize:${body}}%%${line}pie`);
for(let i=0;i<150;i++) texts.push(`%%{init:${JSON.stringify(tree())}}%%\n%%{initialize:${JSON.stringify(tree())}}%%`);
for(const text of texts) for(const type of ['flowchart-v2','flowchart-elk','stateDiagram-v2','pie','config','__proto__','constructor','custom-中文','!error']) {
  let calls=0,result,error;
  const config={flowchart:{defaultRenderer:'elk'}};
  const fn=vm.runInNewContext(initProgram,{directiveRegex:new RegExp(directiveRegex.source,directiveRegex.flags),log,
    sanitizeDirective,assignWithDepth:assign,detectType:(actual,cfg)=>{
      assert.equal(actual,text); assert.equal(cfg,config); calls++;
      if(type==='!error') throw new Error('oracle detector failure');
      return type;
    }});
  try { result=fn(text,config); } catch(e) {
    if(e.name!=='TypeError'&&e.message!=='oracle detector failure') throw e;
    error=e.name;
  }
  // VM objects have another realm's Object.prototype. A prototype changed to a directive value
  // has no own constructor; primitive assignments leave the realm prototype unchanged.
  const changedPrototype=result&&typeof result==='object'&&!Array.isArray(result)&&
    !Object.hasOwn(Object.getPrototypeOf(result)??{},'constructor');
  rows.push(['init',String(rows.length),hex(text),hex(type),error?'E':snapshot(result),
    type==='__proto__'&&changedPrototype?'prototype-setter':'-',String(calls)].join('\t'));
}
const content='# Official Mermaid 12.2.0 + es-toolkit 1.45.1: op/id/input/input-or-type/result/restriction/detector-calls\n'+rows.join('\n')+'\n';
const file=path.join(root,'tests/mermaid_cpp/fixtures/init-merge.tsv');
if(process.argv.includes('--write')) writeFileSync(file,content);
else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),content);
console.log(JSON.stringify({observations:rows.length,clean:rows.filter(r=>r.startsWith('clean\t')).length,init:rows.filter(r=>r.startsWith('init\t')).length}));
