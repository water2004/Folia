// Root actual-CJS oracle: complete maction, including observable partial state.
'use strict';

// Lossless scalar and own-key encoding; this file executes no C++ emitter.
const crypto=require('node:crypto');
function encode(v){if(v===undefined)return {$type:'undefined'};if(typeof v==='number'){if(Number.isNaN(v))return {$type:'number',value:'NaN'};if(Object.is(v,-0))return {$type:'number',value:'-0'};if(!Number.isFinite(v))return {$type:'number',value:String(v)};}if(v&&typeof v==='object'){const r=Array.isArray(v)?new Array(v.length):{};for(const k of Object.keys(v))Object.defineProperty(r,k,{value:encode(v[k]),enumerable:true,writable:true,configurable:true});return r;}return v;}
const records=[];let current=null;
function start(name){current={name,observations:[]};records.push(current);}
function record(v){current.observations.push(encode(v));}
function finish(profileName){const profilePath=path.join(root,'docs/math-port/evidence',profileName);const expected=JSON.parse(fs.readFileSync(profilePath,'utf8'));assert.equal(records.length,expected.records.length);for(let i=0;i<records.length;i++){const a=records[i],b=expected.records[i];assert.equal(a.name,b.name);assert.equal(a.observations.length,b.observations.length,a.name+' count');for(let j=0;j<a.observations.length;j++){if(JSON.stringify(a.observations[j])!==JSON.stringify(b.observations[j])){function diff(x,y,p){if(require('node:util').isDeepStrictEqual(x,y))return null;if(!x||!y||typeof x!=='object'||typeof y!=='object')return {path:p,actual:x,expected:y};for(const k of new Set([...Object.keys(x),...Object.keys(y)])){const d=diff(x[k],y[k],p+'/'+k);if(d)return d;}return {path:p,actualKeys:Object.keys(x),expectedKeys:Object.keys(y)};}throw Error(JSON.stringify({case:a.name,observation:j,...diff(a.observations[j],b.observations[j],'')}));}}}console.log(JSON.stringify({profile:profileName,cases:records.length,observations:records.reduce((n,r)=>n+r.observations.length,0),mode:'check',sha256:crypto.createHash('sha256').update(fs.readFileSync(profilePath)).digest('hex')}));}

const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,MmlFactory,core}=require(path.join(__dirname,'inheritance-reference.cjs'));
const {MmlMaction}=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNodes/maction.js'));
const {Attributes}=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/Attributes.js'));

