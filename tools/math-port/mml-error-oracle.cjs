// Root-owned calls to original mError and derived verification, not a native service.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root,MmlFactory,core}=require(path.join(__dirname,'./inheritance-reference.cjs'));
const options=['U','N','false','true','0','7','NaN','empty','raw','objectFalse','objectTrue','objectString','objectArray','arrayFalse','arrayTrue','arrayObject'];
const parents=['N','U','normal','merror','missing'],modes=['','parent-null-on-read','parent-kind-throw','create-error-throw','create-error-null','error-attrs-null','create-text-null','create-text-wrong','create-text-full-false','create-text-full-true','replace-throw','replace-full-false','replace-full-true','replace-attrs-null'];
const inputs=[];
for(const option of options)for(const parent of parents)for(const short of [false,true])for(const mode of modes)inputs.push({op:'error',option,parent,short,mode,message:'message'});
for(const op of ['math','mprescripts','none'])for(const option of options)for(const parent of parents)for(const mode of ['','create-error-null','replace-attrs-null','replace-full-false'])inputs.push({op,option,parent,short:false,mode,message:'message'});
for(const short of [false,true])for(const option of ['objectFalse','objectTrue'])inputs.push({op:'error',option,parent:'normal',short,mode:'',message:'a😀b'});
const scalar=v=>v===undefined?'U':v===null?'N':typeof v==='boolean'?String(v):typeof v==='number'?(Object.is(v,-0)?'-0':String(v)):typeof v==='string'?'s:'+v:Array.isArray(v)?'a':'o';
function run(i){let active=false,trace=[],created=[],serial=0,parentReads=0,opts,n,p;const ids=new Map(),note=s=>{if(active)trace.push(s);},fail=()=>{throw Object.assign(new Error('witness'),{name:'WitnessError'});};
 const tag=v=>v===undefined?'U':v===null?'N':ids.get(v)??'UNBOUND';
 const setFull=v=>{if(opts&&typeof opts==='object')opts.fullErrors=v;};
 class Probe extends core.AbstractMmlNode{get kind(){note('kind/n');return 'error-probe';}get parent(){note('parent/n');const result=this.rawParent;if(active&&i.mode==='parent-null-on-read'&&parentReads++===0)this.rawParent=null;return result;}set parent(v){this.rawParent=v;}}
 class Parent extends core.AbstractMmlNode{get kind(){return 'error-parent';}isKind(k){note('isKind/p/'+k);if(i.mode==='parent-kind-throw')fail();return i.parent==='merror'&&k==='merror';}replaceChild(a,b){note('replace/'+tag(a)+'/'+tag(b));if(i.mode==='replace-throw')fail();const result=super.replaceChild(a,b);if(i.mode==='replace-full-false')setFull(false);if(i.mode==='replace-full-true')setFull(true);if(i.mode==='replace-attrs-null')a.attributes=null;return result;}}
 class Factory extends MmlFactory{create(k,...args){if(active){note('create/'+k);if(k==='merror'&&i.mode==='create-error-throw')fail();if(k==='merror'&&i.mode==='create-error-null')return null;if(k==='text'&&i.mode==='create-text-null')return null;if(k==='text'&&i.mode==='create-text-wrong')k='mrow';}
  const result=super.create(k,...args);if(active){ids.set(result,'c'+serial++);created.push(result);if(k==='merror'&&i.mode==='error-attrs-null')result.attributes=null;if(k==='text'&&i.mode==='create-text-full-false')setFull(false);if(k==='text'&&i.mode==='create-text-full-true')setFull(true);}return result;}}
 const f=new Factory();f.setNodeClass('error-probe',Probe);f.setNodeClass('error-parent',Parent);n=f.create(i.op==='error'?'error-probe':i.op);p=f.create('error-parent');ids.set(n,'n');ids.set(p,'p');
 if(i.parent==='N')n.parent=null;else if(i.parent==='U')n.parent=undefined;else{n.parent=p;if(i.parent!=='missing')p.childNodes=[n];}
 opts=i.option==='U'?undefined:i.option==='N'?null:i.option==='false'?false:i.option==='true'?true:i.option==='0'?0:i.option==='7'?7:i.option==='NaN'?NaN:i.option==='empty'?'':i.option==='raw'?'raw':i.option.startsWith('array')?[]:{};
 if(opts&&typeof opts==='object')opts.fullErrors=i.option.endsWith('False')?false:i.option.endsWith('True')?true:i.option.endsWith('String')?'raw':i.option.endsWith('Array')?[]:{};
 let error='ok',result='void';active=true;try{if(i.op==='error')result=tag(n.mError(i.message,opts,i.short));else n.verifyTree(opts);}catch(e){error=e.name+'|'+e.message;}active=false;
 const snapshot=node=>{const a=node.attributes,attrs=a?Object.keys(a.getAllAttributes()).map(k=>k+'='+scalar(a.getAllAttributes()[k])).join(','):'N';const children=node.childNodes===null?'N':node.childNodes.length+':'+Array.from({length:node.childNodes.length},(_,j)=>tag(node.childNodes[j])).join(',');return tag(node)+'/'+node.kind+'/'+tag(node.parent)+'/'+attrs+'/'+(node instanceof core.TextNode?scalar(node.getText()):'U')+'/'+children;};
 return error+'|'+result+'|'+scalar(opts)+((opts&&typeof opts==='object')?'/'+scalar(opts.fullErrors):'')+'|'+[n,p,...created].map(snapshot).join(';')+'|'+trace.join(',');
}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';

const frozen=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence',"p33-error-source.json"),'utf8'));
assert.deepEqual(rows,frozen);
assert.equal(data,fs.readFileSync(path.join(root,'docs/math-port/evidence',"p33-error-source.json"),'utf8').replaceAll('\r\n','\n'));
console.log(JSON.stringify({sourceCases:rows.length,sourceErrors:rows.filter(r=>!r.expected.startsWith('ok|')).length}));
