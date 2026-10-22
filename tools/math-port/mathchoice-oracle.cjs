// Root direct pinned-CJS oracle for the complete MathChoice semantic class.
'use strict';

// Lossless scalar and own-key encoding; this file executes no C++ emitter.
const crypto=require('node:crypto');
function encode(v){if(v===undefined)return {$type:'undefined'};if(typeof v==='number'){if(Number.isNaN(v))return {$type:'number',value:'NaN'};if(Object.is(v,-0))return {$type:'number',value:'-0'};if(!Number.isFinite(v))return {$type:'number',value:String(v)};}if(v&&typeof v==='object'){const r=Array.isArray(v)?new Array(v.length):{};for(const k of Object.keys(v))Object.defineProperty(r,k,{value:encode(v[k]),enumerable:true,writable:true,configurable:true});return r;}return v;}
const records=[],sourceMetadata=[];let current=null;
function start(name){current={name,observations:[]};records.push(current);}
function record(v){current.observations.push(encode(v));}
function finish(profileName,quiet=false){const profilePath=path.join(root,'docs/math-port/evidence',profileName);const expected=JSON.parse(fs.readFileSync(profilePath,'utf8'));assert.equal(records.length,expected.records.length);for(let i=0;i<records.length;i++){const a=records[i],b=expected.records[i];assert.equal(a.name,b.name);assert.equal(a.observations.length,b.observations.length,a.name+' count');for(let j=0;j<a.observations.length;j++){if(JSON.stringify(a.observations[j])!==JSON.stringify(b.observations[j])){function diff(x,y,p){if(require('node:util').isDeepStrictEqual(x,y))return null;if(!x||!y||typeof x!=='object'||typeof y!=='object')return {path:p,actual:x,expected:y};for(const k of new Set([...Object.keys(x),...Object.keys(y)])){const d=diff(x[k],y[k],p+'/'+k);if(d)return d;}return {path:p,actualKeys:Object.keys(x),expectedKeys:Object.keys(y)};}throw Error(JSON.stringify({case:a.name,observation:j,...diff(a.observations[j],b.observations[j],'')}));}}}if(!quiet)console.log(JSON.stringify({profile:profileName,cases:records.length,observations:records.reduce((n,r)=>n+r.observations.length,0),mode:'check',sha256:crypto.createHash('sha256').update(fs.readFileSync(profilePath)).digest('hex')}));}

const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,MmlFactory,core,MathChoice}=require(path.join(__dirname,'inheritance-reference.cjs'));

