// Root observes actual Source return values before any native signature migration.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
assert.equal(process.versions.node,'24.13.0');
const prerequisiteText=fs.readFileSync(path.join(root,'tools/math-port/profiles/evidence/p23b-sparse-mml-source.json'),'utf8').replaceAll('\r\n','\n');
const prerequisite=JSON.parse(prerequisiteText);
for(const[file,pin]of Object.entries(prerequisite.loadedPins))assert.equal(hash(fs.readFileSync(path.join(base,file))),pin);
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js'));
const {MmlMrow}=require(path.join(base,'core/MmlTree/MmlNodes/mrow.js'));
const originalEmbellished=Object.getOwnPropertyDescriptor(MmlMrow.prototype,'isEmbellished').get;
const cases=[];
function trial(name,run){
 const factory=new MmlFactory(),ids=new WeakMap(),nodes={},arrays={},trace=[];
 const create=(kind,label)=>{const n=factory.create(kind);ids.set(n,label);nodes[label]=n;return n;};
 const a=create('mi','a'),b=create('mo','b'),c=create('mi','c');
 const tag=v=>v===undefined?'U':v===null?'N':typeof v==='boolean'?v:ids.get(v)??'UNBOUND';
 const array=shape=>{const out=new Array(shape.length);shape.forEach((v,i)=>{if(v!=='H')out[i]=v==='U'?undefined:v==='N'?null:nodes[v];});return out;};
 const snapshot=out=>({length:out.length,own:Array.from({length:out.length},(_,i)=>Object.hasOwn(out,i)),values:Array.from({length:out.length},(_,i)=>tag(out[i]))});
 let result,error=null;try{result=tag(run({factory,create,nodes,array,arrays,trace,tag}));}catch(e){error={name:e.name,message:e.message};}
 cases.push({name,result:result===undefined?'THREW':result,error,nodes:Object.fromEntries(Object.entries(nodes).map(([k,n])=>[k,{parent:tag(n.parent),children:snapshot(n.childNodes)}])),captured:Object.fromEntries(Object.entries(arrays).map(([k,a])=>[k,snapshot(a)])),trace});
}
for(const kind of ['mstyle','mfrac'])for(const state of ['H','U','N','a'])for(const method of ['core','coreMO'])trial(kind+'-'+method+'-'+state,c=>{const n=c.create(kind,'target');n.childNodes=c.array([state]);return n[method]();});
for(const state of ['H','U','N','a'])trial('mstyle-notParent-'+state,c=>{const n=c.create('mstyle','target');n.childNodes=c.array([state]);return n.notParent;});
for(const state of ['H','U','N','a'])for(const method of ['core','coreMO'])trial('mrow-'+method+'-getter-rebind-'+state,c=>{
 const n=c.create('mrow','target');n.childNodes=c.array(['b']);c.arrays.original=n.childNodes;
 Object.defineProperty(n,'isEmbellished',{get(){const result=originalEmbellished.call(this);c.trace.push('embellished:'+result);n.childNodes=c.array([state]);return result;}});
 return n[method]();
});
for(const state of ['H','U','N','a'])trial('mstyle-notParent-flag-length-'+state,c=>{const n=c.create('mstyle','target');n.childNodes=c.array([state,'b']);return n.notParent;});
assert.equal(cases.length,32);assert.ok(!JSON.stringify(cases).includes('UNBOUND'));
const data=JSON.stringify({schema:1,status:'Root original Source return-value evidence; no native API acceptance',node:process.versions.node,prerequisite_sha256:hash(prerequisiteText),cases},null,2)+'\n';
const out=path.join(root,'tools/math-port/profiles/evidence/p23b-sparse-mml-return-source.json');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(out));fs.writeFileSync(out,data);}
console.log(JSON.stringify({cases:cases.length,actual_source_errors:cases.filter(c=>c.error).length,undefined_returns:cases.filter(c=>c.result==='U').length,null_returns:cases.filter(c=>c.result==='N').length,sha256:hash(data)}));
