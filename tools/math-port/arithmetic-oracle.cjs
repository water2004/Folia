// Root independent observations of actual pinned fraction/radical semantic classes.
'use strict';

// Lossless scalar and own-key encoding; this file executes no C++ emitter.
const crypto=require('node:crypto');
function encode(v){if(v===undefined)return {$type:'undefined'};if(typeof v==='number'){if(Number.isNaN(v))return {$type:'number',value:'NaN'};if(Object.is(v,-0))return {$type:'number',value:'-0'};if(!Number.isFinite(v))return {$type:'number',value:String(v)};}if(v&&typeof v==='object'){const r=Array.isArray(v)?new Array(v.length):{};for(const k of Object.keys(v))Object.defineProperty(r,k,{value:encode(v[k]),enumerable:true,writable:true,configurable:true});return r;}return v;}
const records=[];let current=null;
function start(name){current={name,observations:[]};records.push(current);}
function record(v){current.observations.push(encode(v));}
function finish(profileName){const profilePath=path.join(root,'docs/math-port/evidence',profileName);const expected=JSON.parse(fs.readFileSync(profilePath,'utf8'));assert.equal(records.length,expected.records.length);for(let i=0;i<records.length;i++){const a=records[i],b=expected.records[i];assert.equal(a.name,b.name);assert.equal(a.observations.length,b.observations.length,a.name+' count');for(let j=0;j<a.observations.length;j++){if(JSON.stringify(a.observations[j])!==JSON.stringify(b.observations[j])){function diff(x,y,p){if(require('node:util').isDeepStrictEqual(x,y))return null;if(!x||!y||typeof x!=='object'||typeof y!=='object')return {path:p,actual:x,expected:y};for(const k of new Set([...Object.keys(x),...Object.keys(y)])){const d=diff(x[k],y[k],p+'/'+k);if(d)return d;}return {path:p,actualKeys:Object.keys(x),expectedKeys:Object.keys(y)};}throw Error(JSON.stringify({case:a.name,observation:j,...diff(a.observations[j],b.observations[j],'')}));}}}console.log(JSON.stringify({profile:profileName,cases:records.length,observations:records.reduce((n,r)=>n+r.observations.length,0),mode:'check',sha256:crypto.createHash('sha256').update(fs.readFileSync(profilePath)).digest('hex')}));}

const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,MmlFactory,kinds,classes}=require(path.join(__dirname,'arithmetic-reference.cjs'));
const {AbstractMmlNode}=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNode.js'));
let arithmeticEvents=[],calls=[],texCalls=[],mode='',target=null,late=null,originalChildren=null;
class Record extends AbstractMmlNode {
 get kind(){return 'record';}
 setInheritedAttributes(a,d,l,p){arithmeticEvents.push('inherit/'+this.label);calls.push({node:this,a,d,l,p});if(this.label==='first'&&mode==='swap')target.childNodes=[this,late];if(this.label==='first'&&mode==='attributes'){a.onlyFirst='changed';a.shared[1]='changed';}}
 setTeXclass(prev){arithmeticEvents.push('tex/'+this.label+(prev?'/prev':'/null'));texCalls.push(this);if(this.label==='first'&&mode==='grow-rebind'){originalChildren.push(late);target.childNodes=[late];}if(this.label==='first'&&mode==='swap')target.childNodes=[this,late];return this;}
}
const proxies={};for(const k of kinds)proxies[k]=class extends classes[k]{hook(a,d,l,p){return super.setChildInheritedAttributes(a,d,l,p);}getPrevClass(p){arithmeticEvents.push('previous');return super.getPrevClass(p);}};
function factory(){const f=new MmlFactory();f.setNodeClass('record',Record);for(const k of kinds)f.setNodeClass(k,proxies[k]);return f;}
function reset(){arithmeticEvents=[];calls=[];texCalls=[];mode='';target=late=originalChildren=null;}
function setup(k){reset();const f=factory(),n=f.create(k),r0=f.create('record'),r1=f.create('record'),r2=f.create('record');r0.label='first';r1.label='second';r2.label='late';n.childNodes=[r0,r1];r0.parent=r1.parent=r2.parent=n;return {f,n,r0,r1,r2};}

