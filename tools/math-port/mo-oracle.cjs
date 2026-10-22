// Root independent observations of the actual pinned MmlMo class.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,M}=require('./mo-pattern-reference.cjs'),npm=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const {MmlFactory:F}=require(path.join(npm,'core/MmlTree/MmlFactory.js'));
const D=require(path.join(npm,'core/MmlTree/OperatorDictionary.js'));
class Mo extends M{op(s){return this.getOperatorDef(s);}table(s){this.checkOperatorTable(s);}forms(a){return this.handleExplicitForm(a);}pseudo(s){this.checkPseudoScripts(s);}prime(s){this.checkPrimes(s);}accent(s){this.checkMathAccent(s);}}
class Fork extends Mo{}Fork.OPTABLE={infix:{},prefix:{},postfix:{}};Fork.MMLSPACING=Array.from({length:8},()=>[9,8]);
const f=()=>{const f=new F();f.setNodeClass('mo',Mo);f.setNodeClass('fork',Fork);return f;};
const token=(f,k,text,attrs={})=>f.create(k,attrs,[f.create('text').setText(text)]);
const sourceProfile=[];let assertions=0,cases=0;
function value(x){if(x===undefined)return {type:'missing'};if(x===null)return {type:'null'};if(typeof x==='boolean')return {type:'boolean',value:x};if(typeof x==='number')return {type:'number',value:Number.isNaN(x)?'NaN':Object.is(x,-0)?'-0':x===Infinity?'Infinity':x};if(typeof x==='string')return {type:'string',units:Array.from({length:x.length},(_,i)=>x.charCodeAt(i))};if(Array.isArray(x))return {type:'array',items:x.map(value)};if(typeof x==='object')return {type:'object',entries:Object.keys(x).map(k=>[k,value(x[k])])};throw Error('Unbound observation');}
const p=value;
function eq(x){assertions++;sourceProfile.push(['eq',value(x)]);}function pp(x){assertions++;sourceProfile.push(['property',value(x)]);}function begin(n){cases++;sourceProfile.push(['case',n]);}function end(){}
function state(n){for(const key of ['form','fence','separator','lspace','rspace','accent','stretchy','movablelimits'])pp(n.attributes.get(key));for(const key of ['noDictDef','pseudoscript','primes','mathaccent','texClass'])pp(n.getProperty(key));pp(n._texClass);pp(n.lspace);pp(n.rspace);}
function attempt(fn){let error;try{fn();}catch(e){error=e;}sourceProfile.push(['attempt',error?error.name:'ok']);if(error)assertions++;return error;}
begin('source_defaults_aliases_and_initialization');
;const init=token(f(),'mo','+');pp(init._texClass);pp(init.lspace);pp(init.rspace);eq(init.kind);eq(init.isEmbellished);
for(const key of Object.keys(M.defaults))pp(M.defaults[key]);
assert.equal(M.OPTABLE,D.OPTABLE);assert.equal(M.MMLSPACING,D.MMLSPACING);eq(true);eq(true);
;pp(M.defaults.form);;end();
begin('dictionary_all_initial_entries');
const attrKeys=['form','fence','separator','lspace','rspace','accent','stretchy','movablelimits'],propKeys=['noDictDef','pseudoscript','primes','mathaccent','texClass'];
const states=[],stateIds=new Map(),rows=[];
for(const form of ['infix','prefix','postfix'])for(const key of Object.keys(D.OPTABLE[form])){
 const n=token(f(),'mo',key,{form});n.table(key);
 const values=[...attrKeys.map(k=>n.attributes.get(k)),...propKeys.map(k=>n.getProperty(k)),n._texClass,n.lspace,n.rspace];
 const serialized=JSON.stringify(values.map(p));let id=stateIds.get(serialized);if(id===undefined){id=states.length;stateIds.set(serialized,id);states.push(values);}
 const extra=Object.keys(D.OPTABLE[form][key][3]||{}).filter(k=>!attrKeys.includes(k)).map(k=>[k,n.attributes.get(k)]);
 assertions+=values.length+extra.length;rows.push({key,form,id,extra});
}
;
;
for(const r of rows);
;
for(const k of attrKeys);for(const k of propKeys);
;end();
begin('prototype_hits_and_misses');;
for(const text of ['', 'a','ab','x1','😀','+','toString','constructor','__proto__','hasOwnProperty','valueOf','unknown!'])for(const form of ['infix','prefix','postfix']){
 const n=token(f(),'mo',text,{form});;attempt(()=>n.table(text));state(n);;
}end();
begin('raw_field_getter_and_lazy_writes');;
for(const value of [undefined,null,0,-0,1,3,-1,false,true,'','0',NaN]){const n=token(f(),'mo','ab');n._texClass=value;;pp(n.texClass);pp(n._texClass);pp(n.attributes.getInherited('form'));pp(n.getProperty('noDictDef'));;}end();
begin('positional_forms_and_explicit_filter_identity');;
for(const count of [1,2,3])for(let index=0;index<count;index++){const sf=f(),nodes=Array.from({length:count},(_,i)=>token(sf,i===index?'mo':'mi',i===index?'+':'x')),n=nodes[index];sf.create('mrow',{},nodes);;for(let i=0;i<3;i++)pp(n.getForms()[i]);eq(false);;}end();
begin('class_adjustment_matrix');;
const cls=[undefined,null,0,1,2,3,4,5,6,7,-1,false,'0'];
for(const own of cls)for(const prev of cls)for(const auto of [false,true]){const sf=f(),n=token(sf,'mo','+'),a=token(sf,'mi','a'),last=token(sf,'mi','z');sf.create('mrow',{},[a,n,last]);n._texClass=own;a.texClass=prev;a.setProperty('autoOP',auto);n.attributes.setInherited('scriptlevel','2');let result,error;try{result=n.adjustTeXclass(a);}catch(e){error=e;};if(error)attempt(()=>{throw error;});else eq(result===n);pp(n._texClass);pp(n.prevClass);pp(n.prevLevel);pp(a.texClass);;}end();
begin('fence_spacing_and_binary_last');;
for(const form of ['prefix','infix','postfix'])for(const spacing of [undefined,0,'0em']){const sf=f(),attrs={form,fence:true};if(spacing!==undefined)attrs.lspace=spacing;const n=token(sf,'mo','⫿',attrs);n._texClass=3;const row=sf.create('mrow',{},[n]);let result;n.setProperty('texClass',undefined);result=n.setTeXclass(null);;eq(result===n);pp(n._texClass);pp(n.prevClass);;}
{const sf=f(),n=token(sf,'mo','+');sf.create('mrow',{},[n]);n._texClass=2;n.adjustTeXclass(null);;pp(n._texClass);;}end();
begin('primes_pseudoscripts_and_inheritance');;
for(const text of ["'",'"',"''",'‘’„‟','*','ª','x','^','→','\n','\u2032'])for(const parent of ['none','msub','msup','msubsup']){const sf=f(),n=token(sf,'mo',text);if(parent!=='none'){const b=token(sf,'mi','b'),other=token(sf,'mi','s');sf.create(parent,{},parent==='msubsup'?[b,other,n]:[b,n]);}n.pseudo(text);n.prime(text);;state(n);;}
for(const text of ['+','ab',"'",'→','^']){const n=token(f(),'mo',text);n.setInheritedAttributes({},false,0,false);;state(n);;}end();
begin('accent_source_quirks');;
for(const kind of ['munder','mover','munderover'])for(const pos of ['base','under','over'])for(const value of [undefined,false,true,'yes',0]){const sf=f(),n=token(sf,'mo','^'),b=token(sf,'mi','b'),other=token(sf,'mi','o');let list;if(kind==='munder')list=pos==='base'?[n,other]:[b,n];else if(kind==='mover')list=pos==='base'?[n,other]:[b,n];else list=pos==='base'?[n,other,other]:pos==='under'?[b,n,other]:[b,other,n];const attrs={};if(value!==undefined)attrs[kind==='munder'?'accentunder':'accent']=value;sf.create(kind,attrs,list);n.attributes.set('accent','raw');let result,error;try{result=n.isAccent;}catch(e){error=e;}n.accent('^');;if(error)attempt(()=>{throw error;});else pp(result);;pp(n.getProperty('mathaccent'));;}end();
begin('constructor_tables_distinct_from_module');;
for(const text of ['+','ab','!','😀']){const n=token(f(),'fork',text);const def=n.op(text);;for(let i=0;i<3;i++)pp(def[i]);eq(true);eq(false);;}end();
begin('accent_optional_text_and_core_traversal');;
for(const text of ['^','→','x','',"'",'\u0301']){const sf=f(),n=token(sf,'mo',text);;eq(n.isMathAccent());eq(n.isMathAccentWithWidth());eq(n.isMathAccent(''));eq(n.isMathAccentWithWidth(''));;}
{const sf=f(),n=token(sf,'mo','+'),x=token(sf,'mi','x'),style=sf.create('mstyle',{},[n]),math=sf.create('math',{},[style]);assert.ok(n.coreParent()===math.childNodes[0]);;eq(true);eq(n.coreText(null));eq(n.coreText(style));eq(n.coreText(x));eq(n.coreText(math));;}end();
begin('orphan_binary_error_and_width_accent');;
{const sf=f(),n=token(sf,'mo','+'),a=token(sf,'mi','a');n._texClass=2;;attempt(()=>n.adjustTeXclass(a));pp(n.prevClass);pp(n.prevLevel);;}
for(const text of ['^','→','x']){const sf=f(),n=token(sf,'mo',text),b=token(sf,'mi','b'),over=token(sf,'mi','o');sf.create('munderover',{},[b,n,over]);n.accent(text);;pp(n.getProperty('mathaccent'));;}end();
begin('explicit_forms_raw_filter_and_alias');;
for(const form of [undefined,null,'infix','prefix','weird',0,false]){const sf=f(),n=token(sf,'mo','+'),a=['infix','prefix','postfix','infix'];if(form!==undefined)n.attributes.set('form',form);const result=n.forms(a);;eq(result===a);eq(result.length);for(let i=0;i<result.length;i++)pp(result[i]);;}end();
begin('lookup_hit_alias_and_constructor_capture');;
{const sf=f(),n=token(sf,'mo','+',{form:'infix'}),def=n.op('+');assert.ok(def===D.OPTABLE.infix['+']);;eq(true);;}
{const sf=f(),n=token(sf,'fork','+');sf.setNodeClass('fork',Mo);const def=n.op('+');;for(let i=0;i<3;i++)pp(def[i]);;}end();

const profile=({assertions,cases,sourceProfile,states:states.map(s=>s.map(value)),rows:rows.map(r=>[r.key,r.form,r.id,r.extra.map(([k,v])=>[k,value(v)])])});const frozen=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p24-mo-source.json'),'utf8'));assert.deepEqual(profile,frozen);assert.ok(process.argv.includes('--check'),'Source-only owner supports --check');console.log(JSON.stringify({profile:'p24-mo-source.json',cases:profile.cases?.length??profile.cases,assertions:profile.assertions??null,mode:'check'}));
