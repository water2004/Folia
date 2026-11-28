// Root actual ECMAScript Array observations for the single sparse child store.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),cases=[],objects={a:{},b:{},c:{}},tag=x=>x===undefined?'U':x===null?'N':Object.keys(objects).find(k=>objects[k]===x)??'BAD';
const make=shape=>{const a=new Array(shape.length);for(let i=0;i<shape.length;i++)if(shape[i]!=='H')a[i]=shape[i]==='U'?undefined:shape[i]==='N'?null:objects[shape[i]];return a;};
const value=s=>s==='U'?undefined:s==='N'?null:objects[s];
const snapshot=a=>({length:a.length,own:Array.from({length:a.length},(_,i)=>Object.hasOwn(a,i)),has:Array.from({length:a.length},(_,i)=>i in a),values:Array.from({length:a.length},(_,i)=>tag(a[i])),outside:[tag(a[5]),5 in a]});
const shapes=[[],['H'],['U'],['N'],['a'],['H','U','N','a','b'],['a','H','b','U','N','c']];
for(const x of ['H','U','N','a','b'])for(const y of ['H','U','N','a','b'])shapes.push([x,y,'c']);
function add(shape,prototype,operation){const a=make(shape),proto=prototype?make(['H','b','H','H','H','c']):null;if(proto)Object.setPrototypeOf(a,proto);let result=null;
 switch(operation.kind){
 case'set':a[operation.index]=value(operation.value);break;
 case'delete':delete a[operation.index];break;
 case'resize':a.length=operation.length;break;
 case'push':result=a.push(value(operation.value));break;
 case'pop':result=tag(a.pop());break;
 case'slice':result=snapshot(operation.end===null?a.slice(operation.start):a.slice(operation.start,operation.end));break;
 case'splice':result=snapshot(operation.count===null?a.splice(operation.start):a.splice(operation.start,operation.count,...operation.added.map(value)));break;
 case'indexOf':result=a.indexOf(value(operation.value),operation.start);break;
 case'forEach':{const trace=[];a.forEach((x,i,self)=>{trace.push([tag(x),i,self===a]);if(i===0){if(operation.mutation==='append')a.push(objects.c);if(operation.mutation==='delete')delete a[1];if(operation.mutation==='shrink')a.length=1;if(operation.mutation==='fill')a[1]=objects.b;}});result=trace;break;}
 default:throw Error('unbound operation');
 }
 cases.push({shape,prototype,operation,result,after:snapshot(a)});
}
for(const shape of shapes)for(const proto of [false,true]){
 for(const index of [0,1,3,7]){for(const v of ['U','N','a'])add(shape,proto,{kind:'set',index,value:v});add(shape,proto,{kind:'delete',index});}
 for(const length of [0,1,3,8])add(shape,proto,{kind:'resize',length});
 for(const v of ['U','N','b'])add(shape,proto,{kind:'push',value:v});add(shape,proto,{kind:'pop'});
 for(const start of [-9,-2,0,1,8])for(const end of [null,-1,0,2,9])add(shape,proto,{kind:'slice',start,end});
 for(const start of [-9,-2,0,1,8])for(const count of [null,0,1,9])for(const added of count===null?[[]]:[[],['U'],['N','a']])add(shape,proto,{kind:'splice',start,count,added});
 for(const v of ['U','N','a','b','c'])for(const start of [-9,-1,0,2,9])add(shape,proto,{kind:'indexOf',value:v,start});
 for(const mutation of ['none','append','delete','shrink','fill'])add(shape,proto,{kind:'forEach',mutation});
}
const data=JSON.stringify({schema:1,scope:'ordinary numeric-index child arrays and bound numeric prototype data; no generic JS exotic/species/descriptors',node:process.versions.node,cases},null,2)+'\n',out=path.join(root,'tools/math-port/profiles/evidence/p23-child-array-source.json');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(out));fs.writeFileSync(out,data);}
console.log(JSON.stringify({cases:cases.length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
