// Root evidence from original MathJax MML classes, with observer-only bindings.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
assert.equal(process.versions.node,'24.13.0');
const pins={
 'core/Tree/Node.js':'94174004b89f19bf3c98e8eec5fabdd0bcbf1e13e0b740963fa69554ad8a12a7',
 'core/MmlTree/MmlNode.js':'86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
 'core/MmlTree/MmlFactory.js':'d0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
 'core/MmlTree/MML.js':'380332b7f0cc13104130affb65532ebca2ac214ccd897b93edda551cea443684',
 'core/MmlTree/MmlNodes/mrow.js':'5846d2b0334223b759a4059dfb0ac5e31aef1f5a96c041da45b3de94c0689896',
 'core/MmlTree/MmlNodes/msubsup.js':'7971b391a8d7c92e0d6115b17c4c394dafc828942fc00591ba6064b871610c18',
 'core/MmlTree/MmlNodes/mfrac.js':'b2e053ce68304c03ea740fb22baa5b01a28d540002faeddc30ad5720a4172770',
 'core/MmlTree/MmlNodes/mi.js':'47058d7ddb5b24ed530936baffe834a06c0557ec7dbdc03312d3ffa0aca17241',
 'input/tex/NodeUtil.js':'4a55841fd22167d25f505e8f41adea072715ef024e00ea5ac6df3f199f2cb809'
};
for(const[file,pin]of Object.entries(pins))assert.equal(hash(fs.readFileSync(path.join(base,file))),pin);
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js'));
const NodeUtil=require(path.join(base,'input/tex/NodeUtil.js')).default;
const cases=[];
function trial(name,run){
 const ids=new WeakMap(),created=[];
 class ObservedFactory extends MmlFactory{create(...args){const node=super.create(...args);if(!ids.has(node)){ids.set(node,'n'+created.length);created.push(node);}return node;}}
 const factory=new ObservedFactory(),nodes={};
 for(const label of ['a','b','c'])nodes[label]=factory.create('mi');
 const tag=value=>value===undefined?'U':value===null?'N':ids.get(value)??'UNBOUND';
 const array=shape=>{const out=new Array(shape.length);for(let i=0;i<shape.length;i++)if(shape[i]!=='H')out[i]=shape[i]==='U'?undefined:shape[i]==='N'?null:nodes[shape[i]];return out;};
 const snapshot=a=>({length:a.length,own:Array.from({length:a.length},(_,i)=>Object.hasOwn(a,i)),has:Array.from({length:a.length},(_,i)=>i in a),values:Array.from({length:a.length},(_,i)=>tag(a[i]))});
 const captured={},extra={},trace=[];let error=null;
 const context={factory,nodes,tag,array,snapshot,captured,extra,trace};
 try{run(context);}catch(e){error={name:e.name,message:e.message};}
 const observed=created.map(n=>({id:tag(n),kind:n.kind,parent:tag(n.parent),children:snapshot(n.childNodes)}));
 cases.push({name,error,nodes:observed,captured:Object.fromEntries(Object.entries(captured).map(([k,a])=>[k,snapshot(a)])),extra,trace});
}
const states=['H','U','N','a','b'],shapes=states.flatMap(a=>states.map(b=>[a,b,'c']));
for(const shape of shapes){
 const suffix=shape.join('-');
 for(const kind of ['mrow','msubsup','mfrac'])trial('copy-'+kind+'-'+suffix,c=>{const node=c.factory.create(kind);node.childNodes=c.array(shape);c.captured.original=node.childNodes;c.extra.result=c.tag(node.copy());});
 for(const kind of ['mrow','mfrac'])trial('append-inferred-'+kind+'-'+suffix,c=>{const row=c.factory.create('inferredMrow'),node=c.factory.create(kind);row.childNodes=c.array(shape);c.captured.original=row.childNodes;c.extra.result=c.tag(node.appendChild(row));});
 trial('inferred-copy-'+suffix,c=>{const node=c.factory.create('math'),row=node.childNodes[0];row.childNodes=c.array(shape);c.captured.original=row.childNodes;c.extra.result=c.tag(node.copy());});
 for(const member of ['isEmpty','isSpacelike','isEmbellished'])trial('mrow-'+member+'-'+suffix,c=>{const node=c.factory.create('mrow');node.childNodes=c.array(shape);c.extra.value=node[member];});
 trial('arity-truncate-'+suffix,c=>{const node=c.factory.create('mfrac');node.childNodes=c.array(shape.concat(['a','b']));c.captured.original=node.childNodes;node.setInheritedAttributes({},false,0,false);c.extra.rebound=node.childNodes!==c.captured.original;});
 trial('arity-grow-'+suffix,c=>{const node=c.factory.create('msubsup');node.childNodes=c.array(shape.slice(0,2));c.captured.original=node.childNodes;node.setInheritedAttributes({},false,0,false);c.extra.rebound=node.childNodes!==c.captured.original;});
}
for(const value of ['H','U','N','a'])trial('copy-single-'+value,c=>{const node=c.factory.create('mrow');node.childNodes=c.array([value]);c.extra.result=c.tag(node.copy());});
for(const proto of [false,true])trial('append-inferred-prototype-'+proto,c=>{const row=c.factory.create('inferredMrow'),node=c.factory.create('mrow');row.childNodes=c.array(['a','H','c']);if(proto)Object.setPrototypeOf(row.childNodes,c.array(['H','b']));c.captured.original=row.childNodes;c.extra.result=c.tag(node.appendChild(row));});
for(const operation of ['copy','copyChildren','inherit','fill-and-inherit'])trial('superscript-first-'+operation,c=>{
 const node=c.factory.create('msubsup',{},[c.nodes.a]);NodeUtil.setChild(node,2,c.nodes.c);c.captured.superscript=node.childNodes;c.extra.before=c.snapshot(node.childNodes);
 if(operation==='copy')c.extra.result=c.tag(node.copy());
 if(operation==='copyChildren'){const target=c.factory.create('msubsup');NodeUtil.copyChildren(node,target);c.extra.result=c.tag(target);}
 if(operation==='inherit')node.setInheritedAttributes({},false,0,false);
 if(operation==='fill-and-inherit'){NodeUtil.setChild(node,1,c.nodes.b);node.setInheritedAttributes({},false,0,false);}
});
for(const operation of ['append-inferred','setChildren'])for(const mutation of ['append','delete','shrink','rebind-source','rebind-target'])trial(operation+'-parent-setter-'+mutation,c=>{
 const row=c.factory.create('inferredMrow'),node=c.factory.create('mrow');row.childNodes=c.array(['a','b']);const original=row.childNodes;c.captured.original=original;c.captured.target=node.childNodes;
 let parent=null;Object.defineProperty(c.nodes.a,'parent',{get(){return parent;},set(value){parent=value;c.trace.push('set:'+c.tag(value));if(mutation==='append')original.push(c.nodes.c);if(mutation==='delete')delete original[1];if(mutation==='shrink')original.length=1;if(mutation==='rebind-source')row.childNodes=c.array(['c']);if(mutation==='rebind-target')node.childNodes=c.array(['c']);}});
 if(operation==='append-inferred')c.extra.result=c.tag(node.appendChild(row));else node.setChildren(original);
});
for(const mutation of ['append','delete','shrink','rebind'])trial('copy-live-child-'+mutation,c=>{
 const node=c.factory.create('mrow');node.childNodes=c.array(['a','b']);const original=node.childNodes;c.captured.original=original;const copy=c.nodes.a.copy;
 c.nodes.a.copy=function(...args){c.trace.push('copy:a');if(mutation==='append')original.push(c.nodes.c);if(mutation==='delete')delete original[1];if(mutation==='shrink')original.length=1;if(mutation==='rebind')node.childNodes=c.array(['c']);return copy.apply(this,args);};
 c.extra.result=c.tag(node.copy());
});
assert.ok(cases.length>250);
assert.ok(cases.every(c=>c.nodes.every(n=>n.id!=='UNBOUND'&&n.parent!=='UNBOUND'&&n.children.values.every(v=>v!=='UNBOUND'))),'Register every observed Source identity');
assert.ok(!JSON.stringify(cases).includes('UNBOUND'),'Captured arrays and extra observations also require registered identities');
const loadedPins=Object.fromEntries(Object.keys(require.cache).filter(f=>f.startsWith(base+path.sep)).sort().map(f=>[path.relative(base,f).replaceAll('\\','/'),hash(fs.readFileSync(f))]));
const data=JSON.stringify({schema:1,status:'Root actual MML Source evidence only; native full child graph migration has not passed',node:process.versions.node,pins,loadedPins,cases},null,2)+'\n';
const out=path.join(root,'tools/math-port/profiles/evidence/p23b-sparse-mml-source.json');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(out));fs.writeFileSync(out,data);}
console.log(JSON.stringify({cases:cases.length,actual_source_errors:cases.filter(c=>c.error).length,loaded_source_files:Object.keys(loadedPins).length,sha256:hash(data)}));
