// Root independent order, identity, and failure observations from pinned CJS.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,M}=require('./mo-pattern-reference.cjs');
const base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js'));
const D=require(path.join(base,'core/MmlTree/OperatorDictionary.js'));
const {MmlMi}=require(path.join(base,'core/MmlTree/MmlNodes/mi.js'));
const {AbstractMmlNode}=require(path.join(base,'core/MmlTree/MmlNode.js'));
let log=[],enabled=false;
const note=s=>{if(enabled)log.push(s);};
class Probe extends M {
 get Parent(){note('Parent');return super.Parent;}
 get attributes(){note('attributes');return this.savedAttributes;}
 set attributes(v){this.savedAttributes=v;}
 getText(){note('text');return super.getText();}
 getProperty(k){note('get/'+k);return super.getProperty(k);}
 setProperty(k,v){note('set/'+k);return super.setProperty(k,v);}
 get texClass(){note('tex.read');return super.texClass;}
 set texClass(v){note('tex.write');super.texClass=v;}
 getForms(){note('forms');return super.getForms();}
 handleExplicitForm(a){note('explicit');return super.handleExplicitForm(a);}
 static get OPTABLE(){note('static.table');return M.OPTABLE;}
 static get MMLSPACING(){note('static.spacing');return M.MMLSPACING;}
 static get opPattern(){note('static.op');return M.opPattern;}
 static get pseudoScripts(){note('static.pseudo');return M.pseudoScripts;}
 static get primes(){note('static.primes');return M.primes;}
 static get remapPrimes(){note('static.remap');return M.remapPrimes;}
 static get mathaccents(){note('static.accent');return M.mathaccents;}
 static get mathaccentsWithWidth(){note('static.width');return M.mathaccentsWithWidth;}
 op(s){return super.getOperatorDef(s);} table(s){return super.checkOperatorTable(s);}
 forms(a){return super.handleExplicitForm(a);} accent(s){return super.checkMathAccent(s);}
 pseudo(s){return super.checkPseudoScripts(s);} prime(s){return super.checkPrimes(s);}
}
class Dispatch extends Probe {
 checkOperatorTable(s){note('table/'+s);}
 checkPseudoScripts(s){note('pseudo/'+s);}
 checkPrimes(s){note('prime/'+s);}
 checkMathAccent(s){note('accent/'+s);}
}
class NullCore extends Probe {coreParent(){return null;}}
class Phase extends Probe {
 setChildren(c){note('phase/'+[this._texClass,this.lspace,this.rspace].map(v=>v===undefined?'missing':String(v)).join('/'));return super.setChildren(c);}
}
let changingDef=null,attributeReads=0;
class AttrMutation extends Probe {
 get attributes(){const a=super.attributes;if(enabled&&++attributeReads===3)changingDef[3]={a:9,b:8};return a;}
 set attributes(v){super.attributes=v;}
}
class Snapshot extends MmlMi {get isEmbellished(){if(enabled){note('snapshot');this.parent.childNodes[1]=this;this.parent.childNodes[2]=this;}return false;}}
class Atom extends AbstractMmlNode {get kind(){return 'TeXAtom';}}
let current=null;
class Previous extends Probe {get texClass(){note('prev.tex.read');return super.texClass;}set texClass(v){note('prev.tex.write/'+String(current.prevClass));super.texClass=v;}}
function factory(){const f=new MmlFactory();for(const [k,c]of Object.entries({probe:Probe,dispatch:Dispatch,nullcore:NullCore,phase:Phase,mutation:AttrMutation,snapshot:Snapshot}))f.setNodeClass(k,c);return f;}
function token(f,k,s,a={}){return f.create(k,a,[f.create('text').setText(s)]);}
let assertions=0,cases=[],currentCase;function value(x){if(x===undefined)return {type:'missing'};if(x===null)return {type:'null'};if(typeof x==='boolean')return {type:'boolean',value:x};if(typeof x==='number')return {type:'number',value:Number.isNaN(x)?'NaN':Object.is(x,-0)?'-0':x===Infinity?'Infinity':x};if(typeof x==='string')return {type:'string',units:Array.from({length:x.length},(_,i)=>x.charCodeAt(i))};if(Array.isArray(x))return {type:'array',items:x.map(value)};if(typeof x==='object')return {type:'object',entries:Object.keys(x).map(k=>[k,value(x[k])])};throw Error('Unbound observation');}

function begin(n){enabled=false;log=[];currentCase={id:'math_port_mo_order_'+n,observations:[]};cases.push(currentCase);}function end(){enabled=false;}
function observe(o){currentCase.observations.push(o);assertions++;}
function trace(fn){log=[];enabled=true;let error;try{fn();}catch(e){error=e;}enabled=false;if(error)observe({kind:'error',name:error.name});observe({kind:'trace',value:[...log]});}
function expectedBool(v){observe({kind:'scalar',expected:value(v)});}function slot(v){observe({kind:'value',expected:value(v)});}

