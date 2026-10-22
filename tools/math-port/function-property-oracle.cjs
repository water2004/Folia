// Root actual Node/CJS data and invocation observations, no native output.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
assert.equal(process.version,'v24.13.0');const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs'),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
assert.equal(hash(fs.readFileSync(path.join(base,'input/tex/TokenMap.js'))),'9e3f086080a6ab052b68b22a5d9ee3dbc5e17d7b0c03a31e789664b01df36cf7');assert.equal(hash(fs.readFileSync(path.join(base,'util/FunctionList.js'))),'571f57801b863b2d9f01a66dc72f2836b7fd06a1a48a3fe0057e4ec3d51a7278');
const{MacroMap}=require(path.join(base,'input/tex/TokenMap.js')),{FunctionList}=require(path.join(base,'util/FunctionList.js')),cases=[];
// Every tuple slot is a function, present undefined, inherited function/undefined,
// or a hole. Both callback identities share state only with their own aliases.
for(let length=0;length<=7;length++)for(let mask=0;mask<16;mask++)for(const first of [0,1,2,7,100]){
 const f=function(){},g=function(){},a=new Array(length),proto=Object.create(Array.prototype);Object.setPrototypeOf(a,proto);
 for(let i=0;i<length;i++){const mode=(mask+i)%5;if(mode===0)a[i]=f;else if(mode===1)a[i]=g;else if(mode===2)a[i]=undefined;else if(mode===3)proto[i]=i%2?f:undefined;}
 const b=a.slice(first),has=Array.from({length:b.length},(_,i)=>Object.hasOwn(b,i)),values=Array.from({length:b.length},(_,i)=>b[i]===f?1:b[i]===g?2:0);cases.push({length,mask,first,size:b.length,has,values,fresh:b!==a});
}
let count=0;function f(){return ++count}function g(){return ++count+10}const graph={'9':f,x:f,'2':g},snapshot=Object.entries(graph);graph.x=g;const tuple=[f,undefined],map=new MacroMap('p22a-data',{x:tuple}),list=new FunctionList();list.add(map.lookup('x').func);const first=list.execute(),flags=[snapshot.map(([k])=>k).join(',')==='2,9,x',snapshot[2][1]===f,graph.x===g,map.lookup('x').func===f,map.lookup('x').args[0]===undefined,tuple[0]===f,Boolean(f),!Array.isArray(f),first===true,count===1];
list.remove(graph['9']);flags.push(Array.from(list).length===0);list.add(graph.x);list.execute();flags.push(count===2);const counter=map.parse([{},'x']);flags.push(counter===3);let saved;
function callback(){saved=this;return false}const l=new FunctionList();l.add(callback,13);const record=Array.from(l)[0];flags.push(l.execute()===false,saved===record,record.item===callback);
const values=[f,g,undefined,null,false,true,0,-0,NaN,'','x',{},[]],equal=values.flatMap(a=>values.map(b=>a===b));
const report={node:process.version,stage:'actual Source function-data and ordinary slice host only',cases,flags,equal},text=JSON.stringify(report,null,2)+'\n',out=path.join(root,'docs/math-port/evidence/p22a-function-property-source.json');
assert.equal(fs.readFileSync(out,'utf8'),text);

console.log(JSON.stringify({sourceCases:cases.length,flags:flags.length,equalities:equal.length,mode:'check'}));
