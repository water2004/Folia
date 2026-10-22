// Root source witnesses for constructor phases, virtual dispatch and outer inheritance.
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
let enabled=false,trace=[],change='',reads=0,additions=0,input=null,alternate=null;
const note=s=>{if(enabled)trace.push(s);};
const probes={};
for(const k of kinds)probes[k]=class extends classes[k]{
 get attributes(){note('attributes');const a=this.savedAttributes;if(enabled&&change==='receiver'&&++reads===1)this.savedAttributes=alternate;return a;}
 set attributes(a){this.savedAttributes=a;}
 setChildren(c){note('phase/'+String(this.texClass));return super.setChildren(c);}
 getPrevClass(p){note('previous');return super.getPrevClass(p);}
 addInheritedAttributes(a,b){note('add/'+Object.keys(b)[0]);if(enabled&&change==='copy-identity')note('input/'+(a===input));if(enabled&&change==='proto'){note('keys/'+Object.keys(a).join(','));note('prototype/'+Array.isArray(Object.getPrototypeOf(a)));}const result=super.addInheritedAttributes(a,b);if(enabled&&change==='helper'&&++additions===1)input.between=['test','later'];return result;}
 hook(a,d,l,p){return super.setChildInheritedAttributes(a,d,l,p);}
 add(a,b){return super.addInheritedAttributes(a,b);}
};
class Record extends AbstractMmlNode{get kind(){return 'record';}setInheritedAttributes(a,d,l,p){note('inherit/'+this.label);this.received={a,d,l,p};}setTeXclass(p){note('tex/'+this.label+'/'+String(p));return this;}}
function factory(){const f=new MmlFactory();f.setNodeClass('record',Record);for(const k of kinds)f.setNodeClass('probe-'+k,probes[k]);return f;}

let cases=0;

function begin(name){enabled=false;trace=[];change='';reads=additions=0;cases++;start('math_port_arithmetic_order_'+name);}
function end(){enabled=false;}
function pp(v){record(v);}
function observe(fn){enabled=true;trace=[];let error;try{fn();}catch(e){error=e;}enabled=false;if(error)record(error.name);record(trace);}
for(const k of kinds){begin('construction_phase_'+k);const f=factory();let n;observe(()=>n=f.create('probe-'+k));pp(n.texClass);end();}
for(const changeMode of ['','receiver','helper','proto','copy-identity']){begin('fraction_dispatch_'+(changeMode||'normal'));const f=factory(),n=f.create('probe-mfrac'),a=f.create('record'),b=f.create('record');a.label='first';b.label='second';n.childNodes=[a,b];n.attributes.set('numalign','left');n.attributes.set('denomalign','old');alternate=f.create('mfrac').attributes;alternate.set('denomalign','right');input={shared:['test','old']};change=changeMode;if(changeMode==='proto'){Object.defineProperty(input,'__proto__',{value:['test','own'],enumerable:true,writable:true});}observe(()=>n.hook(input,true,0,false));pp(a.received.a);pp(b.received.a);pp(input);end();}
begin('base_helper_own_spread');{const f=factory(),n=f.create('probe-mfrac'),a={shared:['test','old']};Object.defineProperty(a,'__proto__',{value:['test','own'],enumerable:true,writable:true});let result;observe(()=>result=n.add(a,{}));pp(result);record(result===a);record(result.shared===a.shared);}end();
function snapshot(n){const nodes=[];function walk(x){const row={kind:x.kind,children:x.childNodes.length,tex:x.texClass,prime:x.getProperty('texprimestyle')};if(x.attributes)row.inherited=x.attributes.getAllInherited();for(const c of x.childNodes)walk(c);nodes.push(row);}walk(n);return nodes;}
for(const k of kinds)for(const explicit of [false,true])for(const missing of [false,true]){begin('outer_'+k+'_'+explicit+'_'+missing);const f=new MmlFactory(),token=s=>f.create('mi',{},[f.create('text').setText(s)]),children=missing?[]:[token('x'),token('y')],attrs=explicit?{displaystyle:true,scriptlevel:3,indentalign:'right'}:{},n=f.create(k,attrs,children),a={mathcolor:['math','blue'],mathsize:['math','120%']};observe(()=>n.setInheritedAttributes(a,false,'2',true));pp(snapshot(n));end();}
begin('nested_outer_shared_tree');{const f=new MmlFactory(),token=s=>f.create('mi',{},[f.create('text').setText(s)]),frac=f.create('mfrac',{},[token('a'),token('b')]),sqrt=f.create('msqrt',{},[frac]),n=f.create('mroot',{},[sqrt,token('n')]);observe(()=>n.setInheritedAttributes({},false,0,false));pp(snapshot(n));}end();

finish("source-arithmetic-order-profile.json");