let cases=0;

function pp(v){record(v);}
function eq(v){record(v);}
function begin(name){cases++;start('math_port_arithmetic_'+name);}function end(){}
function attempt(fn){let error;try{fn();}catch(e){error=e;}if(error)record(error.name);return error;}
function observeCalls(a){eq(calls.length);calls.forEach((c,i)=>{pp(c.a);pp(c.d);pp(c.l);pp(c.p);eq(c.a===a);});}
begin('source_defaults_fields_and_accessors');for(const k of kinds){const n=new MmlFactory().create(k);eq(n.kind);eq(n.arity);eq(n.linebreakContainer);eq(n.linebreakAlign);pp(n.texClass);for(const name of Object.keys(classes[k].defaults))pp(classes[k].defaults[name]);if(k==='msqrt')eq(n.childNodes[0].isInferred);}eq(classes.msqrt.defaults===classes.mroot.defaults);end();
begin('raw_inheritance_matrix');
const levels=[undefined,null,false,true,-1,-0,0,1,2,NaN,Infinity,-Infinity,'','0','2','2x',[],[2],{}, {toString:false,valueOf:false}],displays=[undefined,null,false,true,'raw',[]],primes=[undefined,false,true,'raw'];
const matrix=[];
for(const k of kinds)for(const d of displays)for(const l of levels)for(const prime of primes){const s=setup(k),a={shared:['test','old']};let error;try{s.n.hook(a,d,l,prime);}catch(e){error=e;}matrix.push({k,input:[d,l,prime],error:error?.name||'',observed:calls.map(c=>({a:c.a,values:[c.d,c.l,c.p],alias:c.a===a}))});}record(matrix);end();
begin('alignment_raw_values_and_indentation');for(const [num,den]of [['left','right'],[null,false],[0,''],[['x'],{x:1}],['_inherit_','_inherit_']]){const s=setup('mfrac');s.n.attributes.set('numalign',num);s.n.attributes.set('denomalign',den);const a={};attempt(()=>s.n.hook(a,true,0,'raw'));observeCalls(a);}end();
begin('own_spread_proto_and_shallow_aliases');for(const k of kinds){const s=setup(k),a={shared:['test','old']};Object.defineProperty(a,'__proto__',{value:['test','own'],enumerable:true,writable:true});mode='attributes';attempt(()=>s.n.hook(a,true,0,false));observeCalls(a);pp(a);for(let i=0;i<calls.length;i++)eq(calls[i].a.shared===a.shared);}end();
begin('child_inheritance_rebinding');for(const k of kinds){const s=setup(k),a={};mode='swap';target=s.n;late=s.r2;attempt(()=>s.n.hook(a,true,0,true));observeCalls(a);for(let i=0;i<calls.length;i++)eq(calls[i].node===s.r2);}end();
begin('tex_class_transfer_iteration_and_live_children');for(const k of kinds)for(const change of ['', 'swap','grow-rebind']){const s=setup(k),prev=s.f.create('record');prev.texClass='raw';prev.attributes.set('scriptlevel','2');mode=change;target=s.n;late=s.r2;originalChildren=s.n.childNodes;let result;attempt(()=>{result=s.n.setTeXclass(prev);});eq(result===s.n);pp(s.n.prevClass);pp(s.n.prevLevel);pp(s.n.texClass);record(arithmeticEvents);eq(texCalls.length);for(let i=0;i<texCalls.length;i++)eq(texCalls[i]===s.r2);}end();
begin('malformed_children_partial_state');for(const k of kinds)for(const shape of ['null-array','empty','null-first','missing-second','null-second'])for(const op of ['tex','inherit']){const s=setup(k),prev=s.f.create('record');prev.texClass='raw';prev.attributes.set('scriptlevel',3);s.n.childNodes=shape==='null-array'?null:shape==='empty'?[]:shape==='null-first'?[null,s.r1]:shape==='missing-second'?[s.r0]:[s.r0,null];const a={};attempt(()=>op==='tex'?s.n.setTeXclass(prev):s.n.hook(a,true,0,false));eq(calls.length);eq(texCalls.length);pp(s.n.prevClass);pp(s.n.prevLevel);}end();

finish("source-arithmetic-profile.json");