let active=false,events=[],calls=[],raw,mode='',choice,other,late;
const note=x=>{if(active)events.push(x);},id=n=>n?n.getProperty('id'):null;
class Receive extends core.AbstractMmlNode {
get kind(){return 'receive';}
setInheritedAttributes(a,d,l,p){note('inherit');calls.push({id:id(this),a,d,l,p,same:a===raw});if(mode==='inherit-throw')throw Object.assign(new Error('witness'),{name:'WitnessError'});}
}
class Row extends Receive {get kind(){return 'mrow';}}
class Parent extends core.AbstractMmlNode {
get kind(){return 'parent';}
replaceChild(n,o){note('replace:'+id(this));if(mode==='replace-before')throw Object.assign(new Error('witness'),{name:'WitnessError'});const result=super.replaceChild(n,o);if(mode==='replace-after')throw Object.assign(new Error('witness'),{name:'WitnessError'});if(mode==='replace-rebind')this.childNodes=[late];return mode==='replace-return-null'?null:result;}
}
function setup(shape,scenario){active=false;events=[];calls=[];mode=scenario;
const f=new MmlFactory();f.setNodeClass('MathChoice',MathChoice);f.setNodeClass('receive',Receive);f.setNodeClass('parent',Parent);f.setNodeClass('mrow',Row);
const rs=[0,1,2,3].map(i=>{const n=f.create('receive');n.setProperty('id',i);return n;});
choice=f.create('MathChoice',{},rs);choice.setProperty('id',99);const parent=f.create('parent');parent.setProperty('id',10);other=f.create('parent');other.setProperty('id',11);late=f.create('receive');late.setProperty('id',8);parent.appendChild(choice);
if(shape==='empty')choice.childNodes=[];if(shape==='null-array')choice.childNodes=null;if(shape==='null-slot')choice.childNodes=[rs[0],null,rs[2],rs[3]];if(shape==='short')choice.childNodes=[rs[0]];
if(scenario==='detached')choice.parent=null;
const create=f.create;f.create=function(kind,...args){if(active&&kind==='mrow'){note('factory:'+args.length);if(mode==='factory-throw')throw Object.assign(new Error('witness'),{name:'WitnessError'});if(mode==='factory-null')return null;if(mode==='factory-reparent')choice.parent=other;if(mode==='factory-rebind')choice.childNodes=[late];const n=create.call(this,kind,...args);n.setProperty('id',4);return n;}return create.call(this,kind,...args);};
return {f,n:choice,parent,other,rs};}
function observe(s,args,count){let error='';active=true;try{s.n.setInheritedAttributes(...args.slice(0,count));}catch(e){error=e.name;}active=false;return {error,kind:s.n.kind,arity:s.n.arity,notParent:s.n.notParent,parent:id(s.n.parent),children:s.n.childNodes?.map(id)??null,first:s.parent.childNodes.map(id),second:s.other.childNodes.map(id),rs:s.rs.map(n=>id(n.parent)),calls,events};}
function add(name,attrs,display,level,prime,count=4,shape='full',scenario=''){
const s=setup(shape,scenario);raw=attrs;const wanted=observe(s,[raw,display,level,prime],count);
sourceMetadata.push(['call',name,count,shape,scenario]);start('math_port_mathchoice_'+name);record(wanted);}
const levels=[undefined,null,false,true,-Infinity,-3,-0,0,0.5,1,1.5,2,3,Infinity,NaN,'','2','x',[],[1],[1,2],{}, {valueOf:0,toString:null}];
const displays=[undefined,null,false,true,0,-0,NaN,'',[],{},'raw'];
let index=0;for(const d of displays)for(const l of levels)add('selection_'+index++,{mathcolor:['math','blue']},d,l,'raw-prime');
for(const a of [undefined,null,false,3,'xy',[],{}, {mathsize:['math','2em']}])for(const count of [0,1,2,3,4])add('arity_'+index++,a,true,1,true,count);
for(const shape of ['full','empty','null-array','null-slot','short'])for(const scenario of ['', 'detached','factory-throw','factory-null','factory-reparent','factory-rebind','inherit-throw','replace-before','replace-after','replace-rebind','replace-return-null'])for(const level of [0,NaN])add('state_'+index++,null,false,level,MissingValue(),4,shape,scenario);
function MissingValue(){return undefined;}
function tree(n){return {kind:n.kind,text:n.kind==='text'?n.getText():null,inherited:n.attributes?.getAllInherited()??null,properties:n.getAllProperties(),children:n.childNodes.map(tree)};}
for(const display of ['inline','block'])for(const level of [0,1,2]){
active=false;const f=new MmlFactory();f.setNodeClass('MathChoice',MathChoice);
const token=text=>f.create('mi',{},[f.create('text').setText(text)]);
const inner=f.create('MathChoice',{},['D','T','S','SS'].map(x=>f.create('mrow',{},[token(x)])));
const branches=['display','text','script','scriptscript'].map(x=>f.create('mrow',{},[token(x),f.create('MathChoice',{},['α','β','γ','δ'].map(y=>f.create('mrow',{},[token(y)])))]));
branches[0].appendChild(inner);const math=f.create('math',{display},[f.create('MathChoice',{},branches)]);math.setInheritedAttributes({mathcolor:['math','blue']},false,level,false);const wanted=tree(math);
sourceMetadata.push(['real',display,level]);start('math_port_mathchoice_real_'+display+'_'+level);record(wanted);
}
const defaults=Object.assign({},MathChoice.defaults);assert.notEqual(MathChoice.defaults,core.AbstractMmlBaseNode.defaults);const old=core.AbstractMmlBaseNode.defaults['choice-root-probe'];core.AbstractMmlBaseNode.defaults['choice-root-probe']='later';assert.equal(MathChoice.defaults['choice-root-probe'],undefined);if(old===undefined)delete core.AbstractMmlBaseNode.defaults['choice-root-probe'];else core.AbstractMmlBaseNode.defaults['choice-root-probe']=old;
start('math_port_mathchoice_defaults');record(defaults);

module.exports={records,sourceMetadata,check:()=>finish("source-mathchoice-profile.json",true)};
if(require.main===module)finish("source-mathchoice-profile.json");
