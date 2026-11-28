'use strict';
// Independent Source witness for default TagFormat callbacks and emitted
// TagFormat prototype order. This intentionally does not alter the locked
// 23-case TagFormat/SetOptions oracle output.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../../..'),bundle=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const lock=JSON.parse(fs.readFileSync(path.join(__dirname,'source-lock.json'),'utf8'));
assert.equal(digest(path.join(root,'tools/math-port/profiles/scope.json')),lock.scopeSha256,'scope.json differs from the locked profile');
for(const entry of lock.sources){
  assert.equal(digest(path.join(root,'.reference/MathJax-src',entry.file)),entry.sha256,`${entry.file} differs from its Source lock`);
  if(entry.cjsFile)assert.equal(digest(path.join(root,entry.cjsFile)),entry.cjsSha256,`${entry.cjsFile} differs from its executable Source lock`);
}
const load=name=>require(path.join(bundle,name));
const {TeX}=load('input/tex.js');
load('input/tex/base/BaseConfiguration.js');
load('input/tex/ams/AmsConfiguration.js');
load('input/tex/tagformat/TagFormatConfiguration.js');
const jax=new TeX({packages:['base','ams','tagformat']});
const format=jax.parseOptions.options.tagformat;
const tags=jax.parseOptions.tags,prototype=Object.getPrototypeOf(tags);
function plain(value,seen=new WeakSet()){
  if(value===undefined)return{$:'undefined'};
  if(value===null||typeof value==='string'||typeof value==='boolean')return value;
  if(typeof value==='number')return Number.isNaN(value)?{$:'NaN'}:Object.is(value,-0)?{$:'-0'}:Number.isFinite(value)?value:{$:value<0?'-Infinity':'Infinity'};
  if(typeof value==='function')return{$:'function',name:value.name||'',length:value.length};
  if(value instanceof RegExp)return{$:'regexp',source:value.source,flags:value.flags};
  if(typeof value!=='object')return String(value);
  if(seen.has(value))return{$:'cycle'};seen.add(value);
  if(Array.isArray(value))return Array.from({length:value.length},(_,i)=>Object.hasOwn(value,i)?plain(value[i],seen):{$:'hole'});
  const out={};for(const key of Reflect.ownKeys(value))out[typeof key==='symbol'?`symbol:${key.description||''}`:key]=plain(value[key],seen);return out;
}
function attempt(call){try{return{value:plain(call())};}catch(error){return{error:{name:error.name||error.constructor.name,message:String(error.message)}};}}
const callbackEvents=[];
const numberObject={toString(){callbackEvents.push({name:'number.toString',receiver:this===numberObject?'argument':'other',arity:arguments.length});return 'custom-number';}};
const number={ordinary:attempt(()=>format.number(12)),custom:attempt(()=>format.number(numberObject)),null:attempt(()=>format.number(null)),undefined:attempt(()=>format.number(undefined))};
const nativeReplace=String.prototype.replace;
let overriddenReplace;
try{
  String.prototype.replace=function(...args){
    callbackEvents.push({name:'String.prototype.replace',receiver:this==='a b'?'argument':String(this),args:plain(args)});
    return 'patched-id';
  };
  overriddenReplace=attempt(()=>format.id('a b'));
}finally{String.prototype.replace=nativeReplace;}
const idObject={replace(pattern,replacement){
  callbackEvents.push({name:'id.replace',receiver:this===idObject?'argument':'other',arity:arguments.length,pattern:plain(pattern),replacement});
  return {[Symbol.toPrimitive](hint){callbackEvents.push({name:'id.result.toPrimitive',hint});return 'custom-id';}};
}};
const id={ordinary:attempt(()=>format.id('alpha beta')),customReplace:attempt(()=>format.id(idObject)),null:attempt(()=>format.id(null)),undefined:attempt(()=>format.id(undefined)),nonString:attempt(()=>format.id(7)),stringReplaceOverride:overriddenReplace};
const names=Reflect.ownKeys(prototype).filter(name=>typeof name==='string');
const methods=['formatNumber','formatTag','formatRef','formatId','formatUrl'];
const prototypeSurface={ownKeys:names,methods:methods.map(name=>{
  const descriptor=Object.getOwnPropertyDescriptor(prototype,name);
  return{name,descriptor:descriptor?{value:plain(descriptor.value),writable:descriptor.writable,enumerable:descriptor.enumerable,configurable:descriptor.configurable}:null};
})};
const output={schema:1,sourceLockSha256:digest(path.join(__dirname,'source-lock.json')),number,id,prototypeSurface,callbackEvents};
const destination=path.join(root,'build/math-port/tag-setoptions-default-methods-original.json');
fs.mkdirSync(path.dirname(destination),{recursive:true});fs.writeFileSync(destination,JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify({output:path.relative(root,destination),sourceLockSha256:output.sourceLockSha256,methods:names.filter(name=>methods.includes(name))}));
