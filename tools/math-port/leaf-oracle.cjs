// Root reference generator: actual locked concrete classes, never native output.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),npm=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const lock=require(path.join(root,'docs/math-port/evidence/p9-source-lock.json'));
const sha=b=>crypto.createHash('sha256').update(b).digest('hex'),modules={};
for(const f of lock.files){const src=fs.readFileSync(path.join(root,'.reference/MathJax-src',f.source));assert.equal(sha(src),f.sha256);assert.equal(fs.readFileSync(path.join(npm,f.source),'utf8').replaceAll('\r\n','\n'),src.toString().replaceAll('\r\n','\n'));
 const resolved=require.resolve('@mathjax/src/js/'+f.source.slice(3,-3)+'.js',{paths:[path.join(root,'tools/mathjax-bundle')]});assert.equal(fs.realpathSync(resolved),fs.realpathSync(path.join(npm,f.runtime)));assert.equal(sha(fs.readFileSync(resolved)),f.runtime_sha256);modules[path.basename(f.source,'.ts')]=require(resolved);}
const N=require(path.join(npm,'cjs/core/MmlTree/MmlNode.js')),NF=require(path.join(npm,'cjs/core/Tree/NodeFactory.js')).AbstractNodeFactory;
const entries=[];for(const f of lock.files)for(const [name,C]of Object.entries(modules[path.basename(f.source,'.ts')]))entries.push([C.prototype.kind,C,name]);
class Probe extends N.AbstractMmlNode{get kind(){return 'probe';}get isSpacelike(){return !!this.getProperty('space');}get isEmbellished(){return !!this.getProperty('embellished');}setTeXclass(p){if(this.onClass)this.onClass(p);return super.setTeXclass(p);}setInheritedAttributes(a,d,l,p){if(this.onInherit)this.onInherit(a,d,l,p);return super.setInheritedAttributes(a,d,l,p);}}
const classes=()=>Object.fromEntries([...entries.map(([kind,C])=>[kind,C]),['text',N.TextNode],['XML',N.XMLNode],['probe',Probe]]);

const sourceProfile=[],directProfile=[];let currentCase='';let assertions=0,cases=0;
function encode(v){if(v===undefined)return {$type:'undefined'};if(typeof v==='number'){if(Number.isNaN(v))return {$type:'number',value:'NaN'};if(Object.is(v,-0))return {$type:'number',value:'-0'};if(!Number.isFinite(v))return {$type:'number',value:String(v)};}if(v&&typeof v==='object'){const out=Array.isArray(v)?new Array(v.length):{};for(const k of Object.keys(v))Object.defineProperty(out,k,{value:encode(v[k]),enumerable:true,writable:true,configurable:true});return out;}return v;}



