// Root independent actual-CJS atom, shared-class and semantics observations.
'use strict';

// Lossless scalar and own-key encoding; this file executes no C++ emitter.
const crypto=require('node:crypto');
function encode(v){if(v===undefined)return {$type:'undefined'};if(typeof v==='number'){if(Number.isNaN(v))return {$type:'number',value:'NaN'};if(Object.is(v,-0))return {$type:'number',value:'-0'};if(!Number.isFinite(v))return {$type:'number',value:String(v)};}if(v&&typeof v==='object'){const r=Array.isArray(v)?new Array(v.length):{};for(const k of Object.keys(v))Object.defineProperty(r,k,{value:encode(v[k]),enumerable:true,writable:true,configurable:true});return r;}return v;}
const records=[];let current=null;
function start(name){current={name,observations:[]};records.push(current);}
function record(v){current.observations.push(encode(v));}
function finish(profileName){const profilePath=path.join(root,'docs/math-port/evidence',profileName);const expected=JSON.parse(fs.readFileSync(profilePath,'utf8'));assert.equal(records.length,expected.records.length);for(let i=0;i<records.length;i++){const a=records[i],b=expected.records[i];assert.equal(a.name,b.name);assert.equal(a.observations.length,b.observations.length,a.name+' count');for(let j=0;j<a.observations.length;j++){if(JSON.stringify(a.observations[j])!==JSON.stringify(b.observations[j])){function diff(x,y,p){if(require('node:util').isDeepStrictEqual(x,y))return null;if(!x||!y||typeof x!=='object'||typeof y!=='object')return {path:p,actual:x,expected:y};for(const k of new Set([...Object.keys(x),...Object.keys(y)])){const d=diff(x[k],y[k],p+'/'+k);if(d)return d;}return {path:p,actualKeys:Object.keys(x),expectedKeys:Object.keys(y)};}throw Error(JSON.stringify({case:a.name,observation:j,...diff(a.observations[j],b.observations[j],'')}));}}}console.log(JSON.stringify({profile:profileName,cases:records.length,observations:records.reduce((n,r)=>n+r.observations.length,0),mode:'check',sha256:crypto.createHash('sha256').update(fs.readFileSync(profilePath)).digest('hex')}));}

const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,MmlFactory,kinds,classes,core,Mo}=require(path.join(__dirname,'atom-semantics-reference.cjs'));

let enabled=false,phase=false,trace=[],mode='',owner=null,late=null,phaseBag=null;
const note=s=>{if(enabled)trace.push(s);},clone=v=>Array.isArray(v)?v.map(clone):v&&typeof v==='object'?Object.fromEntries(Object.keys(v).map(k=>[k,clone(v[k])])):v;
function reset(){enabled=phase=false;trace=[];mode='';owner=late=phaseBag=null;}
function proxy(T){return class extends T {
 get texClass(){note('tex.read');const v=super.texClass;return mode==='constructor-getter'&&v===0?'virtual':v;}
 set texClass(v){note('tex.write');super.texClass=v;}
 get prevClass(){note('prev.read');return this.savedPrev;}
 set prevClass(v){note('prev.write');if(mode==='prev-setter-throw'){owner=this;throw Object.assign(new Error('witness'),{name:'WitnessError'});}this.savedPrev=v;}
 get prevLevel(){note('level.read');return this.savedLevel;}
 set prevLevel(v){note('level.write');if(mode==='level-setter-throw'){owner=this;throw Object.assign(new Error('witness'),{name:'WitnessError'});}this.savedLevel=v;}
 get attributes(){note('attributes');return this.savedAttributes;}
 set attributes(a){note('attributes.set');this.savedAttributes=a;}
 getProperty(k){note('get/'+k);return super.getProperty(k);}
 setProperty(k,v){note('set/'+k);return super.setProperty(k,v);}
 setChildren(c){if(phase){note('phase/'+String(this.texclass));phaseBag=this.properties;}return super.setChildren(c);}
 hook(a,d,l,p){return super.setChildInheritedAttributes(a,d,l,p);}
};}
class Record extends core.AbstractMmlNode {get kind(){return 'record';}setTeXclass(p){note('child/'+(p?'prev':'null'));if(mode==='child-throw')throw Object.assign(new Error('witness'),{name:'WitnessError'});if(mode==='child-rebind')owner.childNodes=[late];if(mode==='child-write')owner.texClass=2;return late;}
 setInheritedAttributes(a,d,l,p){note('inherit/record');this.received={a,d,l,p};}}
