// Root expected values execute the original registry and constructor hierarchy.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs'),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const prerequisite=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/evidence/p23b-sparse-mml-source.json')));assert.equal(process.versions.node,'24.13.0');for(const[f,p]of Object.entries(prerequisite.loadedPins))assert.equal(hash(fs.readFileSync(path.join(base,f))),p);
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js')),{MmlMi}=require(path.join(base,'core/MmlTree/MmlNodes/mi.js'));
const inputs=[],cases=[];for(const kind of new MmlFactory().getKinds())inputs.push({kind,properties:'empty',children:''});
for(const kind of ['mi','mrow','mfrac','math','mstyle','msubsup','mtable','mtr'])for(const properties of ['list','null'])inputs.push({kind,properties,children:''});
for(const kind of ['mrow','math','mstyle','mfrac'])for(const children of ['ab','aH','aN','aU','null'])inputs.push({kind,properties:'list',children});
for(const children of ['','ab','null'])for(const properties of ['empty','list','null'])inputs.push({kind:'mi',properties,children,trace:true});
function run(input){const ids=new WeakMap(),nodes=[],trace=[],register=n=>{if(!ids.has(n)){ids.set(n,'n'+nodes.length);nodes.push(n);}return n;};
 class Observed extends MmlFactory{create(...args){return register(super.create(...args));}}
 const factory=new Observed();if(input.trace){class Trace extends MmlMi{setChildren(c){trace.push('children:attrs='+!!this.attributes);return super.setChildren(c);}}factory.setNodeClass('mi',Trace);}
 const tag=v=>v===undefined?'U':v===null?'N':(register(v),ids.get(v));
 const scalar=v=>v===undefined?'U':v===null?'N':typeof v==='boolean'?(v?'T':'F'):typeof v==='number'?'D'+v:'S'+v;
 let error='ok',result='';try{
  let children;if(input.children==='null')children=null;else{children=new Array(input.children.length);for(let i=0;i<input.children.length;i++){const c=input.children[i];if(c==='H')continue;children[i]=c==='U'?undefined:c==='N'?null:factory.create('mi');}}
  const attributes=input.properties==='null'?null:input.properties==='list'?{'10':10,'2':2,z:'exact',id:'kept',accent:true}:{};
  result=tag(factory.create(input.kind,attributes,children));
 }catch(e){error=e.name+'|'+e.message;}
 // Discovery also names a failed outer constructor retained by an inferred row.
 // It observes existing object identity; it does not make another Source node.
 let out=error+'|'+result+'|';for(let i=0;i<nodes.length;i++){const n=nodes[i],a=n.childNodes;let own='',has='',values='';for(let j=0;j<a.length;j++){own+=Object.hasOwn(a,j)?'1':'0';has+=j in a?'1':'0';if(j)values+=',';values+=tag(a[j]);}
  const parent=tag(n.parent),attrs=n.attributes;out+=ids.get(n)+':'+n.kind+':'+parent+':'+a.length+':'+own+':'+has+':'+values+':'+!!attrs+':';if(attrs){const list=attrs.getAllAttributes();out+=Object.keys(list).map(k=>k+'='+scalar(list[k])).join(',');}out+=';';}
 return {input,expected:out+'|'+trace.join(',')};
}
for(const input of inputs)cases.push(run(input));
const data=JSON.stringify({schema:1,status:'Root original constructor/registry evidence only; other MML semantics and full graph acceptance pending',node:process.versions.node,pins:prerequisite.loadedPins,cases},null,2)+'\n',out=path.join(root,'tools/math-port/profiles/evidence/p23f-constructor-source.json');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(out));fs.writeFileSync(out,data);}console.log(JSON.stringify({cases:cases.length,errors:cases.filter(c=>!c.expected.startsWith('ok|')).length,sha256:hash(data)}));
