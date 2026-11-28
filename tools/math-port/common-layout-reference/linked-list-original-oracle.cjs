'use strict';
// Execute the pinned upstream LinkedList/AbstractMathList, recording graph
// identities and behavior directly from the original CJS implementation.
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../../..');
const sourceRoot=path.join(root,'.reference/MathJax-src');
const packageRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
const sources=['ts/util/LinkedList.ts','ts/core/MathList.ts'];
for(const file of sources){
  const entry=scope.files.find(x=>x.source===file);assert.ok(entry,`missing scope lock: ${file}`);
  assert.equal(digest(path.join(sourceRoot,file)),entry.source_sha256,file);
}
const cjsFiles=['cjs/util/LinkedList.js','cjs/core/MathList.js'];
const cjsLocks=Object.fromEntries(cjsFiles.map(file=>[file,digest(path.join(packageRoot,file))]));
const {LinkedList,END}=require(path.join(packageRoot,'cjs/util/LinkedList.js'));
const {AbstractMathList}=require(path.join(packageRoot,'cjs/core/MathList.js'));
const out={source: sources.map(file=>({file,sha256:scope.files.find(x=>x.source===file).source_sha256})),cjs:cjsLocks,cases:[]};
const add=(name,observe)=>out.cases.push({name,...observe()});
const values=list=>Array.from(list);
add('end-symbol-and-circular-sentinel',()=>{
  const list=new LinkedList();const sentinel=list.list;
  return {endIsSymbol:typeof END==='symbol',sentinelDataIsEnd:sentinel.data===END,
    emptyLinksSelf:sentinel.next===sentinel&&sentinel.prev===sentinel,
    listHasEndSentinel:sentinel.data===END};
});
add('push-unshift-and-chain-identity',()=>{
  const list=new LinkedList();const push=list.push(2,3),unshift=list.unshift(-1,0);
  return {values:values(list),pushReturnsSelf:push===list,unshiftReturnsSelf:unshift===list,
    headPrevIsSentinel:list.list.next.prev===list.list,tailNextIsSentinel:list.list.prev.next===list.list};
});
add('pop-shift-detach-and-empty-null',()=>{
  const list=new LinkedList('a','b','c');const tail=list.list.prev,head=list.list.next;
  const popped=list.pop(),shifted=list.shift(),last=list.pop(),emptyPop=list.pop(),emptyShift=list.shift();
  return {popped,shifted,last,emptyPop,emptyShift,tailDetached:tail.next===null&&tail.prev===null,
    headDetached:head.next===null&&head.prev===null,empty:list.list.next===list.list&&list.list.prev===list.list};
});
add('remove-map-membership-and-detach',()=>{
  const a={},b={},c={},list=new LinkedList(a,b,a,c);const first=list.list.next,removed=first.next.next;
  const result=list.remove(a,a);
  return {values:values(list).map(x=>x===b?'b':x===c?'c':'?'),returnsSelf:result===list,
    removedDetached:removed.next===null&&removed.prev===null};
});
add('clear-detaches-sentinel-endpoints-only',()=>{
  const list=new LinkedList(1,2,3);const first=list.list.next,last=list.list.prev,interior=first.next;
  const result=list.clear();
  return {returnsSelf:result===list,empty:list.list.next===list.list&&list.list.prev===list.list,
    firstDetached:first.prev===null,lastDetached:last.next===null,
    interiorStillLinked:interior.next!==null&&interior.prev!==null};
});
add('forward-and-reverse-iterators-see-live-tail',()=>{
  const list=new LinkedList(1,2);const forward=list[Symbol.iterator]();const first=forward.next().value;list.push(3);
  const rest=[...forward],reverse=[...list.reversed()];
  return {first,rest,reverse};
});
add('default-insert-dispatches-virtual-isBefore',()=>{
  class Ordered extends LinkedList{constructor(){super();this.calls=[];}isBefore(a,b){this.calls.push([a,b,this]);return a.rank<b.rank;}}
  const list=new Ordered(),a={rank:2,id:'a'},b={rank:1,id:'b'},c={rank:2,id:'c'};
  const result=list.insert(a).insert(b).insert(c);
  return {ids:values(list).map(x=>x.id),returnsSelf:result===list,
    calls:list.calls.map(([x,y,receiver])=>[x.id,y.id,receiver===list])};
});
add('sort-is-stable-and-replaces-sentinel',()=>{
  const list=new LinkedList(),items=[{k:2,id:'a'},{k:1,id:'b'},{k:2,id:'c'},{k:1,id:'d'}];
  list.push(...items);const oldSentinel=list.list;const result=list.sort((a,b)=>a.k<b.k);
  return {ids:values(list).map(x=>x.id),returnsSelf:result===list,sentinelChanged:list.list!==oldSentinel,
    oldSentinelStillEnd:oldSentinel.data===END};
});
add('merge-mutates-both-lists-as-source',()=>{
  const a=new LinkedList(1,3,5),b=new LinkedList(2,4,6);const otherSentinel=b.list;
  const result=a.merge(b,(x,y)=>x<y);
  return {values:values(a),returnsReceiver:result===a,otherEmptied:b.list.next===b.list&&b.list.prev===b.list,
    otherSentinelRetained:b.list===otherSentinel};
});
add('iterator-retains-current-node-after-remove',()=>{
  const list=new LinkedList('a','b','c'),iterator=list[Symbol.iterator]();
  const first=iterator.next().value;const firstNode=list.list.next;list.remove(first);
  let next,thrown=null;
  try{next=iterator.next();}catch(error){thrown={name:error.name,message:error.message};}
  return {first,next:next&&{done:next.done,value:next.value},thrown,
    removedLinksAreNull:firstNode.next===null&&firstNode.prev===null};
});
add('source-iterator-survives-merge-relinks',()=>{
  const target=new LinkedList(1,4),source=new LinkedList(2,3,5);
  const iterator=source[Symbol.iterator]();const first=iterator.next().value;
  target.merge(source,(a,b)=>a<b);
  let rest,thrown=null;
  try{rest=Array.from(iterator);}catch(error){thrown={name:error.name,message:error.message};}
  return {first,rest,thrown,target:values(target),source:values(source)};
});
add('sort-comparator-mutates-original-during-singleton-merge',()=>{
  const list=new LinkedList(3,1,2),calls=[];let changed=false;
  const result=list.sort((a,b)=>{calls.push([a,b]);if(!changed){changed=true;list.push(9);}return a<b;});
  return {returnsSelf:result===list,values:values(list),calls,changed};
});
add('abstract-math-list-start-i-then-n',()=>{
  const list=new AbstractMathList();
  const items=[{start:{i:2,n:0}},{start:{i:1,n:4}},{start:{i:1,n:2}},{start:{i:1,n:2}}];
  list.push(...items);const result=list.sort();
  return {order:Array.from(list,x=>items.indexOf(x)),returnsSelf:result===list};
});
const output=JSON.stringify(out,null,2)+'\n';
const target=path.join(root,'build/math-port/linked-list-original.json');
fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,output);
process.stdout.write(JSON.stringify({target,cases:out.cases.length,cjs:cjsLocks},null,2)+'\n');
