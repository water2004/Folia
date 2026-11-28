// Root actual original token walk, callbacks, sparse arrays and data identity.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root,MmlFactory,core}=require('./inheritance-reference.cjs');
const shapes=['','H','U','N','a','b','x','ab','ax','xa','Hab','UaNbx','aba','prototype','null'];
const modes=['','root-rebind','root-null','root-grow','child-grow','child-delete','child-shrink','child-rebind','child-prototype','throw-root','throw-a','override-return','override-throw'];
const inputs=[];for(const shape of shapes)for(const mode of modes)for(const data of ['U','N','obj','7'])inputs.push({shape,mode,data});
function run(i){let active=false,trace=[],original,n,a,b,c,x;const ids=new Map(),note=s=>{if(active)trace.push(s);},fail=()=>{throw Object.assign(new Error('witness'),{name:'WitnessError'});};
 class Token extends core.AbstractMmlTokenNode{get kind(){return 'walk-token';}}
 class Record extends core.AbstractMmlNode{get kind(){return 'walk-record';}walkTree(fn,data){note('enter:'+ids.get(this));if(active&&this===a&&i.mode==='override-throw')fail();if(active&&this===a&&i.mode==='override-return')return {count:999};return super.walkTree(fn,data);}}
 class Text extends core.TextNode{walkTree(fn,data){note('text.walk');return super.walkTree(fn,data);}}
 const f=new MmlFactory();f.setNodeClass('walk-token',Token);f.setNodeClass('walk-record',Record);f.setNodeClass('walk-text',Text);
 n=f.create('walk-token');[a,b,c]=Array.from({length:3},()=>f.create('walk-record'));x=f.create('walk-text');for(const [node,id]of [[n,'n'],[a,'a'],[b,'b'],[c,'c'],[x,'x']])ids.set(node,id);
 const value=s=>s==='U'?undefined:s==='N'?null:s==='a'?a:s==='b'?b:s==='c'?c:x;
 const array=s=>{if(s==='null')return null;if(s==='prototype'){const v=Array(3),p=[];p[0]=a;p[1]=x;Object.setPrototypeOf(v,p);v[2]=b;return v;}const v=Array(s.length);for(let j=0;j<s.length;j++)if(s[j]!=='H')v[j]=value(s[j]);return v;};
 n.childNodes=array(i.shape);original=n.childNodes;const data=i.data==='U'?undefined:i.data==='N'?null:i.data==='obj'?{count:0}:7;
 const dt=v=>v===undefined?'U':v===null?'N':typeof v==='object'?'obj:'+v.count:String(v);
 function visit(node,d){note('visit:'+ids.get(node)+'/'+dt(d));if(d&&typeof d==='object')d.count++;if(node===n){if(i.mode==='throw-root')fail();if(i.mode==='root-rebind')n.childNodes=[b];if(i.mode==='root-null')n.childNodes=null;if(i.mode==='root-grow'&&original)original.push(c);}if(node===a){if(i.mode==='throw-a')fail();if(i.mode==='child-grow'){original.push(c);n.childNodes=[];}if(i.mode==='child-delete')delete original[1];if(i.mode==='child-shrink')original.length=1;if(i.mode==='child-rebind')n.childNodes=[c];if(i.mode==='child-prototype'){const p=[];p[1]=c;Object.setPrototypeOf(original,p);delete original[1];}}}
 const tag=v=>v===undefined?'U':v===null?'N':ids.get(v)??'UNBOUND';
 const snapshot=v=>v===null?'N':v.length+':'+Array.from({length:v.length},(_,j)=>Number(Object.hasOwn(v,j))).join('')+':'+Array.from({length:v.length},(_,j)=>Number(j in v)).join('')+':'+Array.from({length:v.length},(_,j)=>tag(v[j])).join(',');
 let result='THREW',error='ok';active=true;try{const r=core.AbstractMmlTokenNode.prototype.walkTree.call(n,visit,data);result=String(r===data)+'/'+dt(r);}catch(e){error=e.name+'|'+e.message;}active=false;
 return error+'|'+result+'|'+dt(data)+'|'+snapshot(n.childNodes)+'|'+snapshot(original)+'|'+trace.join(',');
}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'tools/math-port/profiles/evidence/p28-token-walk-source.json');
assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);
console.log(JSON.stringify({source_calls:rows.length,source_errors:rows.filter(r=>!r.expected.startsWith('ok|')).length,profile_sha256:crypto.createHash('sha256').update(data).digest('hex'),mode:'source-profile-check-only'}));