let active=false,events=[],calls=[],mode='',raw,action,late,attrsReads=0,selectedReads=0;
const id=n=>n?n.getProperty('id'):null,note=x=>{if(active)events.push(x);},fail=()=>{throw Object.assign(new Error('witness'),{name:'WitnessError'});};
class Receive extends core.AbstractMmlNode{
get kind(){return 'receive';}get isEmbellished(){note('embellished:'+id(this));return id(this)===0;}get isSpacelike(){note('spacelike:'+id(this));return id(this)===1;}
core(){note('core:'+id(this));return late;}coreMO(){note('coreMO:'+id(this));return this;}
setTeXclass(prev){note('class:'+id(this)+':'+id(prev));if(mode==='class-throw')fail();this.prevClass=100+id(this);this.prevLevel=200+id(this);this.texClass=id(this);if(mode==='tip-rebind'&&id(this)===1)action.childNodes=[late];if(mode==='class-return-null')return null;if(mode==='class-return-late')return late;return this;}
setInheritedAttributes(a,d,l,p){note('inherit:'+id(this));calls.push({id:id(this),a,d,l,p,same:a===raw});if(mode==='inherit-first-throw'&&id(this)===0||mode==='inherit-second-throw'&&id(this)===1)fail();if(id(this)===0){if(mode==='inherit-rebind')action.childNodes=[this,late];if(mode==='inherit-null-array')action.childNodes=null;if(mode==='inherit-shorten')action.childNodes=[this];if(mode==='inherit-mutate-attrs')a.probe='written';}}
}
class Row extends Receive{get kind(){return 'mrow';}}
class Probe extends MmlMaction{
get selected(){note('selected');selectedReads++;if(mode==='selected-null')return null;if(mode==='selected-throw')fail();if(mode==='selected-rotate')return selectedReads===1?this.childNodes[0]:late;return Object.getOwnPropertyDescriptor(MmlMaction.prototype,'selected').get.call(this);}
updateTeXclass(n){note('update:'+id(n));if(mode==='update-throw')fail();super.updateTeXclass(n);}
mError(message,options,short=false){note('error:'+message+':'+short);if(mode==='verify-throw')fail();if(mode==='verify-rebind')this.slot=this.nextAttrs;return null;}
}
function setup(shape,scenario,type,selection,explicit=true){active=false;events=[];calls=[];mode=scenario;attrsReads=selectedReads=0;
const f=new MmlFactory();f.setNodeClass('maction',Probe);f.setNodeClass('receive',Receive);f.setNodeClass('mrow',Row);
const rs=[0,1,2].map(i=>{const n=f.create('receive');n.setProperty('id',i);n.texClass=i+3;n.prevClass=10+i;n.prevLevel=20+i;return n;});late=f.create('receive');late.setProperty('id',8);action=f.create('maction',{},rs);action.setProperty('id',99);action.slot=action.attributes;action.nextAttrs=new Attributes(MmlMaction.defaults,f.getNodeClass('math').defaults);action.nextAttrs.set('actiontype','statusline');action.nextAttrs.set('selection',7);action.slot.set('actiontype',type);if(explicit)action.slot.set('selection',selection);
Object.defineProperty(action,'attributes',{get(){if(active){attrsReads++;note('attrs:'+attrsReads);if(mode==='attrs-null'||mode==='attrs-null-second'&&attrsReads===2)return null;if(mode==='attrs-rebind'&&attrsReads===2)this.slot=this.nextAttrs;}return this.slot;},set(value){this.slot=value;}});
if(shape==='empty')action.childNodes=[];if(shape==='short')action.childNodes=[rs[0]];if(shape==='null-array')action.childNodes=null;if(shape==='null-slot')action.childNodes=[rs[0],null,rs[2]];
const create=f.create;f.create=function(kind,...args){if(active&&kind==='mrow'){note('factory:'+args.length);if(mode==='factory-throw')fail();if(mode==='factory-null')return null;const n=create.call(this,kind,...args);n.setProperty('id',4);if(mode==='factory-rebind')action.childNodes=[late];return n;}return create.call(this,kind,...args);};
return{f,n:action,rs,late};}
function fields(n){return{tex:n.texClass,prev:n.prevClass,level:n.prevLevel};}
function observe(s,op,options,args){let error='',value;active=true;try{if(op==='selected')value=id(s.n.selected);else if(op==='isEmbellished'||op==='isSpacelike')value=s.n[op];else if(op==='core'||op==='coreMO')value=id(s.n[op]());else if(op==='class')value=id(s.n.setTeXclass(s.late));else if(op==='next')s.n.nextToggleSelection();else if(op==='verify')s.n.verifyAttributes(options);else if(op==='hook')s.n.setChildInheritedAttributes(...args);}catch(e){error=e.name;}active=false;return{error,value,kind:s.n.kind,arity:s.n.arity,children:s.n.childNodes?.map(id)??null,attributes:s.n.slot.getAllAttributes(),inherited:s.n.slot.getAllInherited(),fields:fields(s.n),rs:s.rs.map(fields),calls,events,attrsReads,selectedReads,raw};}
let index=0;
function add(op,type,selection,shape='full',scenario='',options={},args=[{mathcolor:['math','blue']},'raw-display',2.5,'raw-prime'],explicit=true){const s=setup(shape,scenario,type,selection,explicit);raw=args[0];const input=structuredClone(raw);const wanted=observe(s,op,options,args);start('math_port_maction_'+op+'_'+index++);record(wanted);}
const selections=[undefined,null,false,true,-Infinity,-3,-0,0,.5,1,1.5,2,2.5,3,10,Infinity,NaN,'','2','2.5',' 0x10','3tail','x',[],[2],[1,2],{}, {valueOf:0,toString:null}];
for(const op of ['selected','isEmbellished','isSpacelike','core','coreMO'])for(const selection of selections)add(op,'toggle',selection);
for(const op of ['selected','isEmbellished','isSpacelike','core','coreMO','class','next'])for(const shape of ['empty','short','null-array','null-slot'])for(const scenario of ['','factory-null','factory-throw','factory-rebind','attrs-null','attrs-null-second','attrs-rebind','selected-null','selected-throw','selected-rotate'])add(op,'toggle',shape==='null-slot'?2:NaN,shape,scenario);
for(const type of ['toggle','tooltip','TOOLTIP','ToolTip','statusline','TooltipΣ','İooltip',undefined,null,false,0,[],{},'']){
add('class',type,1);for(const scenario of ['tip-rebind','class-throw','class-return-null','class-return-late','update-throw','attrs-null','attrs-rebind'])add('class',type,1,'full',scenario);
for(const shape of ['full','empty','short','null-array','null-slot'])add('hook',type,2,shape);
for(const explicit of [false,true])for(const options of [{},{checkAttributes:true},{checkMathvariants:true}])add('verify',type,2,'full','',options,undefined,explicit);
}
for(const selection of selections)for(const shape of ['full','empty','null-array'])add('next','toggle',selection,shape);
for(const scenario of ['inherit-first-throw','inherit-second-throw','inherit-rebind','inherit-null-array','inherit-shorten','inherit-mutate-attrs','attrs-null'])for(const type of ['tooltip','TOOLTIP','toggle'])add('hook',type,1,'full',scenario);
for(const rawAttrs of [undefined,null,false,3,'xy',[],{}, {x:['math',null]}])for(const type of ['tooltip','toggle'])add('hook',type,1,'full','',{},[rawAttrs,undefined,null,NaN]);
for(const scenario of ['verify-throw','verify-rebind'])for(const options of [{checkAttributes:true},{checkMathvariants:true},{checkAttributes:true,checkMathvariants:true}]){const s=setup('full',scenario,'tooltip',2);s.n.slot.set('bad-name','x');s.n.slot.set('mathvariant','invalid');raw={};const wanted=observe(s,'verify',options,[]);start('math_port_maction_verify_errors_'+index++);record(wanted);}
const defaults=Object.assign({},MmlMaction.defaults);start('math_port_maction_defaults');record(defaults);assert.notEqual(MmlMaction.defaults,core.AbstractMmlNode.defaults);
function realTree(type,selection,display,level){active=false;const f=new MmlFactory();const token=(kind,text)=>f.create(kind,{},[f.create('text').setText(text)]);const action=f.create('maction',{actiontype:type,selection},[token('mi','X'),token('mo','+'),f.create('mrow',{},[f.create('mspace'),token('mi','β')])]);const math=f.create('math',{display},[f.create('mrow',{},[token('mi','a'),action,token('mo','=')])]);math.setInheritedAttributes({mathcolor:['math','blue']},false,level,true);const inherited=structuredClone(realNode(math));math.verifyTree({checkAttributes:true,checkMathvariants:true});math.setTeXclass(null);return[inherited,realNode(math)];}
function realNode(n){return{kind:n.kind,text:n.kind==='text'?n.getText():null,properties:n.getAllProperties(),attributes:n.attributes?.getAllAttributes()??null,inherited:n.attributes?.getAllInherited()??null,fields:fields(n),children:n.childNodes.map(realNode)};}
for(const type of ['toggle','tooltip','TOOLTIP','statusline'])for(const selection of [-2,1,2,5,'2'])for(const display of ['inline','block'])for(const level of [0,1,2])(start('math_port_maction_real_'+index++),record(realTree(type,selection,display,level)));

finish("source-maction-profile.json");
