// Root actual original MML replacement; tagged values, callbacks and partial effects.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory,core}=require('./inheritance-reference.cjs');
const arities=['-Inf','-1','-0','0','2','Inf','NaN'],shapes=['','first','U','N','aab','prototype','null'],values=['U','N','a','b'],inputs=[];
const base={arity:'-1',shape:'first',new:'a',old:'b',mode:''};
for(const arity of arities)for(const shape of shapes)for(const n of values)for(const o of values)inputs.push({...base,arity,shape,new:n,old:o});
for(const arity of ['-1','0','NaN'])for(const mode of ['arity-throw','arity-rebind','arity-null','child-throw','child-return-old','child-rebind','index-throw','index-rebind','index-null','index-none','index-two','new-parent-throw','new-parent-rebind','old-parent-throw','old-parent-other'])for(const n of values)for(const o of values)inputs.push({...base,arity,shape:arity==='-1'?'first':'ab',new:n,old:o,mode});
const number=s=>s==='-Inf'?-Infinity:s==='Inf'?Infinity:s==='NaN'?NaN:Number(s);
function run(i){let active=false,trace=[],n,a,b,first,original,alternate;const ids=new Map(),note=s=>{if(active)trace.push(s);},fail=()=>{throw Object.assign(new Error('witness'),{name:'WitnessError'});};
 const tag=v=>v===undefined?'U':v===null?'N':ids.get(v)??'UNBOUND';
 class Probe extends core.AbstractMmlNode{get kind(){return 'replace-probe';}get arity(){note('arity');if(active&&this===n){if(i.mode==='arity-throw')fail();if(i.mode==='arity-rebind')this.childNodes=alternate;if(i.mode==='arity-null')this.childNodes=null;return number(i.arity);}return Infinity;}get parent(){note('parent.get:'+this.id);if(active&&this===b&&i.mode==='old-parent-throw')fail();return this.saved;}set parent(v){note('parent.set:'+this.id+'='+tag(v));if(active&&this===a&&i.mode==='new-parent-throw')fail();this.saved=v;if(active&&this===a&&i.mode==='new-parent-rebind')n.childNodes=alternate;}childIndex(v){note('index:'+tag(v));if(active){if(i.mode==='index-throw')fail();if(i.mode==='index-rebind')this.childNodes=alternate;if(i.mode==='index-null')this.childNodes=null;if(i.mode==='index-none')return null;if(i.mode==='index-two')return 2;}return super.childIndex(v);}replaceChild(x,y){if(this===first){note('child:'+tag(x)+','+tag(y));if(i.mode==='child-rebind')n.childNodes=alternate;if(i.mode==='child-throw')fail();return i.mode==='child-return-old'?y:b;}return super.replaceChild(x,y);}}
 const f=new MmlFactory();f.setNodeClass('replace-probe',Probe);[n,a,b,first]=Array.from({length:4},()=>f.create('replace-probe'));for(const [node,id]of [[n,'n'],[a,'a'],[b,'b'],[first,'first']]){node.id=id;ids.set(node,id);node.saved=null;}
 const value=s=>s==='U'?undefined:s==='N'?null:s==='a'?a:s==='b'?b:first;
 const array=s=>s==='null'?null:s==='first'?[first]:s==='U'?[undefined]:s==='N'?[null]:s==='prototype'?Object.setPrototypeOf(Array(1),[first]):s===''?[]:[...s].map(value);
 original=n.childNodes=array(i.shape);alternate=[first,b];a.saved=b.saved=n;if(i.mode==='old-parent-other')b.saved=first;
 const snapshot=v=>v===null?'N':v.length+':'+Array.from({length:v.length},(_,j)=>Number(Object.hasOwn(v,j))).join('')+':'+Array.from({length:v.length},(_,j)=>Number(j in v)).join('')+':'+Array.from({length:v.length},(_,j)=>tag(v[j])).join(',');
 let result='THREW',error='ok';active=true;try{result=tag(core.AbstractMmlNode.prototype.replaceChild.call(n,value(i.new),value(i.old)));}catch(e){error=e.name+'|'+e.message;}active=false;
 return error+'|'+result+'|'+snapshot(n.childNodes)+'|'+snapshot(original)+'|'+snapshot(alternate)+'|'+[n,a,b,first].map(x=>tag(x.saved)).join(',')+'|'+trace.join(',');
}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'tools/math-port/profiles/evidence/p31-replace-child-source.json');
assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);
console.log(JSON.stringify({source_calls:rows.length,source_errors:rows.filter(r=>!r.expected.startsWith('ok|')).length,profile_sha256:crypto.createHash('sha256').update(data).digest('hex'),mode:'source-profile-check-only'}));
