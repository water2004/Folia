// Root actual original Layout/Base class algorithms under controlled virtual services.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');const{root,MmlFactory,core}=require('./inheritance-reference.cjs');
const modes=['','rebind-base','base-null-array','mutate-tail','rebind-tail','throw-base','throw-script','throw-prev','throw-tex','core-N','core-U','ret-N','ret-U','ret-prev','ret-q'],inputs=[];
for(const family of ['layout','base'])for(const prev of ['N','U','p'])for(const base of ['N','U','b'])for(const kind of ['mi','mn','TeXAtom'])for(const embellished of [false,true])for(const mode of modes)for(const shape of ['dense','hole','prototype','null'])inputs.push({family,prev,base,kind,embellished,mode,shape});
function run(i){let active=false,trace=[],n,b,q,p,s1,s2;const id=v=>v===null?'N':v===undefined?'U':v.id;const note=v=>{if(active)trace.push(v);};const fail=()=>{throw Object.assign(new Error('witness'),{name:'WitnessError'});};
 const mix=Base=>class extends Base{get kind(){return i.family+'Probe';}get texClass(){note(this.id+'.tex');return this.cur;}set texClass(v){note(this.id+'.T='+String(v));this.cur=v;if(active&&this.id==='n'&&i.mode==='throw-tex')fail();}getPrevClass(v){note('n.prev='+id(v));this.pc=id(v);if(i.mode==='throw-prev')fail();}updateTeXclass(v){note('n.update='+id(v));this.cur=v?.cur??-8;}get isEmbellished(){note('n.emb');return i.embellished;}core(){note('n.core');return i.mode==='core-N'?null:i.mode==='core-U'?undefined:i.mode==='base-null-array'?q:this.childNodes[0];}};
 class Layout extends mix(core.AbstractMmlLayoutNode){}class Base extends mix(core.AbstractMmlBaseNode){}
 class Leaf extends core.AbstractMmlNode{get kind(){return 'leaf';}isKind(k){note(this.id+'.kind='+k);return this.classKind===k;}get texClass(){note(this.id+'.tex');return this.cur;}setTeXclass(v){note(this.id+'.set='+id(v));this.calls++;this.cur+=1;
  if(this.id==='b'){if(i.mode==='rebind-base')n.childNodes=[q,s1,s2];if(i.mode==='base-null-array')n.childNodes=null;if(i.mode==='throw-base')fail();}
  if(this.id==='s1'){if(i.mode==='mutate-tail')n.childNodes.length=1;if(i.mode==='rebind-tail')n.childNodes=[q];if(i.mode==='throw-script')fail();}
  return i.mode==='ret-N'?null:i.mode==='ret-U'?undefined:i.mode==='ret-prev'?v:i.mode==='ret-q'?q:this;
 }}
 const f=new MmlFactory();f.setNodeClass('layoutProbe',Layout);f.setNodeClass('baseProbe',Base);f.setNodeClass('leaf',Leaf);n=f.create(i.family+'Probe');n.id='n';n.cur=17;n.pc='unset';[b,q,p,s1,s2]=Array.from({length:5},()=>f.create('leaf'));
 for(const[x,name,value]of [[b,'b',3],[q,'q',8],[p,'p',2],[s1,'s1',5],[s2,'s2',6]]){x.id=name;x.cur=value;x.calls=0;x.classKind=i.kind;}
 const bv=i.base==='b'?b:i.base==='N'?null:undefined;let children;if(i.shape==='null')children=null;else if(i.shape==='hole'){children=Array(5);children[0]=bv;children[2]=null;children[4]=s2;}else if(i.shape==='prototype'){children=Array(4);children[0]=bv;const proto=[];proto[1]=s1;Object.setPrototypeOf(children,proto);children[3]=s2;}else children=[bv,s1,s2];n.childNodes=children;
 active=true;let result='THREW',error='ok';try{result=id((i.family==='layout'?core.AbstractMmlLayoutNode:core.AbstractMmlBaseNode).prototype.setTeXclass.call(n,i.prev==='p'?p:i.prev==='N'?null:undefined));}catch(e){error=e.name+'|'+e.message;}active=false;
 const slots=n.childNodes===null?'NULL':Array.from(n.childNodes,id).join(',');return error+'|'+result+'|'+[n.cur,n.pc,b.cur,q.cur,p.cur,s1.cur,s2.cur,b.calls,s1.calls,s2.calls].join(',')+'|'+slots+'|'+trace.join(',');
}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n',profile=path.join(root,'tools/math-port/profiles/evidence/p25-container-class-source.json');
assert.ok(process.argv.includes('--check'),'Source-only candidate is exact-check only');
assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);
console.log(JSON.stringify({source_calls:rows.length,source_errors:rows.filter(r=>!r.expected.startsWith('ok|')).length,sha256:crypto.createHash('sha256').update(data).digest('hex'),mode:'check'}));