function eq(v){assertions++;sourceProfile.push(['eq',encode(v)]);}
function pp(v){assertions++;sourceProfile.push(['property',encode(v)]);}
function list(v){assertions++;sourceProfile.push(['list',v]);}
function throws(fn){let error;try{fn();}catch(x){error=x;}assert.ok(error,'Reference must throw');sourceProfile.push(['exception',error.name]);assertions+=2;}
function begin(name){cases++;sourceProfile.push(['case',name]);currentCase=name;return new NF(classes());}function end(){}
function text(f,s){return f.create('text').setText(s);}function input(f,k,s,a={}){return f.create(k,a,[text(f,s)]);}
{
 const f=begin('defaults_traits_and_constructor_fields');
 for(const [k,C,name]of entries){const n=f.create(k);eq(n.kind);eq(n.isToken);eq(n.isInferred);eq(!!n.notParent);eq(n.arity);eq(n.linebreakContainer);eq(n.linebreakAlign);eq(n.isEmpty);eq(n.isSpacelike);pp(n.texClass);list(Object.keys(C.defaults));for(const key of Object.keys(C.defaults))pp(C.defaults[key]);eq(n.childNodes.length);if(n.arity<0)eq(n.childNodes[0].kind);}end();
}
{
 begin('utf16_pattern_full_unit_domain');
 function bitString(fn){let out='';for(let i=0;i<65536;i+=4){let b=0;for(let j=0;j<4;j++)if(fn(String.fromCharCode(i+j)))b|=1<<j;out+=b.toString(16);}return out;}
 const patterns=[['single',c=>modules.mi.MmlMi.singleCharacter.test(c)],['combining',c=>modules.mi.MmlMi.singleCharacter.test('a'+c)],['high_prefix',c=>modules.mi.MmlMi.singleCharacter.test('\ud800'+c)],['operator',c=>modules.mi.MmlMi.operatorName.test(c)],['space',c=>/^\s*$/.test(c)]];
 for(const [name,fn]of patterns)eq(bitString(fn));end();
}
{
 const f=begin('row_core_null_persistence_and_queries'),row=f.create('mrow'),a=f.create('probe'),b=f.create('probe'),space=f.create('mspace');a.setProperty('embellished',true);b.setProperty('embellished',true);row.setChildren([space,a]);row.childNodes.unshift(null);

 // Source setChildren null throws after push; bind the intended host state directly.
 eq(row.isEmbellished);eq(row.core()===a);eq(row.coreMO()===a);eq(row.nonSpaceLength());eq(row.firstNonSpace()===a);eq(row.lastNonSpace()===a);throws(()=>row.isSpacelike);row.childNodes.push(b);eq(row.isEmbellished);eq(row.core()===row);row.childNodes=[space];eq(row.isEmbellished);eq(row.nonSpaceLength());eq(row.firstNonSpace()===null);eq(row.isSpacelike);end();
}
{
 const f=begin('row_delimiter_nullish_and_class_transfer');for(const value of [undefined,null,false,'',0,'(']){const r=f.create('mrow'),a=f.create('probe'),prev=f.create('probe');a.texClass=2;prev.texClass=3;r.setChildren([a]);r.setProperty('open',value);const result=r.setTeXclass(prev);
 eq(result===r);pp(r.texClass);pp(r.prevClass);pp(a.prevClass);}end();
}
{
 const f=begin('inferred_join_capture_cycle_and_utf16'),r=f.create('inferredMrow'),x=text(f,'a\ud800');r.childNodes=[x,null,r];assertions++;directProfile.push([currentCase,encode(r.toString())]);end();
}
{
 const f=begin('mi_italic_and_auto_operator');for(const word of ['x','sin','abc2','2x','a-b','a\n','😀','a\u0300','a\u1abf','\ud800\ud800']){const n=input(f,'mi',word);n.setInheritedAttributes();const result=n.setTeXclass(null);
 pp(n.attributes.get('mathvariant'));pp(n.attributes.getInherited('mathvariant'));pp(n.getProperty('autoOP'));pp(n.texClass);eq(result===n);}
 const n=input(f,'mi','x',{mathvariant:'bold'});n.setInheritedAttributes();pp(n.attributes.get('mathvariant'));pp(n.attributes.getInherited('mathvariant'));end();
}
{
 const f=begin('space_explicit_presence_width_and_newlines');for(const value of ['', '-1em',' \ufeff-0', '-0',0,-0,-1,undefined,null,false,'\u0085-1','\u2028-1']){const n=f.create('mspace',{width:value});eq(n.canBreak);eq(n.isSpacelike);eq(n.hasNewline);}for(const attr of ['height','depth','style','mathbackground','background','linebreak'])for(const value of [null,false,0,'']){const n=f.create('mspace',{[attr]:value});eq(n.canBreak);eq(n.isSpacelike);}for(const value of ['newline','indentingnewline','goodbreak',true,null]){const n=f.create('mspace',{linebreak:value});eq(n.hasNewline);}end();
}
{
 const f=begin('mtext_spaces_and_presence');for(const word of ['', ' ','\ufeff\u2028','\u0085','x','😀']){const n=input(f,'mtext',word);eq(n.isSpacelike);}for(const attr of modules.mtext.MmlMtext.NONSPACELIKE){const n=input(f,'mtext',' ',{[attr]:false});eq(n.isSpacelike);}end();
}
{
 const f=begin('mstyle_raw_relative_levels_and_prime');for(const incoming of [2,'2',null,undefined])for(const value of ['+2',' -0x2','3x','0x10',false,null]){const spy=f.create('probe'),n=f.create('mstyle',{scriptlevel:value},[spy]);let observed;spy.onInherit=(a,d,l,p)=>observed=[d,l,p];n.setInheritedAttributes({},true,incoming,true);
 pp(observed[0]);pp(observed[1]);pp(observed[2]);eq(!!n.notParent);}end();
}
{
 const f=begin('mstyle_display_and_cramped');for(const value of [true,false,'true',1,null])for(const cramped of [undefined,false,'yes']){const spy=f.create('probe'),a={displaystyle:value};if(cramped!==undefined)a['data-cramped']=cramped;const n=f.create('mstyle',a,[spy]);let observed;spy.onInherit=(a,d,l,p)=>observed=[d,l,p];n.setInheritedAttributes({},false,4,true);
 pp(observed[0]);pp(observed[1]);pp(observed[2]);}end();
}
{
 const f=begin('math_display_mode_and_explicit_precedence');for(const displaystyle of [undefined,false,true,'yes',0])for(const display of ['inline','block']){const spy=f.create('probe'),a={display};if(displaystyle!==undefined)a.displaystyle=displaystyle;const n=f.create('math',a,[spy]);let observed;spy.onInherit=(a,d,l,p)=>observed=[d,l,p];n.setInheritedAttributes({},false,2,'prime');
 pp(observed[0]);pp(observed[1]);pp(observed[2]);pp(n.attributes.getInherited('displaystyle'));}end();
}
{
 const f=begin('math_constructor_defaults_survive_registry_rebind'),C=modules.math.MmlMath,n=f.create('math'),spy=f.create('probe');n.childNodes[0].appendChild(spy);const old=C.defaults.scriptlevel;C.defaults.scriptlevel=7;class Other extends C{static defaults={...C.defaults,scriptlevel:11};}f.setNodeClass('math',Other);let level;spy.onInherit=(a,d,l)=>level=l;n.setInheritedAttributes();
 pp(level);C.defaults.scriptlevel=old;end();
}
{
 const f=begin('math_nested_null_options_failure_after_allocation'),parent=f.create('probe'),n=f.create('math');parent.appendChild(n);let error;class Recorded extends modules.merror.MmlMerror{constructor(...args){super(...args);error=this;}}f.setNodeClass('merror',Recorded);
 throws(()=>n.verifyTree());eq(parent.childNodes[0]===n);eq(error!==undefined);pp(error.attributes.getExplicit('data-mjx-message'));eq(error.childNodes.length);eq(error.childNodes[0].childNodes.length);end();
}
{
 const f=begin('source_field_phase_after_virtual_super_hooks'),trace=[];let sets=0;
 class Trace extends modules.mi.MmlMi{setChildren(c){trace.push(this.texClass);return super.setChildren(c);}set texClass(v){sets++;super.texClass=v;}get texClass(){return super.texClass;}}
 f.setNodeClass('mi',Trace);const n=f.create('mi');
 eq(trace.length);trace.forEach((v,i)=>pp(v));eq(sets);pp(n.texClass);end();
}
{
 const f=begin('row_last_search_reads_replaced_array');
 class SpaceProbe extends Probe{get isSpacelike(){if(this.onSpace)this.onSpace();return super.isSpacelike;}}
 f.setNodeClass('probe',SpaceProbe);const r=f.create('mrow'),a=f.create('probe'),b=f.create('probe'),replacement=f.create('probe');a.setProperty('space',true);b.setProperty('space',true);r.childNodes=[a,b];b.onSpace=()=>{r.childNodes=[replacement];};
 eq(r.lastNonSpace()===replacement);end();
}
{
 const f=begin('row_core_uses_current_array_after_predicate');
 class Flicker extends Probe{get isEmbellished(){if(this.callback)this.callback();return super.isEmbellished;}}
 f.setNodeClass('probe',Flicker);const r=f.create('mrow'),a=f.create('probe'),replacement=f.create('probe');a.setProperty('embellished',true);r.childNodes=[a];a.callback=()=>{r.childNodes=[replacement];};
 eq(r.core()===replacement);r.childNodes=[a];a.callback=()=>{r.childNodes=[null];};throws(()=>r.coreMO());end();
}
{
 const f=begin('row_live_class_iteration_and_current_first_child'),r=f.create('mrow'),a=f.create('probe'),late=f.create('probe'),replacement=f.create('probe');a.texClass=2;late.texClass=3;replacement.texClass=1;const original=[a];r.childNodes=original;a.onClass=()=>{original.push(late);r.childNodes=[replacement];};const result=r.setTeXclass(null);
 eq(result===late);pp(r.texClass);eq(r.childNodes[0]===replacement);end();
}
{
 const f=begin('mi_tuple_and_property_presence');for(const key of ['autoOP','texClass'])for(const value of [null,false,0,'']){const n=input(f,'mi','sin');n.setProperty(key,value);n.setTeXclass(null);pp(n.texClass);pp(n.getProperty('autoOP'));}
 const n=input(f,'mi','x');n.setInheritedAttributes({mathvariant:['mstyle','']});pp(n.attributes.get('mathvariant'));end();
}
{
 const f=begin('math_mode_display_is_inherited_not_explicit');for(const explicit of [false,true]){const a={mode:'display'};if(explicit)a.display='inline';const spy=f.create('probe'),n=f.create('math',a,[spy]);let level;spy.onInherit=(a,d)=>level=d;n.setInheritedAttributes();pp(level);pp(n.attributes.getInherited('display'));pp(n.attributes.get('display'));}end();
}
{
 const f=begin('mi_operator_assignment_uses_virtual_setter');let sets=0;
 class Observe extends modules.mi.MmlMi{get texClass(){return super.texClass;}set texClass(v){sets++;super.texClass=v;}}
 f.setNodeClass('mi',Observe);const n=input(f,'mi','sin');n.setTeXclass(null);
 eq(sets);pp(n.texClass);end();
}
{
 const f=begin('math_each_attributes_access_is_virtual');
 const A=require(path.join(npm,'cjs/core/MmlTree/Attributes.js')).Attributes;
 class Sequence extends modules.math.MmlMath{get attributes(){return this.sequence?this.sequence[this.cursor++]:this.saved;}set attributes(v){this.saved=v;}run(){this.setChildInheritedAttributes({},false,0,false);}}
 f.setNodeClass('math',Sequence);const n=f.create('math');n.sequence=Array.from({length:7},()=>new A(modules.math.MmlMath.defaults,modules.math.MmlMath.defaults));n.sequence[4].set('display','block');n.cursor=0;n.run();
 eq(n.cursor);pp(n.sequence[5].getInherited('displaystyle'));end();
}
{
 const f=begin('mstyle_defaults_precede_first_write_and_partial_throw');
 class Partial extends modules.mstyle.MmlMstyle{get attributes(){if(this.armed&&++this.calls===2)throw new TypeError('second read');return this.saved;}set attributes(v){this.saved=v;}}
 f.setNodeClass('mstyle',Partial);const n=f.create('mstyle'),saved=n.attributes;n.armed=true;n.calls=0;
 throws(()=>n.setInheritedAttributes({},undefined,undefined,undefined));pp(saved.getInherited('displaystyle'));eq(n.calls);end();
}
{
 const f=begin('inferred_join_snapshots_array_and_length');
 class Trigger extends N.TextNode{toString(){if(this.callback)this.callback();return super.toString();}}
 f.setNodeClass('text',Trigger);const r=f.create('inferredMrow'),x=text(f,'a'),y=text(f,'b'),late=text(f,'c');const original=[x,y];r.childNodes=original;x.callback=()=>{original.push(late);r.childNodes=[];};
 eq(r.toString());eq(r.childNodes.length);end();
}
{
 const f=begin('mstyle_dispatches_only_literal_first_child'),n=f.create('mstyle'),a=f.create('probe'),b=f.create('probe');n.childNodes=[a,b];let calls=[0,0];a.onInherit=()=>calls[0]++;b.onInherit=()=>calls[1]++;n.setInheritedAttributes();
 calls.forEach((v,i)=>eq(v));end();
}
{
 const f=begin('newline_is_suppressed_by_break_constraints');for(const a of [{linebreak:'newline',height:0},{linebreak:'indentingnewline',width:'-1em'}]){const n=f.create('mspace',a);eq(n.hasNewline);}end();
}
{
 const f=begin('static_bindings_copy_alias_and_live_callbacks'),R=modules.mrow.MmlMrow,I=modules.mrow.MmlInferredMrow,Mi=modules.mi.MmlMi;
 eq(R.defaults===I.defaults);R.defaults['p9-own']=1;pp(I.defaults['p9-own']);pp(Mi.defaults['p9-own']);delete R.defaults['p9-own'];
 const old=Mi.singleCharacter;Mi.singleCharacter=/^xyz$/;const n=input(f,'mi','xyz');n.setInheritedAttributes();pp(n.attributes.get('mathvariant'));Mi.singleCharacter=old;
 modules.mspace.MmlMspace.NONSPACELIKE.push('width');const space=f.create('mspace',{width:'1em'});eq(space.canBreak);modules.mspace.MmlMspace.NONSPACELIKE.pop();end();
}
// Native resource capacity is frozen literally; these do not probe VM stack size.
cases++;assertions+=6;

