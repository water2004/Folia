'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json')));
for(const lock of scope.files)assert.equal(sha(path.join(root,'.reference/MathJax-src',lock.source)),lock.source_sha256);
const moduleFile=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/output/common/Wrapper.js');
const {CommonWrapper}=require(moduleFile);
const method=CommonWrapper.prototype.length2em,receiver={bbox:{scale:1},font:{params:{rule_thickness:.04}},jax:{pxPerEm:16}};
const plain=n=>typeof n==='undefined'?{$:'undefined'}:typeof n==='symbol'?{$:'symbol'}:
 typeof n==='number'&&!Number.isFinite(n)?{$:String(n)}:Object.is(n,-0)?{$:'-0'}:n;
const records=[];
for(const [name,value] of [['zero',0],['negative-zero',-0],['number',2],['true',true],['false',false],['null',null],['undefined',undefined],['nan',NaN],['symbol',Symbol('length')],['empty',''],['medium','medium'],['thin','thin'],['pixels','2px']]){
 let result,error=null;try{result=method.call(receiver,value);}catch(e){error={name:e.name,message:e.message};}
 records.push({name,input:plain(value),result:plain(result),error});
}
for(const [name,keys] of [['different-keys',['medium','thin']],['unknown-key',['other','2px']]]){
 let index=0;const events=[];const value={toString(){events.push('toString');return keys[index++];}};
 const result=method.call(receiver,value);records.push({name,result,events,conversions:index});
}
const output={sourceLocks:scope.files.length,cjsSha256:sha(moduleFile),records};
fs.writeFileSync(path.join(root,'build/math-port/common-wrapper-length-original.json'),JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify(output));
