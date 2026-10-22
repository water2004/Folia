// Source-only check; original profile bytes and all Source fields are preserved.
// Original FunctionList consuming actual option arrays; no native gold algorithm.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root}=require('./simple-mml-reference.cjs'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p46-parser-options-source.json'),'utf8')),hashes=[];
for(const source of ['ts/util/FunctionList.ts','ts/util/PrioritizedList.ts','ts/util/Options.ts']){
 const entry=lock.hashes.find(x=>x.source===source);assert.ok(entry,source+' Source lock');
 const upstream=fs.readFileSync(path.join(root,'.reference/MathJax-src',source)),installed=fs.readFileSync(path.join(base,'..',source));
 assert.equal(crypto.createHash('sha256').update(upstream).digest('hex'),entry.source_sha256);
 assert.equal(installed.toString().replaceAll('\r\n','\n'),upstream.toString().replaceAll('\r\n','\n'));
 assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(base,source.replace(/^ts\//,'').replace(/\.ts$/,'.js')))).digest('hex'),entry.cjs_sha256);hashes.push(entry);
}
const {FunctionList}=require(path.join(base,'util/FunctionList.js')),{PrioritizedList}=require(path.join(base,'util/PrioritizedList.js')),{defaultOptions}=require(path.join(base,'util/Options.js'));
const rows=[];const val=p=>p===undefined?'U':p===null?'N':String(p);
function add(label,run){PrioritizedList.DEFAULTPRIORITY=5;const trace=[];function f(){trace.push('call:f:'+String(this.item===f));}function g(){trace.push('call:g:'+String(this.item===g));}let hook=null;class Probe extends FunctionList{add(fn,p=PrioritizedList.DEFAULTPRIORITY){trace.push('add:'+(fn===f?'f':fn===g?'g':val(fn))+':'+p);if(hook)hook(fn,p);return super.add(fn,p);}}const list=new Probe();let error='ok',result='THREW';try{result=run({trace,f,g,list,setHook:h=>hook=h});}catch(e){error=e.name+'|'+e.message;}rows.push({label,expected:error+'|'+result+'|'+Array.from(list).map(r=>(r.item===f?'f':r.item===g?'g':val(r.item))+':'+r.priority).join(',')+'|'+trace.join('/')});PrioritizedList.DEFAULTPRIORITY=5;}
add('direct-and-tuple-order',e=>{e.list.addList([e.f,[e.g,1],e.f]);e.list.execute();return'ok';});
add('holes-and-inherited-slots',e=>{const a=new Array(3),p=Object.create(Array.prototype);p[1]=e.f;Object.setPrototypeOf(a,p);a[2]=e.g;e.list.addList(a);return'ok';});
add('live-array-growth',e=>{const a=[e.f];let n=0;e.setHook(()=>{if(++n===1)a.push(e.g);});e.list.addList(a);return'ok';});
add('live-array-shrink',e=>{const a=[e.f,e.g];e.setHook(()=>a.length=1);e.list.addList(a);return'ok';});
add('captured-array-after-rebind',e=>{let a=[e.f,e.g];e.setHook(()=>a=[e.g]);e.list.addList(a);return'ok';});
add('tuple-read-before-add',e=>{const t=[e.f,7];e.setHook(()=>{t[0]=e.g;t[1]=9;});e.list.addList([t]);return'ok';});
add('getter-zero-before-one',e=>{const t=[e.f,1];Object.defineProperty(t,0,{get(){e.trace.push('get:0');t[1]=8;return e.f;}});e.list.addList([t]);return'ok';});
add('getter-live-default-priority',e=>{const t=[e.f,undefined];Object.defineProperty(t,0,{get(){e.trace.push('get:0');PrioritizedList.DEFAULTPRIORITY=11;return e.f;}});e.list.addList([t,e.g]);return'ok';});
add('indexed-get-after-add',e=>{const a=[e.f,e.g];Object.defineProperty(a,1,{get(){e.trace.push('get:1');return e.g;}});e.list.addList(a);return'ok';});
add('virtual-callee-default-and-undefined',e=>{class OwnDefault extends FunctionList{add(fn,p=17){return e.list.add(fn,p);}}new OwnDefault().addList([e.f,[e.g,undefined],[e.f,3]]);return'ok';});
add('getter-error-prefix',e=>{const t=[e.g,1];Object.defineProperty(t,1,{get(){e.trace.push('get:1');throw Error('priority');}});e.list.addList([e.f,t]);return'ok';});
add('function-alias-after-option-copy',e=>{const out=defaultOptions({}, {pre:[e.f,e.f]});e.list.addList(out.pre);return'ok';});
add('canonical-function-binding',e=>String([e.f,e.f][0]===e.f));
add('typed-context-call-same-canonical-body',e=>{e.list.addList([e.f]);const record=Array.from(e.list)[0];return val(record.item.call(record));});
for(const [label,v]of [['undefined',undefined],['null',null],['false',false],['zero',0],['empty','']])add('constructor-falsy:'+label,()=>String(Array.from(new FunctionList(v)).length));
;
const report={stage:'original ordinary-array/numeric-priority option binding observations',hashes,rows};
const profile=path.join(root,'docs/math-port/evidence/p47-option-filter-source.json');
const sourceGold=JSON.parse(fs.readFileSync(profile,'utf8'));
const projected={...sourceGold,rows:sourceGold.rows.map(({cpp,...row})=>row)};
assert.deepEqual(report,projected);
console.log(JSON.stringify({sourceCases:rows.length,mode:'read-only-source-check'}));