const profile={assertions,cases,sourceProfile:encode(sourceProfile),directProfile};
function check(){const file=path.join(root,'docs/math-port/evidence/leaf-source-profile.json');const frozen=JSON.parse(fs.readFileSync(file,'utf8'));const legacy=fs.readFileSync(path.join(root,frozen.legacy.path));assert.equal(sha(legacy),frozen.legacy.sha256,'Original leaf profile bytes changed');assert.equal(legacy.length,frozen.legacy.bytes);assert.equal(profile.assertions,frozen.profile.assertions);assert.equal(profile.cases,frozen.profile.cases);for(const key of ['sourceProfile','directProfile']){assert.equal(profile[key].length,frozen.profile[key].length);for(let i=0;i<profile[key].length;i++)assert.equal(JSON.stringify(profile[key][i]),JSON.stringify(frozen.profile[key][i]),key+' record '+i);}return {sources:lock.files.length,legacyAssertions:assertions,legacyCases:cases,sourceCases:sourceProfile.filter(r=>r[0]==='case').length,helperObservations:sourceProfile.filter(r=>r[0]!=='case').length,directObservations:directProfile.length,profileSha256:sha(fs.readFileSync(file)),mode:'check'};}
module.exports={profile,check};
if(require.main===module){assert.ok(process.argv.includes('--check'),'Source-only oracle is check-only');console.log(JSON.stringify(check()));}
