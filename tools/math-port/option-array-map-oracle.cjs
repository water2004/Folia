// Source-only check; original profile bytes and all Source fields are preserved.
// Root observes original ordinary Array.map for actual Configuration.ts callers.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),source='ts/input/tex/Configuration.ts',scope=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json'))),caller=scope.files.find(x=>x.source===source);
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'.reference/MathJax-src',source))).digest('hex'),caller.source_sha256);
const text=v=>v===undefined?'U':v===null?'N':typeof v==='object'?'object':String(v),rows=[];
function fixture(){const e={array:[],trace:[],set(i,v){this.array[i]=v;},get(i,body){Object.defineProperty(this.array,i,{get:body,configurable:true});},callback(body){return function(v,i,a){e.trace.push(i+':'+text(v)+':'+arguments.length+':'+(a===e.array)+':'+text(this));return body?body(v,i,a):99;};},map(cb,receiver){return this.array.map(cb,receiver);},summary(v){if(!Array.isArray(v))return text(v);let r='len:'+v.length;for(let i=0;i<v.length;i++)r+=';'+i+':'+(Object.hasOwn(v,i)?'O:'+text(v[i]):'H');return r;}};return e;}
function add(label,body){const e=fixture();let error='ok',result='THREW';try{result=body(e);}catch(x){error=x.name;}rows.push({label,expected:error+'|'+result+'|'+e.trace.join('/')});}
add('empty-result-is-new-array',e=>e.summary(e.map(e.callback())));
add('callback-args-receiver-and-values',e=>{e.set(0,4);e.set(1,5);return e.summary(e.map(e.callback()));});
add('holes-preserved-with-original-length',e=>{e.array.length=3;e.set(1,7);return e.summary(e.map(e.callback()));});
add('explicit-undefined-is-a-visited-slot',e=>{e.set(0,undefined);return e.summary(e.map(e.callback()));});
add('inherited-index-is-visited',e=>{const p=Object.create(Array.prototype);p[1]=8;Object.setPrototypeOf(e.array,p);e.array.length=2;return e.summary(e.map(e.callback()));});
add('appended-beyond-captured-length-is-ignored',e=>{e.set(0,1);return e.summary(e.map(e.callback(()=>{e.set(1,9);return 99;})));});
add('deleted-future-slot-becomes-result-hole',e=>{e.set(0,1);e.set(1,2);return e.summary(e.map(e.callback((v,i)=>{if(i===0)delete e.array[1];return 99;})));});
add('future-slot-is-read-live',e=>{e.set(0,1);e.set(1,2);return e.summary(e.map(e.callback((v,i)=>{if(i===0)e.set(1,9);return v;})));});
add('filled-future-hole-is-visited',e=>{e.set(0,1);e.array.length=3;return e.summary(e.map(e.callback((v,i)=>{if(i===0)e.set(2,9);return 99;})));});
add('shrink-input-retains-original-result-length',e=>{e.set(0,1);e.set(1,2);e.set(2,3);return e.summary(e.map(e.callback((v,i)=>{if(i===0)e.array.length=1;return 99;})));});
add('replacing-caller-array-keeps-captured-receiver',e=>{e.set(0,1);e.set(1,2);return e.summary(e.map(e.callback((v,i)=>{if(i===0){e.array=[];e.set(0,9);}return 99;})));});
add('getter-before-callback-and-once',e=>{e.get(0,()=>{e.trace.push('get');return 6;});return e.summary(e.map(e.callback()));});
add('getter-fills-next-hole',e=>{e.array.length=2;e.get(0,()=>{e.trace.push('get');e.set(1,6);return 5;});return e.summary(e.map(e.callback()));});
add('getter-throw-propagates',e=>{e.get(0,()=>{throw Error('getter');});return e.summary(e.map(e.callback()));});
add('callback-throw-propagates',e=>{e.set(0,1);e.set(1,2);return e.summary(e.map(e.callback(()=>{throw Error('callback');})));});
for(const [label,value]of [['undefined',undefined],['null',null],['number',4],['object',{}]])add('empty-array-validates-callback:'+label,e=>e.summary(e.map(value)));
for(const [label,value]of [['null',null],['false',false],['zero',0],['object',{}]])add('this-argument:'+label,e=>{e.set(0,1);return e.summary(e.map(e.callback(),value));});
add('callback-return-array-identity-retained',e=>{const tuple=[1,2];e.set(0,tuple);const out=e.map(e.callback(v=>v));return String(out[0]===tuple);});
add('input-unchanged-and-result-distinct',e=>{e.set(0,1);const out=e.map(e.callback());return String(out!==e.array)+':'+e.array[0]+':'+e.summary(out);});
add('result-defines-own-data-without-prototype-setter',e=>{e.set(0,1);e.trace.push('before');const desc=Object.getOwnPropertyDescriptor(Array.prototype,'0');Object.defineProperty(Array.prototype,'0',{set(){e.trace.push('setter');},configurable:true});try{return e.summary(e.map(e.callback()));}finally{if(desc)Object.defineProperty(Array.prototype,'0',desc);else delete Array.prototype[0];}});
add('method-metadata',e=>{const d=Object.getOwnPropertyDescriptor(Array.prototype,'map');return e.array.map.length+':'+d.enumerable+':'+d.writable+':'+d.configurable;});
add('overridden-looked-up-map-is-called',e=>{e.array.map=function(){e.trace.push('override:'+(this===e.array)+':'+arguments.length);return 12;};return e.summary(e.map(e.callback()));});
add('constructor-get-before-element-get',e=>{e.get(0,()=>{e.trace.push('index');return 5;});Object.defineProperty(e.array,'constructor',{get(){e.trace.push('constructor');return Array;}});return e.summary(e.map(e.callback()));});
add('constructor-get-mutates-later-element',e=>{e.set(0,1);Object.defineProperty(e.array,'constructor',{get(){e.trace.push('constructor');e.set(0,6);return Array;}});return e.summary(e.map(e.callback()));});
add('constructor-get-after-length-snapshot',e=>{e.set(0,1);Object.defineProperty(e.array,'constructor',{get(){e.trace.push('constructor');e.set(1,6);return Array;}});return e.summary(e.map(e.callback()));});
add('constructor-getter-throw-propagates',e=>{Object.defineProperty(e.array,'constructor',{get(){e.trace.push('constructor');throw Error('constructor');}});return e.summary(e.map(e.callback()));});
add('invalid-callback-before-constructor-get',e=>{Object.defineProperty(e.array,'constructor',{get(){e.trace.push('constructor');return Array;}});return e.summary(e.map(4));});
add('undefined-constructor-defaults-array',e=>{e.set(0,1);e.array.constructor=undefined;return e.summary(e.map(e.callback()));});
for(const [label,value]of [['null',null],['zero',0],['false',false]])add('primitive-constructor-is-not-constructible:'+label,e=>{e.array.constructor=value;return e.summary(e.map(e.callback()));});
add('result-slot-is-own-writable-enumerable-configurable-data',e=>{e.set(0,1);const out=e.map(e.callback()),d=Object.getOwnPropertyDescriptor(out,'0');return String(!!d)+':'+d.writable+':'+d.enumerable+':'+d.configurable+':'+!('get'in d);});
add('object-this-argument-exact-identity',e=>{e.set(0,1);const sentinel={};return e.summary(e.map(function(){e.trace.push('same:'+(this===sentinel));return 98;},sentinel));});
for(const inherited of [false,true])add((inherited?'inherited':'own')+'-getter-receives-original-array',e=>{const owner=inherited?Object.create(Array.prototype):e.array;if(inherited){Object.setPrototypeOf(e.array,owner);e.array.length=1;}Object.defineProperty(owner,'0',{get(){e.trace.push('same:'+(this===e.array));return 5;},configurable:true});return e.summary(e.map(e.callback()));});
const report={stage:'ordinary native Array.map required by Configuration; not Configuration or parser acceptance',node:process.version,caller:{source,source_sha256:caller.source_sha256},rows};
const profile=path.join(root,'docs/math-port/evidence/p56-option-array-map-source.json');
const sourceGold=JSON.parse(fs.readFileSync(profile,'utf8'));
const projected={...sourceGold,rows:sourceGold.rows.map(({cpp,...row})=>row)};
assert.deepEqual(report,projected);
console.log(JSON.stringify({sourceCases:rows.length,mode:'read-only-source-check'}));
