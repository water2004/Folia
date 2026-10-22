// Root actual CJS comparisons for default ownership and raw attribute flow.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const crypto=require('node:crypto');
const {root,MmlFactory,core}=require('./inheritance-reference.cjs');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
let active=false,events=[],calls=[],raw,mode='';const note=s=>{if(active)events.push(s);};
class Plain extends core.AbstractMmlNode{get kind(){return 'plain';}}
function proxy(T){return class extends T{get kind(){note('kind');return super.kind;}get attributes(){note('attributes');return super.attributes;}set attributes(a){super.attributes=a;}
setChildInheritedAttributes(a,d,l,p){note('hook');calls.push({a,d,l,p,same:a===raw});if(mode==='throw')throw Object.assign(new Error('witness'),{name:'WitnessError'});}
add(a,b){return super.addInheritedAttributes(a,b);}};}
function setup(kind,attrs={}){active=false;events=[];calls=[];mode='';const f=new MmlFactory();f.setNodeClass('plain',Plain);const T=f.getNodeClass(kind);f.setNodeClass('probe-'+kind,proxy(T));const n=f.create('probe-'+kind,attrs);let captured=n.attributes;Object.defineProperty(n,'attributes',{get(){note('attributes');return captured;},set(a){captured=a;},configurable:true});return {f,n};}
function state(n){const a=n.attributes;return {inherited:a.getAllInherited(),explicit:a.getAllAttributes(),prime:n.getProperty('texprimestyle'),calls,events};}
function observed(run,n){let error='';active=true;try{run();}catch(e){error=e.name;}active=false;return {error,...state(n)};}
let assertions=0;const cases=[];
function test(name,wanted){++assertions;cases.push({name:'math_port_raw_inheritance_'+name,expected:profileValue(wanted)});}
const values=[undefined,null,false,true,0,NaN,'','xy','😀\ud800',{mathcolor:['math','blue'],mathsize:['math','2em']},{mathcolor:'😀x'},{mathcolor:[null,'null-origin']},{mathcolor:[7,'number-origin']},{mathcolor:[]},{mathcolor:false},['x',null,3]];
for(const kind of ['plain','mi','mo','mstyle','mtable'])for(let i=0;i<values.length;++i)for(const count of [1,4]){
const {n}=setup(kind,{indentalign:'right'});raw=values[i];const args=[raw,undefined,undefined,undefined];const result=observed(()=>n.setInheritedAttributes(...args.slice(0,count)),n);
test(kind+'_'+i+'_'+count,result);
}
for(const kind of ['plain','mi','mo','mstyle','mtable'])for(const count of [0,2,3,4]){
const {n}=setup(kind);raw=undefined;const a=[raw,'raw-display',null,'raw-prime'];const result=observed(()=>n.setInheritedAttributes(...a.slice(0,count)),n);
test(kind+'_omitted_'+count,result);
}
for(const kind of ['plain','mi','mo','mstyle','mtable']){
const {n}=setup(kind);raw={mathcolor:['math','blue']};mode='throw';const result=observed(()=>n.setInheritedAttributes(raw,true,'2','yes'),n);
test(kind+'_hook_throw',result);
}
// Strings enumerate UTF16 indices, but string tuple destructuring iterates code points.
for(const tuple of ['', 'a','😀x','\ud800y', [undefined,7],[false,'false-origin'],[null,'null-origin'],[7,'number-origin'],{},false,7,{0:'math',1:'indexed-record'},null,undefined]){
const {n}=setup('plain');raw={mathcolor:tuple};const result=observed(()=>n.setInheritedAttributes(raw,false,0,false),n);
test('tuple_'+cases.length,result);
}
for(const current of [undefined,null,false,3,'😀x',[],{shared:['origin','value']},JSON.parse('{"__proto__":["origin","value"],"a":1}')])for(const additions of [undefined,null,false,'ab',{a:7,displaystyle:true,scriptlevel:3,style:'s'}]){
const {n}=setup('plain');let result,error='';active=true;try{result=n.add(current,additions);}catch(e){error=e.name;}active=false;
const keys=result?Object.keys(result):[],protoArray=result?Array.isArray(Object.getPrototypeOf(result)):false;
const same=result===current,nestedAlias=result?.shared===current?.shared;
test('spread_'+cases.length,{result,error,keys,protoArray,same,nestedAlias,events});
}
for(const kind of ['plain','mi','mo','mstyle']){
const {n}=setup(kind);const wanted=observed(()=>{n.setInheritedAttributes();n.setInheritedAttributes();},n);wanted.fresh=calls[0].a!==calls[1].a;
test(kind+'_fresh_defaults',wanted);
}
// Typed optional-chain lookup must accept raw tuple origins and avoid eager kind access.
for(const origin of [null,undefined,7,false,[],['mstyle']]){
const {n}=setup('plain');const key=String(origin);const old=core.AbstractMmlNode.noInherit[key];core.AbstractMmlNode.noInherit[key]={plain:{mathcolor:true}};raw={mathcolor:[origin,'blocked']};const result=observed(()=>n.setInheritedAttributes(raw,false,0,false),n);if(old===undefined)delete core.AbstractMmlNode.noInherit[key];else core.AbstractMmlNode.noInherit[key]=old;
test('raw_origin_'+cases.length,result);
}
function profileValue(v){if(v===undefined)return {type:'missing'};if(v===null)return {type:'null'};if(typeof v==='boolean')return {type:'boolean',value:v};if(typeof v==='number')return {type:'number',value:Number.isNaN(v)?'NaN':Object.is(v,-0)?'-0':v};if(typeof v==='string')return {type:'string',units:Array.from({length:v.length},(_,i)=>v.charCodeAt(i))};if(Array.isArray(v))return {type:'array',items:v.map(profileValue)};return {type:'object',entries:Object.keys(v).map(k=>[k,profileValue(v[k])])};}
const profilePath=path.join(root,'docs/math-port/evidence/inheritance-boundary-source-profile.json'),frozen=JSON.parse(fs.readFileSync(profilePath,'utf8'));
assert.equal(cases.length,frozen.cases.length);for(let i=0;i<cases.length;i++){assert.equal(cases[i].name,frozen.cases[i].name);assert.deepEqual(cases[i].expected,frozen.valuePool[frozen.cases[i].expectedRef]);}
console.log(JSON.stringify({cases:cases.length,assertions,source_values:frozen.valuePool.length,sourcePins:frozen.sourcePins.length,sourceSha256:hash(fs.readFileSync(path.join(root,'.reference/MathJax-src/ts/core/MmlTree/MmlNode.ts'))),mode:process.argv.includes('--check')?'check':'source-only-check'}));
