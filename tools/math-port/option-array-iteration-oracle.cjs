// Source-only check; original profile bytes and all Source fields are preserved.
// Root original Array.forEach observations for the actual FindTeX call boundary.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p50-regexp-host-source.json'))),entry=lock.hashes.find(x=>x.source==='ts/input/tex/FindTeX.ts');assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'.reference/MathJax-src',entry.source))).digest('hex'),entry.source_sha256);
const rows=[],text=v=>v===undefined?'U':v===null?'N':typeof v==='object'?'object':String(v);
function fixture(){const e={array:[],trace:[],set(i,v){this.array[i]=v;},callback(body){return function(v,i,a){e.trace.push(i+':'+text(v)+':'+arguments.length+':'+(a===e.array)+':'+text(this));if(body)body(v,i,a);};},each(cb,receiver){return this.array.forEach(cb,receiver);}};return e;}
function add(label,body){const e=fixture();let error='ok',result='THREW';try{result=body(e);}catch(x){error=x.name;}rows.push({label,expected:error+'|'+result+'|'+e.trace.join('/')});}
add('empty-array-and-undefined-result',e=>text(e.each(e.callback())));
add('three-callback-arguments-and-original-array',e=>{e.set(0,4);e.set(1,5);return text(e.each(e.callback()));});
add('holes-skipped',e=>{e.array.length=3;e.set(1,7);return text(e.each(e.callback()));});
add('inherited-slot-is-visited',e=>{const p=Object.create(Array.prototype);p[1]=8;Object.setPrototypeOf(e.array,p);e.array.length=2;return text(e.each(e.callback()));});
add('appended-entry-outside-original-length-ignored',e=>{e.set(0,1);return text(e.each(e.callback(()=>e.set(1,9))));});
add('deleted-future-slot-skipped',e=>{e.set(0,1);e.set(1,2);return text(e.each(e.callback((v,i)=>{if(i===0)delete e.array[1];})));});
add('changed-future-slot-read-live',e=>{e.set(0,1);e.set(1,2);return text(e.each(e.callback((v,i)=>{if(i===0)e.set(1,9);})));});
add('filled-future-hole-is-visited',e=>{e.set(0,1);e.array.length=3;return text(e.each(e.callback((v,i)=>{if(i===0)e.set(2,9);})));});
add('shrinking-array-skips-deleted-slots',e=>{e.set(0,1);e.set(1,2);e.set(2,3);return text(e.each(e.callback((v,i)=>{if(i===0)e.array.length=1;})));});
add('replacing-caller-array-keeps-captured-receiver',e=>{e.set(0,1);e.set(1,2);return text(e.each(e.callback((v,i)=>{if(i===0){e.array=[];e.set(0,9);}})));});
add('getter-read-once-before-callback',e=>{Object.defineProperty(e.array,0,{get(){e.trace.push('get');return 6;},configurable:true});return text(e.each(e.callback()));});
add('getter-appends-outside-captured-length',e=>{Object.defineProperty(e.array,0,{get(){e.trace.push('get');e.set(1,9);return 6;},configurable:true});return text(e.each(e.callback()));});
add('callback-throw-propagates',e=>{e.set(0,1);e.set(1,2);return text(e.each(e.callback(()=>{throw Error('callback');})));});
add('getter-throw-propagates',e=>{Object.defineProperty(e.array,0,{get(){throw Error('getter');}});return text(e.each(e.callback()));});
add('empty-array-still-validates-callback',e=>text(e.each(4)));
add('undefined-callback-is-invalid',e=>text(e.each(undefined)));
for(const [label,value]of [['null',null],['false',false],['zero',0],['object',{}]])add('this-argument:'+label,e=>{e.set(0,1);return text(e.each(e.callback(),value));});
add('method-arity-and-nonenumerable',e=>{const d=Object.getOwnPropertyDescriptor(Array.prototype,'forEach');return e.array.forEach.length+':'+d.enumerable;});
add('looked-up-native-method-can-be-overridden',e=>{e.array.forEach=function(){e.trace.push('override:'+(this===e.array)+':'+arguments.length);return 12;};return text(e.each(e.callback()));});
const report={stage:'ordinary Context array forEach native primitive; not FindTeX',node:process.version,caller:entry,rows};
const profile=path.join(root,'docs/math-port/evidence/p51a-option-array-iteration-source.json');
const sourceGold=JSON.parse(fs.readFileSync(profile,'utf8'));
const projected={...sourceGold,rows:sourceGold.rows.map(({cpp,...row})=>row)};
assert.deepEqual(report,projected);
console.log(JSON.stringify({sourceCases:rows.length,mode:'read-only-source-check'}));