for(const text of ['+','ab','!','😀']){begin('lookup_trace_'+Buffer.from(text).toString('hex'));const f=factory(),n=token(f,'probe',text);;trace(()=>n.table(text));end();}
begin('inherited_gettext_once_dispatch');{const f=factory(),n=token(f,'dispatch','x');;trace(()=>n.setInheritedAttributes({},false,2,true));end();}
begin('phase_missing_before_initializers');{const f=factory();;trace(()=>token(f,'phase','+'));end();}
begin('parent_read_before_predefined_accent');{const f=factory(),n=token(f,'probe','^');n.setProperty('mathaccent',false);;trace(()=>n.accent('^'));end();}
for(const op of ['pseudo','isAccent']){begin('null_core_'+op);const f=factory(),n=token(f,'nullcore',"'");;trace(()=>op==='pseudo'?n.pseudo("'"):n.isAccent);end();}
begin('remap_missing_range_error');{const old=M.remapPrimes;M.remapPrimes={};const f=factory(),n=token(f,'probe',"'");;trace(()=>n.prime("'"));slot(n.getProperty('primes'));M.remapPrimes=old;;end();}
begin('sparse_forms_keep_inherited_slots');{const f=factory(),n=token(f,'probe','+',{form:'infix'}),a=['infix',,'postfix',,'prefix'];Object.setPrototypeOf(a,Object.assign(Object.create(Array.prototype),{1:'prefix'}));const b=n.forms(a);;expectedBool(b.length);b.forEach((v,i)=>slot(v));end();}
begin('module_rebind_is_separate_from_class_alias');{const original=D.OPTABLE,oldSpacing=D.MMLSPACING;D.OPTABLE={infix:{},prefix:{},postfix:{}};D.MMLSPACING=[];const f=factory(),n=token(f,'probe','+'),r=n.op('+');;expectedBool(r===original.infix['+']);D.OPTABLE=original;D.MMLSPACING=oldSpacing;;end();}
begin('class_table_live_rebinding_after_construction');{const f=factory(),n=token(f,'probe','+'),old=M.OPTABLE;M.OPTABLE={infix:{'+':[7,8,6,{accent:'custom'}]},prefix:{},postfix:{}};n.table('+');;slot(n._texClass);slot(n.attributes.get('accent'));M.OPTABLE=old;;end();}
begin('live_pattern_and_remap_after_construction');{const f=factory(),n=token(f,'probe','x'),old=M.primes,remap=M.remapPrimes;M.primes=/^x$/;M.remapPrimes={120:0x1f600};n.prime('x');;slot(n.getProperty('primes'));M.primes=old;M.remapPrimes=remap;;end();}
begin('attribute_receiver_before_definition_slot');{const f=factory(),n=token(f,'mutation','+'),old=M.OPTABLE;changingDef=[1,2,3,{a:1,b:2}];M.OPTABLE={infix:{'+':changingDef},prefix:{},postfix:{}};attributeReads=0;;trace(()=>n.table('+'));slot(n.attributes.getInherited('a'));slot(n.attributes.getInherited('b'));M.OPTABLE=old;;end();}
begin('accent_children_snapshot_before_virtual_base');{const f=factory(),n=token(f,'probe','^'),baseNode=token(f,'snapshot','b'),over=token(f,'probe','^');f.create('munderover',{},[baseNode,n,over]);;trace(()=>n.accent('^'));slot(n.getProperty('mathaccent'));end();}
begin('coreparent_actual_registered_instanceof');{const f=factory(),n=token(f,'probe','+');f.create('mrow',{},[n]);f.setNodeClass('math',Probe);;expectedBool(n.coreParent()===n);end();}
begin('replacement_pattern_survives_its_own_rebinding');{const f=factory(),n=token(f,'probe','x'),old=M.primes,remap=M.remapPrimes;M.primes={[Symbol.match](s){M.primes=/a^/;M.remapPrimes={120:0x2032};return s==='x'?[]:null;}};;trace(()=>n.prime('x'));slot(n.getProperty('primes'));M.primes=old;M.remapPrimes=remap;;end();}
for(const text of ['ab','x']){begin('lookup_limits_shortcircuit_'+text);const f=factory(),n=token(f,'probe',text,{movablelimits:'raw'});n.setProperty('texClass',false);;trace(()=>n.op(text));end();}
for(const op of ['table','pseudo']){begin('null_attributes_failure_'+op);const f=factory(),n=token(f,'probe',"'");n.attributes=null;;trace(()=>op==='table'?n.table("'"):n.pseudo("'"));slot(n.getProperty('pseudoscript'));end();}
begin('string_spacing_iterator');{const f=factory(),n=token(f,'probe','!'),old=M.OPTABLE,spacing=M.MMLSPACING;M.OPTABLE={infix:{},prefix:{},postfix:{}};M.MMLSPACING=Array.from({length:8},()=> '😀x');const r=n.op('!');;for(let i=0;i<3;i++)slot(r[i]);M.OPTABLE=old;M.MMLSPACING=spacing;;end();}
for(const shape of ['empty','null-array','null-base','missing-under-embellished-over']){begin('malformed_accent_'+shape);const f=factory(),n=token(f,'probe','^'),b=token(f,'mi','b'),over=token(f,'probe','^'),p=f.create('munderover',{},[b,n,over]);p.childNodes=shape==='empty'?[]:shape==='null-array'?null:shape==='null-base'?[null,n,over]:[b,undefined,over];n.parent=p;;trace(()=>n.accent('^'));slot(n.getProperty('mathaccent'));end();}
for(const kind of ['mover','munder','munderover']){begin('accent_missing_script_'+kind);const f=factory(),n=token(f,'probe','^'),b=token(f,'mi','b'),p=f.create(kind,{},kind==='munderover'?[b,n,n]:[b,n]);p.childNodes=[b];n.parent=p;;trace(()=>n.isAccent);end();}
for(const value of [-1,0,1,2,3,4,5,6,7]){begin('adjust_null_prev_'+value);const f=factory(),n=token(f,'probe','+');n._texClass=value;;trace(()=>n.adjustTeXclass(null));slot(n._texClass);slot(n.prevClass);end();}
begin('function_prototype_form_hits_and_poison_accessors');;
for(const form of ['constructor','toString','valueOf','__proto__'])for(const text of ['caller','arguments','__proto__','call','apply','bind','toString','constructor','name','length']){const f=factory(),n=token(f,'probe',text,{form});;trace(()=>n.table(text));slot(n._texClass);slot(n.getProperty('noDictDef'));;}end();
for(const shape of ['nested-singletons','multiple-row','under-over','atom']){begin('coretext_non_embellished_'+shape);const f=factory(),n=token(f,'probe','+'),x=token(f,'mi','x'),y=token(f,'mi','y');let node=x;;if(shape==='nested-singletons'){for(const kind of ['mphantom','mstyle','mrow']){node=f.create(kind,{},[node]);;}}else if(shape==='multiple-row'){node=f.create('mrow',{},[x,y]);;}else if(shape==='under-over'){node=f.create('munderover',{},[x,y,y]);;}else{f.setNodeClass('TeXAtom',Atom);node=f.create('TeXAtom',{},[x]);;};slot(n.coreText(node));end();}
for(const shape of ['row-last','style-chain-last','script-chain-last','style-chain-not-last']){begin('binary_parent_walk_'+shape);const f=factory(),n=token(f,'probe','+'),prev=token(f,'mi','p'),other=token(f,'mi','q');n._texClass=2;prev.texClass=0;let node=n;if(shape.startsWith('style')){node=f.create('mstyle',{},[n]);;}if(shape==='script-chain-last'){node=f.create('munderover',{},[n,other,other]);;}const root=f.create('mrow',{},shape.endsWith('not-last')?[node,other]:[prev,node]);;;trace(()=>n.adjustTeXclass(prev));slot(n._texClass);end();}
begin('binary_previous_assignment_right_to_left');{const f=factory();f.setNodeClass('previous',Previous);const n=token(f,'probe','='),prev=token(f,'previous','+');current=n;n._texClass=3;prev._texClass=2;;trace(()=>n.adjustTeXclass(prev));current=null;;end();}
begin('intrinsic_function_identity_metadata');{
 const identities=[['constructor',Object],['defineGetter',Object.prototype.__defineGetter__],['defineSetter',Object.prototype.__defineSetter__],['hasOwnProperty',Object.prototype.hasOwnProperty],['lookupGetter',Object.prototype.__lookupGetter__],['lookupSetter',Object.prototype.__lookupSetter__],['isPrototypeOf',Object.prototype.isPrototypeOf],['propertyIsEnumerable',Object.prototype.propertyIsEnumerable],['toString',Object.prototype.toString],['valueOf',Object.prototype.valueOf],['toLocaleString',Object.prototype.toLocaleString],['arrayConstructor',Array],['arrayToString',Array.prototype.toString],['arrayJoin',Array.prototype.join],['numberConstructor',Number],['numberToString',Number.prototype.toString],['numberValueOf',Number.prototype.valueOf],['functionPrototype',Function.prototype],['functionConstructor',Function],['functionCall',Function.prototype.call],['functionApply',Function.prototype.apply],['functionBind',Function.prototype.bind],['functionToString',Function.prototype.toString],['prototype',Object.prototype],['arrayPrototype',Array.prototype],['numberPrototype',Number.prototype]];
 const names=new Map(identities.map(([n,v])=>[v,n]));
 for(const [name,item]of identities){slot(String(item));for(const key of ['__proto__','constructor','call','apply','bind','toString','prototype','name','length','caller','arguments']){let got,error;try{got=item[key];}catch(e){error=e;}if(error)observe({kind:'error',name:error.name});else if(names.has(got))observe({kind:'identity',name:names.get(got)});else slot(got);}}
}end();

const profile=({cases,assertions});const frozen=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p24-mo-order-source.json'),'utf8'));assert.deepEqual(profile,frozen);assert.ok(process.argv.includes('--check'),'Source-only owner supports --check');console.log(JSON.stringify({profile:'p24-mo-order-source.json',cases:profile.cases?.length??profile.cases,assertions:profile.assertions??null,mode:'check'}));
