'use strict';
// Actual pinned CommonOutputJax delegation, without constructing substitute Jax
// or font implementations. Borrowing the real prototype method is observable.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scopeFile=path.join(root,'tools/math-port/profiles/scope.json');
const scope=JSON.parse(fs.readFileSync(scopeFile));
for(const lock of scope.files)assert.equal(sha(path.join(root,'.reference/MathJax-src',lock.source)),lock.source_sha256,lock.source);
const sourceFile=path.join(root,'.reference/MathJax-src/ts/output/common.ts');
const compiledFile=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/output/common.js');
const {CommonOutputJax}=require(compiledFile);
const method=CommonOutputJax.prototype.addExtension;
const descriptor=Object.getOwnPropertyDescriptor(CommonOutputJax.prototype,'addExtension');
const metadata={name:method.name,length:method.length,writable:descriptor.writable,
  enumerable:descriptor.enumerable,configurable:descriptor.configurable};
assert.deepEqual(metadata,{name:'',length:2,writable:true,enumerable:true,configurable:true});
const records=[];
for(const name of ['omitted','undefined','null','object','extra']){
  const events=[],data={},rawPrefix={},returned={},font={};
  const callback=function(...args){events.push('call');records.push({name,
    receiverIsFont:this===font,argsLength:args.length,dataIsOriginal:args[0]===data,
    prefix:args[1]===rawPrefix?'same-object':args[1],events:[...events]});return returned;};
  Object.defineProperty(callback,'call',{get(){throw Error('direct invocation read function.call');}});
  Object.defineProperty(font,'addExtension',{get(){events.push('method');return callback;}});
  const receiver={get font(){events.push('font');return font;}};
  const args=name==='omitted'?[data]:name==='undefined'?[data,undefined]:
    name==='null'?[data,null]:name==='object'?[data,rawPrefix]:[data,null,'ignored'];
  const result=Reflect.apply(method,receiver,args);
  records.at(-1).sameReturn=result===returned;
  assert.deepEqual(events,['font','method','call']);
}
{
  const marker={},font={addExtension(){throw marker;}};
  let sameThrow=false;try{Reflect.apply(method,{font},[]);}catch(e){sameThrow=e===marker;}
  records.push({name:'same-throw',sameThrow});assert.equal(sameThrow,true);
}
for(const [name,font] of [['null-font',null],['undefined-font',undefined],['non-callable',{addExtension:7}]]){
  let error=null;try{Reflect.apply(method,{font},[]);}catch(e){error={name:e.name,message:e.message};}
  records.push({name,error});assert.equal(error?.name,'TypeError');
}
const output={scopeSha256:sha(scopeFile),sourceSha256:sha(sourceFile),
  cjsSha256:sha(compiledFile),sourceLocks:scope.files.length,metadata,records};
const destination=path.join(root,'build/math-port/common-jax-extension-original.json');
fs.writeFileSync(destination,JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify({destination,sourceLocks:scope.files.length,metadata,records}));
