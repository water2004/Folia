// Source callbacks witness aliasing, evaluation order and partial writes.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,MmlFactory}=require('./simple-mml-reference.cjs'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const u=require(path.join(base,'input/tex/NodeUtil.js')).default,Mo=require(path.join(base,'core/MmlTree/MmlNodes/mo.js')).MmlMo;
const val=p=>p===undefined?'U':p===null?'N':String(p),rows=[];
for(const mode of ['normal','getter-throw','setter-throw','props-throw','second-throw','setter-add','first-mutate','null-new','undefined-new']){
 const f=new MmlFactory(),old=f.create('mi'),next=f.create('mn'),trace=[],attrs=old.attributes;let installed=next.attributes;
 old.setProperty('a',1);old.setProperty('b',2);
 Object.defineProperty(old,'attributes',{get(){trace.push('old.attrs');if(mode==='getter-throw')throw Error('getter');return attrs;}});
 Object.defineProperty(next,'attributes',{get(){return installed;},set(value){trace.push('next.attrs');if(mode==='setter-throw')throw Error('setter');installed=value;if(mode==='setter-add')old.setProperty('c',3);}});
 const all=old.getAllProperties;old.getAllProperties=function(){trace.push('old.props');if(mode==='props-throw')throw Error('props');return all.call(this);};
 const set=next.setProperty;next.setProperty=function(k,v){trace.push('set:'+k+':'+val(v));if(mode==='second-throw'&&k==='b')throw Error('second');if(mode==='first-mutate'&&k==='a')old.setProperty('b',9);return set.call(this,k,v);};
 let error='ok';try{u.copyAttributes(old,mode==='null-new'?null:mode==='undefined-new'?undefined:next);}catch(e){error=e.name+'|'+e.message;}
 rows.push({mode,expected:error+'|'+trace.join(',')+'|'+(installed===attrs?'same':'other')+'|'+['a','b','c'].map(k=>val(next.getProperty(k))).join(',')});
}
for(const mode of ['global','capture','false-op','null-def','false-attr','null-attr','missing-attr','explicit','inherited']){
 const f=new MmlFactory(),mo=f.create('mo',{},[f.create('text').setText('x')]),trace=[],saved=Mo.OPTABLE;
 const def=v=>[0,0,0,{test:v}];let t={infix:{x:def('first')},postfix:{x:def('second')},prefix:{x:def('third')}};Mo.OPTABLE=t;
 if(mode==='false-op')t.infix.x=false;if(mode==='null-def')t.infix.x=[0,0,0,null];if(mode==='false-attr')t.infix.x=def(false);if(mode==='null-attr')t.infix.x=def(null);if(mode==='missing-attr')t.infix.x=def(undefined);
 if(mode==='explicit')mo.attributes.set('test',false);if(mode==='inherited')mo.attributes.setInherited('test',null);
 const text=mo.getText;mo.getText=function(){trace.push('text');if(mode==='capture')Mo.OPTABLE={infix:{x:def('replacement')}};return text.call(this);};
 let result,error='ok';try{result=mode==='global'||mode==='capture'?u.getOp(mo)[3].test:u.getMoAttribute(mo,'test');}catch(e){error=e.name+'|'+e.message;}finally{Mo.OPTABLE=saved;}
 rows.push({mode,expected:error+'|'+trace.join(',')+'|'+val(result)});
}
const data=JSON.stringify(rows)+'\n',profile=path.join(root,'tools/math-port/profiles/evidence/p40-node-util-order-source.json');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(profile));fs.writeFileSync(profile,data);}
console.log(JSON.stringify({source_calls:rows.length}));
