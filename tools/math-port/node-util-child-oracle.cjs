// Root actual original NodeUtil object; hooks are transport, never algorithm substitutes.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs'),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const prerequisite=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/evidence/p23b-sparse-mml-source.json')));for(const[f,p]of Object.entries(prerequisite.loadedPins))assert.equal(hash(fs.readFileSync(path.join(base,f))),p);
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js')),NodeUtil=require(path.join(base,'input/tex/NodeUtil.js')).default;
const inputs=[];const shapes=['','H','U','N','a','HNb','aUb'];
for(const shape of shapes)for(const position of [0,1,3]){inputs.push({op:'get',shape,position});for(const child of ['U','N','a','b'])inputs.push({op:'set',shape,position,child});}
for(const shape of shapes)for(const target of ['','HN','aUb'])inputs.push({op:'copy',shape,target});
for(const position of [0,3]){inputs.push({op:'get',shape:'',position,sourceNull:true});inputs.push({op:'set',shape:'',position,child:'a',targetNull:true});}
for(const shape of ['','H','a'])inputs.push({op:'copy',shape,targetNull:true});inputs.push({op:'copy',shape:'',sourceNull:true});
for(const receiver of ['U','N'])for(const op of ['get','set','copy'])inputs.push({op,shape:'a',position:0,child:'a',receiver});
for(const shape of ['H','U','N'])for(const op of ['get','copy'])inputs.push({op,shape,position:0,prototype:'b'});
for(const mode of ['source-rebind','source-grow','source-truncate','source-delete','source-prototype','target-rebind','target-null','throw'])inputs.push({op:'copy',shape:'aHc',target:'N',mode});
for(const parentMode of ['source-rebind','source-grow','source-truncate','target-rebind','target-null','throw'])inputs.push({op:'copy',shape:'aHc',target:'N',parentMode});
inputs.push({op:'copy',shape:'aHc',newReceiver:'U'},{op:'copy',shape:'aHc',newReceiver:'N'},{op:'copy',shape:'',newReceiver:'N'});
function run(i){const f=new MmlFactory(),nodes={};for(const label of ['a','b','c','old','target'])nodes[label]=f.create('mi');const trace=[];const tag=v=>v===undefined?'U':v===null?'N':Object.keys(nodes).find(k=>nodes[k]===v)??'UNBOUND';
 const value=c=>c==='U'?undefined:c==='N'?null:nodes[c];const shape=s=>{const a=new Array(s.length);for(let n=0;n<s.length;n++)if(s[n]!=='H')a[n]=value(s[n]);return a;};
 nodes.old.childNodes=i.sourceNull?null:shape(i.shape);nodes.target.childNodes=i.targetNull?null:shape(i.target??'');const old=nodes.old.childNodes,target=nodes.target.childNodes;if(i.prototype)Object.setPrototypeOf(old,shape(i.prototype));
 function action(mode){if(mode==='source-rebind')nodes.old.childNodes=shape('b');if(mode==='source-grow')old.push(nodes.b);if(mode==='source-truncate')old.length=1;if(mode==='source-delete')delete old[2];if(mode==='source-prototype')Object.setPrototypeOf(old,shape('bbb'));if(mode==='target-rebind')nodes.target.childNodes=shape('b');if(mode==='target-null')nodes.target.childNodes=null;if(mode==='throw')throw new Error('boom');}
 if(i.parentMode){let parent=nodes.a.parent;Object.defineProperty(nodes.a,'parent',{get(){return parent;},set(v){parent=v;trace.push('parent:a:'+tag(v));action(i.parentMode);}});}
 const receiver={...NodeUtil,setChild(node,position,child){trace.push('set:'+position+':'+tag(child));const v=NodeUtil.setChild(node,position,child);if(position===0)action(i.mode);return v;}};
 let result='THREW',error='ok';try{const source=i.receiver?value(i.receiver):nodes.old,newNode=i.newReceiver?value(i.newReceiver):nodes.target;if(i.op==='get')result=tag(receiver.getChildAt(source,i.position));else if(i.op==='set')result=tag(receiver.setChild(i.receiver?source:newNode,i.position,value(i.child)));else result=tag(receiver.copyChildren(source,newNode));}catch(e){error=e.name+'|'+e.message;}
 const array=a=>a===null?'N':a.length+':'+Array.from({length:a.length},(_,n)=>Number(Object.hasOwn(a,n))).join('')+':'+Array.from({length:a.length},(_,n)=>Number(n in a)).join('')+':'+Array.from({length:a.length},(_,n)=>tag(a[n])).join(',');
 const out=error+'|'+result+'|'+Object.entries(nodes).map(([k,n])=>k+':'+tag(n.parent)+':'+array(n.childNodes)).join(';')+'|'+array(old)+'|'+array(target)+'|'+Number(old===nodes.old.childNodes)+Number(target===nodes.target.childNodes)+'|'+trace.join(',');assert.ok(!out.includes('UNBOUND'));return out;
}
const cases=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify({schema:1,scope:'actual original NodeUtil getChildAt/setChild/copyChildren; typed indexed children, captured/live arrays, receiver and parent hooks',loadedPins:prerequisite.loadedPins,cases},null,2)+'\n',out=path.join(root,'tools/math-port/profiles/evidence/p23i-node-util-child-source.json');if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(out));fs.writeFileSync(out,data);}console.log(JSON.stringify({cases:cases.length,sha256:hash(data)}));