const probes={};for(const k of kinds)probes[k]=proxy(classes[k]);probes.mo=proxy(Mo);probes.record=proxy(Record);
class Dispatch extends probes.TeXAtom {adjustTeXclass(prev){note('adjust/'+(prev?'prev':'null'));return late;}}
function factory(){const f=new MmlFactory();for(const k of [...kinds,'mo','record'])f.setNodeClass('probe-'+k,probes[k]);f.setNodeClass('dispatch',Dispatch);f.setNodeClass('record',Record);return f;}
function snapshot(n){if(!n)return null;const a=n.attributes;return {kind:n.kind,parent:n.parent?.kind,properties:clone(n.properties),tex:n.texClass,prev:n.prevClass,level:n.prevLevel,explicit:clone(a?.getAllAttributes()),inherited:clone(a?.getAllInherited()),children:n.childNodes?.map(snapshot)};}

let cases=0;
function begin(name){reset();cases++;start('math_port_atom_semantics_'+name);}function end(){enabled=false;}
function pp(v){record(v);}function eq(v){record(v);}
function observe(fn){enabled=true;trace=[];let error='';try{fn();}catch(e){error=e.name;}enabled=false;record(error);record(trace);}
begin('defaults_getters_class_identity');{const f=factory();for(const k of kinds){const n=f.create(k);for(const prop of ['kind','arity','notParent','linebreakContainer'])eq(n[prop]);pp(n.texClass);pp(n.properties);pp(classes[k].defaults);}eq(classes.TeXAtom.defaults===core.AbstractMmlBaseNode.defaults);eq(classes.semantics.defaults===core.AbstractMmlBaseNode.defaults);eq(classes['annotation-xml'].defaults===core.AbstractMmlNode.defaults);eq(classes.annotation.defaults===classes['annotation-xml'].defaults);}end();
for(const k of ['TeXAtom','annotation'])for(const change of ['', 'constructor-getter']){begin('constructor_'+k+'_'+(change||'normal'));const f=factory();mode=change;phase=true;let n;observe(()=>n=f.create('probe-'+k,{},[]));phase=false;pp(snapshot(n));eq(phaseBag===n.properties);end();}
for(const change of ['prev-setter-throw','level-setter-throw']){begin('constructor_partial_'+change);const f=factory();mode=change;observe(()=>f.create('probe-TeXAtom'));pp(snapshot(owner));end();}
for(const k of ['TeXAtom','mo'])for(const extra of [false,true])for(const sibling of [false,true]){begin('real_shared_parent_'+k+'_'+extra+'_'+sibling);const f=factory(),n=f.create('probe-'+k),prev=f.create('probe-record');n.texClass=2;prev.texClass=0;const atom=k==='mo'?f.create('TeXAtom',{},[n]):n,sem=f.create('semantics',{},extra?[atom,f.create('annotation')]:[atom]),holder=f.create('mrow',{},sibling?[sem,f.create('mi')]:[sem]);let result=null;observe(()=>result=n.adjustTeXclass(prev));eq(result===n);pp(n.texClass);pp(n.prevClass);pp(n.prevLevel);end();}
for(const k of ['TeXAtom','mo']){begin('raw_adjustment_'+k);const rows=[];for(const current of [-1,0,1,2,3,4,5,6,7,null,undefined,'2',false,NaN])for(const previous of [null,-1,0,1,2,3,4,5,6,'raw',false])for(const auto of [false,true,'raw']){const f=factory(),n=f.create('probe-'+k),prev=previous===null?null:f.create('probe-record');n.texClass=current;n.prevClass='seed';n.prevLevel='level-seed';n.attributes.setInherited('scriptlevel','raw-level');if(prev){prev.texClass=previous;prev.setProperty('autoOP',auto);}const holder=f.create('mrow',{},[n]);let result=null,err='';try{result=n.adjustTeXclass(prev);}catch(e){err=e.name;}rows.push({current,previous,auto,error:err,self:result===n,prevResult:result===prev,tex:n.texClass,prevClass:n.prevClass,prevLevel:n.prevLevel,previousTex:prev?.texClass});}
 record(rows);end();}
