// Root-owned operation fixtures; native implementations never supply expected data.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const cp=require('node:child_process');
const root=path.resolve(__dirname,'../..'),source='ts/util/Options.ts',hash='b810ffc09864eb8ed9295d829b229194d7e38aaac6c8f01525bc1a4b190a61e3';
const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),hash);
const npmRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');assert.equal(require(path.join(npmRoot,'package.json')).version,'4.1.3');
assert.equal(fs.readFileSync(path.join(npmRoot,source),'utf8').replaceAll('\r\n','\n'),bytes.toString('utf8').replaceAll('\r\n','\n'));
const O=require(path.join(npmRoot,'cjs/util/Options.js'));
let seq=0;const sourceCases=[];let activeCase;const observe=(kind,value)=>activeCase.observations.push({kind,value});
const units=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i));
class Case{
constructor(name){this.name=name;activeCase={name,observations:[]};sourceCases.push(activeCase);this.map=new Map();this.warnings=[];this.oldWarn=console.warn;this.oldMode=O.OPTIONS.invalidOption;this.oldError=O.OPTIONS.optionError;O.OPTIONS.invalidOption='warn';console.warn=m=>this.warnings.push(m);for(const[obj,name]of [[Object.prototype,'objectPrototype'],[Function.prototype,'functionPrototype'],[Array.prototype,'arrayPrototype'],[O.Expandable.prototype,'expandablePrototype'],[Number.prototype,'numberPrototype'],[Boolean.prototype,'booleanPrototype'],[String.prototype,'stringPrototype'],[Symbol.prototype,'symbolPrototype'],[Object,'objectConstructor'],[Function,'functionConstructor'],[Array,'arrayConstructor'],[O.Expandable,'expandableConstructor'],[Number,'numberConstructor'],[Boolean,'booleanConstructor'],[String,'stringConstructor'],[Symbol,'symbolConstructor']])this.map.set(obj,name);}
bind(name,value){this.map.set(value,name);return value;}object(name,value){return this.bind(name,value);}set(obj,key,v){obj[key]=v;}define(obj,key,d){Object.defineProperty(obj,key,d);}
value(x){if(x===undefined)return{type:'undefined'};if(x===null)return{type:'null'};if(typeof x==='boolean')return{type:'boolean',value:x};if(typeof x==='number'){const b=Buffer.alloc(8);b.writeDoubleLE(x);return{type:'number',bits:b.readBigUInt64LE().toString(16).padStart(16,'0')};}if(typeof x==='string')return{type:'string',units:units(x)};assert.ok(this.map.has(x),'Unbound identity');return{type:'identity',name:this.map.get(x)};}
key(x){return typeof x==='string'?{type:'string',units:units(x)}:{type:'identity',name:this.map.get(x)};}
expect(x){if(x!==null&&(typeof x==='object'||typeof x==='function')&&!this.map.has(x)){const name='new_object_'+(++seq);observe('boolean',true);this.map.set(x,name);this.snapshot(x);return;}observe('value',this.value(x));}
snapshot(obj){observe('kind',Array.isArray(obj)?'array':typeof obj==='function'?'function':'object');const proto=Object.getPrototypeOf(obj);assert.ok(proto===null||this.map.has(proto),'Unbound prototype');observe('identity',proto===null?null:this.map.get(proto));const keys=Reflect.ownKeys(obj);observe('keys',keys.map(k=>this.key(k)));for(const key of keys){const d=Object.getOwnPropertyDescriptor(obj,key);++seq;observe('boolean',true);observe('flags',[!('value'in d),d.enumerable,d.configurable]);if('value'in d){observe('boolean',d.writable);this.expect(d.value);}else{observe('identity',d.get?this.map.get(d.get):null);observe('identity',d.set?this.map.get(d.set):null);}}}
operation(fn){const actual=fn();++seq;this.expect(actual);return actual;}
throws(fn,exact=false){let error;try{fn();}catch(e){error=e;}assert.ok(error,'Expected reference exception');++seq;observe('errorName',error.name);if(exact)observe('errorMessage',units(error.message));observe('boolean',true);}
finish(){observe('strings',this.warnings.map(units));console.warn=this.oldWarn;O.OPTIONS.invalidOption=this.oldMode;O.OPTIONS.optionError=this.oldError;}}
{
const t=new Case('classification');const a=t.object('a',{}),z=t.object('z',Object.create(null)),e=t.object('e',O.expandable({})),arr=t.object('arr',[]);
const fn=t.bind('fn',()=>{});class Item{};t.bind('ctor',Item);const inst=t.bind('inst',new Item());
for(const x of [undefined,null,false,0,'',Symbol.iterator]){if(typeof x==='symbol')t.bind('sym',x);t.operation(()=>O.isObject(x));}
for(const x of [a,z,e,arr,fn,inst])t.operation(()=>O.isObject(x));
t.set(z,'constructor',Object);t.operation(()=>O.isObject(z));t.set(a,'constructor',123);t.operation(()=>O.isObject(a));t.finish();
}
{
const t=new Case('keys_and_primitive_boxing'),a=t.object('a',{});for(const[k,v]of [['b',1],['2',2],['a',3],['01',4],['4294967295',5],['0',6]])t.set(a,k,v);
const s=t.bind('s',Symbol('same')),s2=t.bind('s2',Symbol('same'));t.define(a,s,{value:7,enumerable:true});t.define(a,s2,{value:8,enumerable:false});t.define(a,'hidden',{value:9});
for(const x of [a,undefined,null,false,0,NaN,'','𝔸a']){observe('keys',O.keys(x).map(k=>t.key(k)));;}
const dst=t.object('dst',{});t.operation(()=>O.insert(dst,a,false));t.snapshot(dst);t.operation(()=>O.copy('𝔸a'));t.throws(()=>O.copy(null));t.throws(()=>O.copy(undefined));t.finish();
}
{
const t=new Case('descriptors_and_copy');let gets=0,sets=0;undefined;const g=t.bind('g',()=>{gets++;return 42;});
const s=t.bind('s',v=>{sets++;});
const a=t.object('a',{});t.define(a,'access',{get:g,set:s,enumerable:true,configurable:true});t.define(a,'frozen',{value:1,writable:false,enumerable:true,configurable:false});t.define(a,'hidden',{value:2});
const result=t.operation(()=>O.copy(a));observe('counts',[gets,sets]);;
t.operation(()=>result.access);observe('counts',[gets]);;
t.throws(()=>O.insert(result,{frozen:3},false));t.snapshot(result);t.finish();
}
{
const t=new Case('deep_copy_and_expandable');const child=t.object('child',{});t.set(child,'v',3);class Item{};t.bind('ctor',Item);const inst=t.bind('inst',new Item()),fn=t.bind('fn',()=>{});
const a=t.object('a',O.expandable({}));t.set(a,'left',child);t.set(a,'right',child);t.set(a,'instance',inst);t.set(a,'callback',fn);
const copied=t.operation(()=>O.copy(a));observe('boolean',copied.left!==copied.right);;t.operation(()=>O.makeArray(fn));
const ar=t.object('ar',[]);t.operation(()=>O.makeArray(ar));t.operation(()=>O.expandable(child));t.finish();
}
{
const t=new Case('array_copy_and_holes');const ar=t.object('ar',new Array(6)),nested=t.object('nested',{});t.set(nested,'k',1);t.set(ar,'0',nested);t.set(ar,'3',undefined);t.set(ar,'extra',4);const s=t.bind('s',Symbol('hidden'));t.define(ar,s,{value:8,enumerable:false});const src=t.object('src',{}),dst=t.object('dst',{});t.set(src,'items',ar);t.operation(()=>O.insert(dst,src,false));t.snapshot(dst);t.snapshot(ar);t.finish();
}
{
const t=new Case('append_remove_identity');const marker=t.object('marker',{}),other=t.object('other',{}),ar=t.object('ar',[]);for(const[i,v]of [marker,NaN,2,2,other,undefined].entries())t.set(ar,String(i),v);
const rm=t.object('rm',[]);for(const[i,v]of [marker,NaN,2].entries())t.set(rm,String(i),v);const add=t.object('add',new Array(2));t.set(add,'1',marker);
const edits=t.object('edits',{});t.set(edits,'[-]',rm);t.set(edits,'[+]',add);const src=t.object('src',{}),dst=t.object('dst',{});t.set(src,'items',edits);t.set(dst,'items',ar);t.operation(()=>O.userOptions(dst,src));t.snapshot(dst);t.snapshot(ar);t.snapshot(add);t.finish();
}
{
const t=new Case('malformed_sentinels');const dst=t.object('dst',{}),ar=t.object('ar',[]);t.set(ar,'0',1);t.set(dst,'items',ar);const edits=t.object('edits',{});t.set(edits,'[+]','bad');const src=t.object('src',{});t.set(src,'items',edits);t.operation(()=>O.insert(dst,src));t.snapshot(ar);
const sym=t.bind('sym',Symbol('unexpected')),mixed=t.object('mixed',{}),add=t.object('add',[]);t.set(mixed,'[+]',add);t.set(mixed,sym,add);t.set(src,'items',mixed);t.throws(()=>O.insert(dst,src),true);t.finish();
}
{
const t=new Case('warn_fatal_inheritance');const proto=t.object('proto',{});t.set(proto,'inherited',5);const dst=t.object('dst',Object.create(proto));t.set(dst,'undefinedOwn',undefined);t.set(dst,'known',0);const src=t.object('src',{});t.set(src,'inherited',6);t.set(src,'undefinedOwn',2);t.set(src,'bad',1);t.set(src,'known',7);t.operation(()=>O.insert(dst,src));t.snapshot(dst);
const fatal=t.object('fatal',{});t.set(fatal,'known',0);const fsrc=t.object('fsrc',{});t.set(fsrc,'known',3);t.set(fsrc,'bad',8);t.set(fsrc,'later',9);O.OPTIONS.invalidOption='fatal';undefined;t.throws(()=>O.insert(fatal,fsrc),true);t.snapshot(fatal);t.finish();
}
{
const t=new Case('function_destination_and_setter');const fn=t.bind('fn',()=>{}),nested=t.object('nested',{});t.set(nested,'x',1);const dst=t.object('dst',{}),src=t.object('src',{});t.set(dst,'value',fn);t.set(src,'value',nested);t.operation(()=>O.insert(dst,src,false));t.operation(()=>fn.x);
let reads=0,writes=0;undefined;const g=t.bind('g',()=>{reads++;return 9;}),s=t.bind('s',()=>{writes++;});const p=t.object('p',{});t.define(p,'x',{get:g,set:s,enumerable:true,configurable:true});const target=t.object('target',Object.create(p)),access=t.object('access',{});t.define(access,'x',{get:g,enumerable:true,configurable:true});t.operation(()=>O.insert(target,access,false));observe('counts',[reads,writes]);observe('boolean',!Object.hasOwn(target,'x'));;t.finish();
}
{
const t=new Case('selection_and_separation');const proto=t.object('proto',{});t.set(proto,'inherited',1);const opts=t.object('opts',Object.create(proto)),child=t.object('child',{});t.set(opts,'a',child);t.set(opts,'b',undefined);t.set(opts,'rest',3);t.operation(()=>O.selectOptions(opts,'a','b','inherited'));
const sel=t.object('sel',{});t.set(sel,'a',0);const s=t.bind('s',Symbol('extra'));t.set(sel,s,1);t.operation(()=>O.selectOptionsFromKeys(opts,sel));const selectorProto=t.object('selectorProto',{});t.set(selectorProto,'a',true);const sel2=t.object('sel2',Object.create(selectorProto));t.set(sel2,'b',undefined);
const js=O.separateOptions(opts,sel2,sel),name='parts_'+(++seq);observe('counts',[js.length]);;js.forEach((x,i)=>t.expect(x));
for(const key of ['a','b','inherited','missing'])t.operation(()=>O.lookup(key,opts,child));t.operation(()=>O.lookup('missing',opts));t.finish();
}
{
const t=new Case('defaults_and_proto_basket');const dst=t.object('dst',{}),d1=t.object('d1',{}),d2=t.object('d2',{});t.set(d1,'new',1);t.set(d2,'new',2);t.set(d2,'another',3);t.operation(()=>O.defaultOptions(dst,d1,d2));t.snapshot(dst);
const source=t.object('source',{}),nested=t.object('nested',{});t.set(nested,'k',1);t.define(source,'__proto__',{value:nested,writable:true,enumerable:true,configurable:true});t.set(source,'ordinary',7);t.operation(()=>O.copy(source));t.finish();
}
{
const t=new Case('live_array_adapters');const grow=t.object('grow',new Array(1));
const gg=t.bind('gg',()=>{grow[2]=9;return 4;});t.define(grow,'0',{get:gg,enumerable:true,configurable:true});
let values=[...grow];observe('counts',[values.length]);;values.forEach((v,i)=>t.expect(v));t.snapshot(grow);
const shrink=t.object('shrink',new Array(3)),sg=t.bind('sg',()=>{shrink.length=1;return 5;});t.define(shrink,'0',{get:sg,enumerable:true,configurable:true});
values=[...shrink];observe('counts',[values.length]);;values.forEach((v,i)=>t.expect(v));
const ar=t.object('ar',[1,,3]);undefined;let calls=0;undefined;const result=ar.filter(v=>{calls++;if(v===1){delete ar[2];ar[1]=2;ar[3]=4;}return true;});
undefined;t.expect(result);observe('counts',[calls]);;t.snapshot(ar);
const holes=t.object('holes',new Array(3));t.set(holes,'2',NaN);t.define(Array.prototype,'1',{value:7,writable:true,enumerable:true,configurable:true});
for(const needle of [undefined,7,NaN])t.operation(()=>holes.indexOf(needle));
values=[...holes];observe('counts',[values.length]);;values.forEach((v,i)=>t.expect(v));delete Array.prototype[1];undefined;t.finish();
}
{
const t=new Case('descriptor_edge_cases');const a=t.object('a',{});t.define(a,'nan',{value:NaN,writable:false,configurable:false});t.define(a,'zero',{value:0,writable:false,configurable:false});t.define(a,'nan',{value:NaN});
t.throws(()=>Object.defineProperty(a,'zero',{value:-0}));
const ar=t.object('ar',new Array(5));t.define(ar,'2',{value:2,writable:true,enumerable:true,configurable:false});t.set(ar,'4',4);
t.throws(()=>Object.defineProperty(ar,'length',{value:1,writable:false}));t.snapshot(ar);
t.throws(()=>{ar[3]=3;});t.throws(()=>{ar.length=1.5;});t.finish();
}
{
const t=new Case('getter_evaluation_order');let constructorReads=0;undefined;const a=t.object('a',{}),g=t.bind('g',()=>{constructorReads++;return O.Expandable;});t.define(a,'constructor',{get:g,configurable:true});t.operation(()=>O.isObject(a));observe('counts',[constructorReads]);;
const dst=t.object('dst',{}),ar=t.object('ar',[1,2,3]);for(const[i,v]of ar.entries())undefined;t.set(dst,'items',ar);
const rm=t.object('rm',[2]);undefined;let removes=0;undefined;const rg=t.bind('rg',()=>{removes++;return rm;}),edits=t.object('edits',{});t.define(edits,'[-]',{get:rg,enumerable:true,configurable:true});const src=t.object('src',{});t.set(src,'items',edits);t.operation(()=>O.insert(dst,src,false));t.snapshot(dst);observe('counts',[removes]);;
const first=t.object('first',[8]),second=t.object('second',[9]);undefined;let selected=first,appends=0;undefined;const left=t.object('left',new Array(1)),lg=t.bind('lg',()=>{selected=second;return 1;});t.define(left,'0',{get:lg,enumerable:true,configurable:true});t.set(dst,'items',left);
const ag=t.bind('ag',()=>{appends++;return selected;}),add=t.object('add',{});t.define(add,'[+]',{get:ag,enumerable:true,configurable:true});t.set(src,'items',add);t.operation(()=>O.insert(dst,src,false));t.snapshot(dst);observe('counts',[appends]);;t.finish();
}
{
const t=new Case('descriptor_conversion_order');const description=t.object('description',{});let events=[];undefined;
for(const [i,name]of ['enumerable','configurable','value','writable','get','set'].entries()){
const v=name==='value'?4:name==='get'||name==='set'?undefined:true;const getter=t.bind('field_'+i,()=>{events.push(name);return v;});t.define(description,name,{get:getter,enumerable:true,configurable:true});}
t.throws(()=>Object.defineProperty({},'x',description));observe('strings',events.map(units));;t.finish();
}
{
const t=new Case('copy_abrupt_mutation');const source=t.object('source',{}),child=t.object('child',{});t.set(source,'child',child);t.set(source,'later',7);
const g=t.bind('g',()=>{delete source.later;return Object;});t.define(child,'constructor',{get:g,configurable:true});t.throws(()=>O.copy(source));t.snapshot(source);t.finish();
}
{
const t=new Case('accessor_receiver');const p=t.object('p',{}),target=t.object('target',Object.create(p));let seen;undefined;
const g=t.bind('g',function(){return this;}),s=t.bind('s',function(){seen=this;});t.define(p,'x',{get:g,set:s,enumerable:true,configurable:true});t.operation(()=>target.x);t.set(target,'x',3);t.expect(seen);t.finish();
}
{
const t=new Case('deep_valid_copy');const input={};let cursor=input;for(let i=0;i<511;++i)cursor=cursor.next={};cursor.leaf=7;const copied=O.copy(input);let leaf=copied;for(let i=0;i<511;++i)leaf=leaf.next;assert.equal(leaf.leaf,7);
observe('boolean',true);observe('boolean',copied!==input);observe('value',t.value(leaf.leaf));;t.finish();
}
{
const t=new Case('warning_utf8_sink');const messages=['𝔸','x\ud800','x\udc00','\ud800x'];
const script=`const O=require(${JSON.stringify(path.join(npmRoot,'cjs/util/Options.js'))});for(const m of ${JSON.stringify(messages)})O.OPTIONS.optionError(m,'key');`;
const plain=cp.spawnSync(process.execPath,['-e',script]);assert.equal(plain.status,0);
undefined;
for(const m of messages)undefined;
observe('bytes',Array.from(plain.stderr));;t.finish();
}

assert.ok(process.argv.includes('--check'),'Source-only owner supports --check');const gold=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p60-options-source.json'),'utf8'));assert.equal(sourceCases.length,gold.cases.length);for(let i=0;i<sourceCases.length;i++){assert.equal(sourceCases[i].name,gold.cases[i].name);assert.equal(sourceCases[i].observations.length,gold.cases[i].observations.length,sourceCases[i].name);for(let j=0;j<sourceCases[i].observations.length;j++)assert.deepEqual(sourceCases[i].observations[j],gold.cases[i].observations[j],sourceCases[i].name+'/'+j);}console.log(JSON.stringify({cases:sourceCases.length,assertions:sourceCases.reduce((n,c)=>n+c.observations.length,0),mode:'check'}));
