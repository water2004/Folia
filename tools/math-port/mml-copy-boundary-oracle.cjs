// Root executes actual Source for nullable arrays/products and retained factory stores.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs'),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const pins=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/evidence/p23b-sparse-mml-source.json'))).loadedPins;assert.equal(process.versions.node,'24.13.0');for(const[f,p]of Object.entries(pins))assert.equal(hash(fs.readFileSync(path.join(base,f))),p);
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js'));
const scalar=v=>v===undefined?'U':v===null?'N':typeof v==='boolean'?(v?'T':'F'):typeof v==='number'?'D'+v:'S'+v;
const cases=[];for(const mode of ['source-null','inferred-inner-null','factory-null','destination-null-falsey','destination-rebind-after-copy','retained-properties']){
 const nodes=[],ids=new WeakMap();let active=false,target=null,held;
 class Observed extends MmlFactory{create(kind,...args){if(active&&kind==='mrow'&&mode==='factory-null')return null;const n=super.create(kind,...args);ids.set(n,'n'+nodes.length);nodes.push(n);if(active&&kind==='mrow'){target=n;if(mode==='destination-null-falsey')n.childNodes=null;if(mode==='retained-properties'){n.setProperty('sentinel',123);held=n.properties;}}return n;}}
 const f=new Observed(),a=f.create('mi'),b=f.create('mi'),n=f.create('mrow');n.setProperty('top',5);
 if(mode==='source-null')n.childNodes=null;else if(mode==='inferred-inner-null'){const r=f.create('inferredMrow');r.childNodes=null;n.childNodes=[r];}else if(mode==='destination-null-falsey')n.childNodes=[null,a];else n.childNodes=[a,null];
 if(mode==='destination-rebind-after-copy'){const copy=a.copy;a.copy=function(...args){const c=copy.apply(this,args);target.childNodes=[b];return c;};}
 const tag=v=>v===undefined?'U':v===null?'N':ids.get(v)??'UNBOUND';const array=a=>a===null?'N':a.length+':'+Array.from({length:a.length},(_,i)=>Object.hasOwn(a,i)?'1':'0').join('')+':'+Array.from({length:a.length},(_,i)=>i in a?'1':'0').join('')+':'+Array.from({length:a.length},(_,i)=>tag(a[i])).join(',');
 active=true;let error='ok',result='';try{result=tag(n.copy());}catch(e){error=e.name+'|'+e.message;}
 const expected=error+'|'+nodes.map(n=>tag(n)+':'+n.kind+':'+tag(n.parent)+':'+array(n.childNodes)).join(';')+'|'+result+'|'+scalar(held?.sentinel)+'|'+scalar(held?.top)+'|'+scalar(!!held&&held===target?.properties);
 cases.push({mode,expected});
}
const data=JSON.stringify({schema:1,pins,cases},null,2)+'\n',out=path.join(root,'tools/math-port/profiles/evidence/p23g-copy-boundary-source.json');if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(out));fs.writeFileSync(out,data);}console.log(JSON.stringify({cases:cases.length,sha256:hash(data)}));
