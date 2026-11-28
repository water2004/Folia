// Root pinned actual base verification/defaulting and live child iteration.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const candidateRoot=path.resolve(__dirname,'../..');
const {root,MmlFactory,core,hashes:sourcePins}=require(path.join(candidateRoot,'tools/math-port/inheritance-reference.cjs'));
const options=['omitted','U','N','false','true','0','7','empty','raw','NaN','object','array'],arities=['-Inf','-1','-0','0','1','2','Inf','NaN'],inputs=[];
const base={op:'tree',options:'object',arity:'2',shape:'ab',check:'true',mode:''};
for(const option of options)for(const arity of arities)for(const shape of ['','a','ab','null'])for(const check of ['U','false','true'])inputs.push({...base,options:option,arity,shape,check});
for(const mode of ['attrs-throw','attrs-null-array','attrs-rebind','attrs-enable','attrs-disable','arity-throw','arity-null-array','arity-rebind','error-throw','error-null-array','error-disable','children-throw','children-seen'])for(const option of ['U','N','false','0','raw','object','array'])for(const arity of ['-1','1','2','NaN'])for(const shape of ['','ab'])inputs.push({...base,options:option,arity,shape,mode});
for(const shape of ['','H','U','N','a','b','ab','ba','aba','aNb','prototype','null'])for(const option of options)for(const mode of ['','child-grow','child-delete','child-shrink','child-rebind','child-prototype','child-throw','child-seen','child-null-array'])inputs.push({...base,op:'children',options:option,shape,mode});
const number=s=>s==='-Inf'?-Infinity:s==='Inf'?Infinity:s==='NaN'?NaN:Number(s);
const scalar=v=>v===undefined?'U':v===null?'N':typeof v==='boolean'?String(v):typeof v==='number'?(Object.is(v,-0)?'-0':String(v)):typeof v==='string'?'s:'+v:Array.isArray(v)?'a:'+v.id:'o:'+v.id;
const flag=s=>s==='U'?undefined:s==='N'?null:s==='false'?false:s==='true'?true:s==='NaN'?NaN:s==='empty'?'':s==='raw'?'raw':s==='obj'?{id:'check'}:s==='array'?Object.assign([],{id:'check'}):Number(s);
function run(i){let active=false,trace=[],n,a,b,c,original,opts;const ids=new Map(),note=s=>{if(active)trace.push(s);},fail=()=>{throw Object.assign(new Error('witness'),{name:'WitnessError'});};
 const opt=v=>scalar(v)+((v&&typeof v==='object')?'/'+scalar(v.checkArity)+'/'+scalar(v.seen):'');
 const set=(v,k,x)=>{if(v&&typeof v==='object')v[k]=x;};
 class Record extends core.AbstractMmlNode{get kind(){return 'verify-record';}verifyTree(o){note('verify:'+ids.get(this)+'/'+opt(o)+'@'+Number(o===opts));if(this===a){if(i.mode==='child-throw')fail();if(i.mode==='child-grow'){original.push(c);n.childNodes=[];}if(i.mode==='child-delete')delete original[1];if(i.mode==='child-shrink')original.length=1;if(i.mode==='child-rebind')n.childNodes=[c];if(i.mode==='child-prototype'){const p=[];p[1]=c;Object.setPrototypeOf(original,p);delete original[1];}if(i.mode==='child-null-array')n.childNodes=null;if(i.mode==='child-seen')set(o,'seen',true);}}}
 class Probe extends Record{get kind(){return 'verify-probe';}get arity(){note('arity');if(active){if(i.mode==='arity-throw')fail();if(i.mode==='arity-null-array')n.childNodes=null;if(i.mode==='arity-rebind')n.childNodes=[b];return number(i.arity);}return Infinity;}verifyAttributes(o){note('attributes:'+opt(o)+'@'+Number(o===opts));if(i.mode==='attrs-throw')fail();if(i.mode==='attrs-null-array')n.childNodes=null;if(i.mode==='attrs-rebind')n.childNodes=[b];if(i.mode==='attrs-enable')set(o,'checkArity',true);if(i.mode==='attrs-disable')set(o,'checkArity',false);}verifyChildren(o){note('children:'+opt(o)+'@'+Number(o===opts));if(i.mode==='children-throw')fail();if(i.mode==='children-seen')set(o,'seen',true);}mError(message,o,short){note('error:'+message+'/'+opt(o)+'@'+Number(o===opts)+'/'+String(short));if(i.mode==='error-throw')fail();if(i.mode==='error-null-array')n.childNodes=null;if(i.mode==='error-disable')set(o,'checkArity',false);return b;}}
 const f=new MmlFactory();f.setNodeClass('verify-probe',Probe);f.setNodeClass('verify-record',Record);n=f.create('verify-probe');[a,b,c]=Array.from({length:3},()=>f.create('verify-record'));for(const[node,id]of [[n,'n'],[a,'a'],[b,'b'],[c,'c']])ids.set(node,id);
 const value=s=>s==='U'?undefined:s==='N'?null:s==='a'?a:s==='b'?b:c;
 const array=s=>{if(s==='null')return null;if(s==='prototype'){const p=[a,b],v=Array(2);Object.setPrototypeOf(v,p);return v;}const v=Array(s.length);for(let j=0;j<s.length;j++)if(s[j]!=='H')v[j]=value(s[j]);return v;};
 original=n.childNodes=array(i.shape);opts=i.options==='object'?{id:'opts',checkArity:flag(i.check)}:i.options==='array'?Object.assign([],{id:'opts',checkArity:flag(i.check)}):i.options==='omitted'||i.options==='U'?undefined:i.options==='N'?null:i.options==='false'?false:i.options==='true'?true:i.options==='0'?0:i.options==='7'?7:i.options==='NaN'?NaN:i.options==='empty'?'':'raw';
 const tag=v=>v===undefined?'U':v===null?'N':ids.get(v)??'UNBOUND';
 const snapshot=v=>v===null?'N':v.length+':'+Array.from({length:v.length},(_,j)=>Number(Object.hasOwn(v,j))).join('')+':'+Array.from({length:v.length},(_,j)=>Number(j in v)).join('')+':'+Array.from({length:v.length},(_,j)=>tag(v[j])).join(',');
 let error='ok';active=true;try{const method=i.op==='tree'?core.AbstractMmlNode.prototype.verifyTree:core.AbstractMmlNode.prototype.verifyChildren;if(i.options==='omitted')method.call(n);else method.call(n,opts);}catch(e){error=e.name+'|'+e.message;}active=false;
 return error+'|'+opt(opts)+'|'+snapshot(n.childNodes)+'|'+snapshot(original)+'|'+trace.join(',');
}
const packageLock=require(path.join(root,'tools/math-port/mo-pattern-reference.cjs')).lock;
assert.equal(packageLock.version,'4.1.3');assert.equal(packageLock.node,process.version);
assert.deepEqual(packageLock.compiled_package,{files:424,sha256:'773bf3ae1c075baa36c02b6958f7e3bed5fb116d0818ecf99bb70c5337d9e537'});
const nodeSource=sourcePins.find(pin=>pin.source==='ts/core/MmlTree/MmlNode.ts');
assert.ok(nodeSource);assert.equal(nodeSource.sha256,'cc0594eb5782928412c4307637866d9f80ac147af577297edd48ee88b63b8656');
const nodeRuntime=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNode.js');
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(nodeRuntime)).digest('hex'),'86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45');
const sourceFile=path.join(root,'tools/math-port/profiles/evidence/p32-verification-source.json');
const flagsFile=path.join(root,'tools/math-port/profiles/evidence/p32-verification-flags-source.json');
const sourceBytes=fs.readFileSync(sourceFile),flagsBytes=fs.readFileSync(flagsFile);
assert.equal(crypto.createHash('sha256').update(sourceBytes).digest('hex'),'020e72f32331a16e1ac42d7da65491d958b7b5a6a90923f2a729b02904aa61b7');
assert.equal(crypto.createHash('sha256').update(flagsBytes).digest('hex'),'60578d1cc08ed3b283d8d8c1080e782325ed391b6e467213c1d3f642b53308b0');
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';
assert.equal(rows.length,3176);assert.equal(rows.filter(row=>!row.expected.startsWith('ok|')).length,748);
assert.equal(sourceBytes.toString('utf8').replaceAll('\r\n','\n'),data,'p32 Source profile differs from the complete ordered Source replay');
const flags=JSON.parse(flagsBytes.toString('utf8'));
assert.equal(flags.length,576);
for(const row of flags)assert.equal(run(row.input),row.expected,`p32 flags Source replay mismatch: ${JSON.stringify(row.input)}`);
assert.equal(flags.filter(row=>!row.expected.startsWith('ok|')).length,32);
const result={sourceCases:rows.length,sourceErrors:748,flagCases:flags.length,flagErrors:32,totalSourceCases:rows.length+flags.length,totalSourceErrors:780,profileSha256:crypto.createHash('sha256').update(sourceBytes).digest('hex'),flagsSha256:crypto.createHash('sha256').update(flagsBytes).digest('hex'),mode:'check-only'};
assert.equal(result.totalSourceCases,3752);assert.equal(result.totalSourceErrors,780);
if(require.main===module){assert.ok(process.argv.includes('--check'),'Source owner is check-only; pass --check');console.log(JSON.stringify(result));}
module.exports={run};