for(const k of ['TeXAtom','mo'])for(const state of ['none','auto-op','prev-binary','last','first','detached','nested','null-array','attributes-null']){begin('adjust_order_'+k+'_'+state);const f=factory(),n=f.create('probe-'+k),prev=f.create('probe-record');n.texClass=state==='none'?-1:state==='prev-binary'?3:2;prev.texClass=state==='prev-binary'?2:0;prev.setProperty('autoOP',state==='auto-op');n.attributes.setInherited('scriptlevel','2');if(state!=='detached'){const holder=f.create('mrow',{},state==='first'?[n,f.create('mi')]:[n]);if(state==='nested')f.create('mrow',{},[holder]);if(state==='null-array')holder.childNodes=null;}if(state==='attributes-null')n.attributes=null;let result=null;observe(()=>result=n.adjustTeXclass(prev));eq(result===n);eq(result===prev);pp(n.texClass);pp(n.prevClass);pp(n.prevLevel);pp(prev.texClass);end();}
for(const change of ['', 'child-write','child-rebind','child-throw','empty','null-child','null-array','dispatch'])for(const previous of [false,true]){begin('child_dispatch_'+cases);const f=factory(),n=f.create(change==='dispatch'?'dispatch':'probe-TeXAtom'),c=f.create('record'),prev=previous?f.create('probe-record'):null;owner=n;late=f.create('record');n.childNodes=change==='empty'?[]:change==='null-array'?null:[change==='null-child'?null:c];c.parent=n;mode=change;const holder=f.create('mrow');n.parent=holder;holder.childNodes=[n];let result=null;observe(()=>result=n.setTeXclass(prev));eq(result===n);eq(result===late);pp(snapshot(n));end();}
for(const k of ['semantics','annotation-xml','annotation'])for(const change of ['normal','null-child','null-array']){begin('inherit_hook_'+k+'_'+change);const f=factory(),n=f.create('probe-'+k),c=f.create('record');n.childNodes=change==='null-child'?[null]:change==='null-array'?null:[c];c.parent=n;const a={shared:['origin','value']};observe(()=>n.hook(a,'raw','2','prime'));pp(c.received);end();}
for(const k of kinds)for(const contents of [0,1,2]){begin('outer_'+k+'_'+contents);const f=factory(),cs=Array.from({length:contents},()=>f.create('mi')),n=f.create(k,{displaystyle:true},cs),holder=f.create('mrow',{},[n]);observe(()=>n.setInheritedAttributes({mathcolor:['math','blue']},true,0,false));pp(snapshot(holder));for(let i=0;i<cs.length;i++){eq(cs[i].Parent===holder);eq(cs[i].Parent===n);}end();}
begin('logical_parent');{const f=factory(),child=f.create('mi'),atom=f.create('TeXAtom',{},[child]),sem=f.create('semantics',{},[atom,f.create('annotation')]),holder=f.create('mrow',{},[sem]);eq(child.Parent===holder);eq(atom.Parent===holder);eq(sem.Parent===holder);}end();

finish("source-atom-semantics-profile.json");
