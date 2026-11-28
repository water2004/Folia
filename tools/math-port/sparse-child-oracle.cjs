// Root Source evidence for the forthcoming single sparse-child migration.
// No native module or proposed storage implementation contributes expected values.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const pins={'core/Tree/Node.js':'94174004b89f19bf3c98e8eec5fabdd0bcbf1e13e0b740963fa69554ad8a12a7','input/tex/NodeUtil.js':'4a55841fd22167d25f505e8f41adea072715ef024e00ea5ac6df3f199f2cb809'};
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
for(const[file,pin]of Object.entries(pins))assert.equal(hash(fs.readFileSync(path.join(base,file))),pin);
const {AbstractNode}=require(path.join(base,'core/Tree/Node.js')),NodeUtil=require(path.join(base,'input/tex/NodeUtil.js')).default;
assert.equal(typeof NodeUtil.copyChildren,'function');assert.equal(typeof NodeUtil.setChild,'function');
const cases=[];
function trial(name,run){
 const names=new WeakMap(),nodes={},factory={create(){const n=new AbstractNode(factory);names.set(n,'copy'+Object.keys(nodes).length);nodes[names.get(n)]=n;return n;},nodeIsKind(){return true;}};
 for(const name of ['old','new','a','b','c']){nodes[name]=new AbstractNode(factory);names.set(nodes[name],name);}
 const tag=value=>value===undefined?'U':value===null?'N':names.get(value)??'UNBOUND';
 const array=tags=>{const a=new Array(tags.length);for(let i=0;i<tags.length;i++)if(tags[i]!=='H')a[i]=tags[i]==='U'?undefined:tags[i]==='N'?null:nodes[tags[i]];return a;};
 const snapshot=a=>({length:a.length,own:Array.from({length:a.length},(_,i)=>Object.hasOwn(a,i)),has:Array.from({length:a.length},(_,i)=>i in a),values:Array.from({length:a.length},(_,i)=>tag(a[i]))});
 const parents=()=>Object.fromEntries(Object.entries(nodes).map(([k,v])=>[k,tag(v.parent)]));
 let error=null,extra={};try{extra=run({nodes,array,tag,snapshot})??{};}catch(e){error={name:e.name,message:e.message};}
 cases.push({name,error,old:snapshot(nodes.old.childNodes),new:snapshot(nodes.new.childNodes),parents:parents(),extra});
}
const states=['H','U','N','a','b'];
for(const first of states)for(const second of states){
 const shape=[first,second,'c'];
 trial('copyChildren-'+shape.join('-'),({nodes,array,snapshot})=>{nodes.old.childNodes=array(shape);nodes.new.childNodes=array(['b']);const captured=nodes.old.childNodes;NodeUtil.copyChildren(nodes.old,nodes.new);return {captured:snapshot(captured),alias:captured===nodes.old.childNodes};});
 trial('setChildren-'+shape.join('-'),({nodes,array,snapshot})=>{const input=array(shape);nodes.old.childNodes=array(['b']);const captured=nodes.old.childNodes;try{nodes.old.setChildren(input);}finally{nodes.new.childNodes=input;nodes.new.properties.captured=captured;}});
 trial('copy-'+shape.join('-'),({nodes,array,snapshot})=>{nodes.old.childNodes=array(shape);const copied=nodes.old.copy();nodes.new.childNodes=copied.childNodes;return {sameArray:copied.childNodes===nodes.old.childNodes};});
}
for(const value of states.filter(v=>v!=='H'))for(const index of [0,1,2,5])trial('setChild-'+index+'-'+value,({nodes,array})=>{nodes.old.childNodes=array(['a']);NodeUtil.setChild(nodes.old,index,array([value])[0]);});
for(const value of ['U','N','a']){
 trial('appendChild-'+value,({nodes,array})=>{nodes.old.childNodes=array(['b']);return {result:nodes.old.appendChild(array([value])[0])===array([value])[0]};});
 trial('replaceChild-'+value,({nodes,array})=>{nodes.old.childNodes=array(['a','b']);nodes.a.parent=nodes.old;const v=array([value])[0];nodes.old.replaceChild(v,nodes.a);});
 trial('removeChild-'+value,({nodes,array})=>{const v=array([value])[0];nodes.old.childNodes=array(['a',value,'b']);nodes.old.removeChild(v);});
}
for(const target of ['U','N','a','b','c'])trial('childIndex-hole-present-'+target,({nodes,array})=>{nodes.old.childNodes=array(['H','U','N','a','b']);return {index:nodes.old.childIndex(array([target])[0])};});
trial('superscript-first-and-subscript',({nodes,array,snapshot})=>{nodes.old.childNodes=array(['a']);NodeUtil.setChild(nodes.old,2,nodes.c);const superscript=snapshot(nodes.old.childNodes);NodeUtil.copyChildren(nodes.old,nodes.new);const copied=snapshot(nodes.new.childNodes);NodeUtil.setChild(nodes.old,1,nodes.b);return {superscript,copied};});
trial('copyChildren-live-source-length',({nodes,array})=>{nodes.old.childNodes=array(['a','b']);const original=NodeUtil.setChild;const receiver=Object.create(NodeUtil);receiver.setChild=function(n,i,c){original.call(this,n,i,c);if(i===0)nodes.old.childNodes.push(nodes.c);};receiver.copyChildren(nodes.old,nodes.new);});
trial('copyChildren-captured-source-rebind',({nodes,array,snapshot})=>{nodes.old.childNodes=array(['a','b']);const captured=nodes.old.childNodes,original=NodeUtil.setChild,receiver=Object.create(NodeUtil);receiver.setChild=function(n,i,c){original.call(this,n,i,c);if(i===0)nodes.old.childNodes=array(['c']);};receiver.copyChildren(nodes.old,nodes.new);return {captured:snapshot(captured)};});
trial('copyChildren-live-source-delete',({nodes,array})=>{nodes.old.childNodes=array(['a','b','c']);const original=NodeUtil.setChild,receiver=Object.create(NodeUtil);receiver.setChild=function(n,i,c){original.call(this,n,i,c);if(i===0)delete nodes.old.childNodes[1];};receiver.copyChildren(nodes.old,nodes.new);});
trial('copyChildren-inherited-numeric-slot',({nodes,array,snapshot})=>{nodes.old.childNodes=array(['a','H','c']);const proto=[];proto[1]=nodes.b;Object.setPrototypeOf(nodes.old.childNodes,proto);const before=snapshot(nodes.old.childNodes);NodeUtil.copyChildren(nodes.old,nodes.new);return {before};});
trial('setChild-captured-target-before-parent-setter',({nodes,array,snapshot})=>{nodes.old.childNodes=array(['a']);const captured=nodes.old.childNodes;let parent=null;Object.defineProperty(nodes.b,'parent',{get(){return parent;},set(v){parent=v;nodes.old.childNodes=array(['c']);}});NodeUtil.setChild(nodes.old,3,nodes.b);return {captured:snapshot(captured)};});
const out=path.join(root,'tools/math-port/profiles/evidence/p23-sparse-child-source.json'),data=JSON.stringify({schema:1,status:'Root original Source evidence; concrete native declarations/red tests/migration not yet frozen',pins,cases},null,2)+'\n';
assert.equal(cases.filter(c=>c.name.startsWith('copyChildren-')&&c.error).length,0,'Source adapters must actually call NodeUtil');
assert.ok(cases.every(c=>c.old.values.concat(c.new.values).every(v=>v!=='UNBOUND')),'All observed node identities must be explicitly registered');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(out));fs.writeFileSync(out,data);}
console.log(JSON.stringify({cases:cases.length,actual_source_errors:cases.filter(c=>c.error).length,source_sha256:hash(data)}));
