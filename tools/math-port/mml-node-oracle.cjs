// Root-frozen MmlNode behavior from actual pinned executable classes.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),npm=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
const src=fs.readFileSync(path.join(root,'.reference/MathJax-src/ts/core/MmlTree/MmlNode.ts'));
assert.equal(sha(src),'cc0594eb5782928412c4307637866d9f80ac147af577297edd48ee88b63b8656');
assert.equal(fs.readFileSync(path.join(npm,'ts/core/MmlTree/MmlNode.ts'),'utf8').replaceAll('\r\n','\n'),src.toString().replaceAll('\r\n','\n'));
const resolved=require.resolve('@mathjax/src/js/core/MmlTree/MmlNode.js',{paths:[path.join(root,'tools/mathjax-bundle')]});
assert.equal(fs.realpathSync(resolved),fs.realpathSync(path.join(npm,'cjs/core/MmlTree/MmlNode.js')));
assert.equal(sha(fs.readFileSync(resolved)),'86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45');
const {AbstractMmlNode:N,AbstractMmlTokenNode:TN,AbstractMmlLayoutNode:LN,AbstractMmlBaseNode:BN,AbstractMmlEmptyNode:EN,TextNode:Text,XMLNode:XML,MATHVARIANTS,indentAttributes,TEXCLASSNAMES}=require(resolved);
const {AbstractNodeFactory:NF}=require(path.join(npm,'cjs/core/Tree/NodeFactory.js'));
class Plain extends N{get kind(){return 'plain';}}
class Row extends Plain{get kind(){return 'mrow';}}
class Inferred extends Row{get kind(){return 'inferredMrow';}get isInferred(){return true;}get notParent(){return true;}}
class MathNode extends Plain{static defaults={mathbackground:'transparent',mathcolor:'black',mathsize:'normal',dir:'ltr',scriptlevel:0,displaystyle:false};get kind(){return 'math';}}
class Fixed extends Plain{get kind(){return 'fixed';}get arity(){return 2;}}
class Unary extends Plain{get kind(){return 'unary';}get arity(){return 1;}}
class Layout extends LN{get kind(){return 'layout';}}
class Base extends BN{get kind(){return 'base';}}
class Token extends TN{get kind(){return 'token';}}
class Leaf extends Plain{get kind(){return 'leaf';}get isEmpty(){return false;}get isSpacelike(){return !!this.getProperty('space');}get isEmbellished(){return !!this.getProperty('embellished');}hasSpacingAttributes(){return !!this.getProperty('spacing');}setTeXclass(p){if(this.onClass)this.onClass(p);return super.setTeXclass(p);}copy(){if(this.onCopy)this.onCopy();return super.copy();}setInheritedAttributes(a,d,l,p){if(this.onInherit)this.onInherit(a,d,l,p);return super.setInheritedAttributes(a,d,l,p);}verifyTree(o){if(this.onVerify)this.onVerify(o);return super.verifyTree(o);}}
class Mi extends Leaf{get kind(){return 'mi';}}class Atom extends Leaf{get kind(){return 'TeXAtom';}}
class ErrorNode extends Plain{get kind(){return 'merror';}}class Mtext extends Token{get kind(){return 'mtext';}}
class Container extends Plain{get kind(){return 'container';}get linebreakContainer(){return true;}}
class Rich extends Plain{get kind(){return 'rich';}}
class VText extends Text{get kind(){return 'vtext';}getText(){this.gets=(this.gets||0)+1;return super.getText();}setText(t){this.sets=(this.sets||0)+1;return super.setText(t);}}
class Trace extends Plain{get kind(){return 'trace';}setChildren(c){(this.trace||(this.trace=[])).push(this.attributes?'children:attrs':'children:absent');return super.setChildren(c);}}
function factory(){return new NF({plain:Plain,mrow:Row,inferredMrow:Inferred,math:MathNode,fixed:Fixed,unary:Unary,layout:Layout,base:Base,token:Token,leaf:Leaf,mi:Mi,TeXAtom:Atom,merror:ErrorNode,mtext:Mtext,container:Container,rich:Rich,text:Text,vtext:VText,XML,trace:Trace});}

const sourceProfile=[],directProfile=[];let currentCase='';let assertions=0,cases=0;
function encode(v){if(v===undefined)return {$type:'undefined'};if(typeof v==='number'){if(Number.isNaN(v))return {$type:'number',value:'NaN'};if(Object.is(v,-0))return {$type:'number',value:'-0'};if(!Number.isFinite(v))return {$type:'number',value:String(v)};}if(v===Object.prototype)return {$type:'builtin',value:'prototype'};for(const [name,tag] of Object.entries(builtins))if(v===Object.prototype[name])return {$type:'builtin',value:tag};if(v&&typeof v==='object'){const out=Array.isArray(v)?new Array(v.length):{};for(const k of Object.keys(v))Object.defineProperty(out,k,{value:encode(v[k]),enumerable:true,writable:true,configurable:true});return out;}return v;}
function direct(value){directProfile.push([currentCase,encode(value)]);}

const builtins={constructor:'constructor',__defineGetter__:'defineGetter',__defineSetter__:'defineSetter',hasOwnProperty:'hasOwnProperty',__lookupGetter__:'lookupGetter',__lookupSetter__:'lookupSetter',isPrototypeOf:'isPrototypeOf',propertyIsEnumerable:'propertyIsEnumerable',toString:'toString',valueOf:'valueOf',toLocaleString:'toLocaleString'};


function begin(name){sourceProfile.push(['case',name]);currentCase=name;cases++;return factory();}function end(){}
function eq(v){assertions++;sourceProfile.push(['eq',encode(v)]);}
function pp(v){assertions++;sourceProfile.push(['property',encode(v)]);}function opt(v){pp(v);}
function list(v){assertions++;sourceProfile.push(['list',v]);}
function throws(fn){let err;try{fn();}catch(x){err=x;}assert.ok(err,'Expected source exception');sourceProfile.push(['exception',err.name]);assertions++;}
function own(js){list(Object.keys(js));for(const k of Object.keys(js))pp(js[k]);}
{
  const f=begin('constructor_state_and_defaults'),n=f.create('trace');list(n.trace);eq(n.childNodes.length);eq(n.attributes.get('mathcolor'));opt(n.texClass);opt(n.prevClass);pp(n.prevLevel);eq(n.isToken);eq(n.isEmpty);eq(n.isEmbellished);eq(n.isSpacelike);eq(n.linebreakContainer);eq(n.linebreakAlign);eq(n.isInferred);eq(n.notParent);eq(n.Parent===null);eq(n.core()===n);eq(n.coreMO()===n);eq(n.coreIndex());eq(n.childPosition()===null);eq(n.arity);
 const a=f.create('plain',null);eq(a.attributes.getExplicitNames().length);throws(()=>f.create('fixed',{},null));
 const gd=MathNode.defaults;gd.mathcolor='red';pp(n.attributes.get('mathcolor'));gd.mathcolor='black';
 const od=Plain.defaults;od.custom='old';const x=f.create('plain');od.custom='new';pp(x.attributes.getDefault('custom'));delete od.custom;end();
}
{
  const f=begin('inferred_routing_conversion_and_identity'),a=f.create('leaf'),b=f.create('leaf'),l=f.create('layout',{},[a,b]);eq(l.childNodes.length);eq(l.childNodes[0].kind);eq(a.parent===l.childNodes[0]);eq(a.Parent===l);eq(a.childPosition());eq(b.childPosition());const c=f.create('leaf');eq(l.appendChild(c)===c);
  const r=f.create('inferredMrow',{},[f.create('leaf')]);r.attributes.set('id','row');r.setProperty('p',7);const fixed=f.create('fixed');const ret=fixed.appendChild(r);eq(ret===r);eq(ret.kind);eq(ret.attributes===r.attributes);pp(ret.getProperty('p'));eq(ret.childNodes[0]===r.childNodes[0]);eq(r.childNodes[0].parent===ret);
  const pnode=f.create('plain'),orig=r.childNodes[0];eq(pnode.appendChild(r)===r); // move generated call after declaration
 eq(orig.parent===pnode);throws(()=>pnode.appendChild(null));eq(pnode.childNodes.length);end();
}
{
  const f=begin('copy_explicit_id_null_and_dynamic_registry'),pnode=f.create('plain',{id:'root',mathcolor:'blue'}),c=f.create('leaf',{id:'child'});pnode.appendChild(c);pnode.attributes.setInherited('mathsize','big');pnode.setProperty('x',5);const copy=pnode.copy(true);pp(copy.attributes.getExplicit('id'));pp(copy.childNodes[0].attributes.getExplicit('id'));pp(copy.attributes.getInherited('mathsize'));pp(copy.getProperty('x'));eq(copy.childNodes[0].parent===copy);pnode.childNodes.push(null);const without=pnode.copy();eq(without.childNodes.length);eq(without.childNodes[1]===null);pp(without.attributes.getExplicit('id'));
 f.setNodeClass('plain',Row);eq(pnode.copy().kind);end();
}
{
  const f=begin('copy_live_children_inferred_and_properties'),l=f.create('layout',{},[f.create('leaf')]);const old=l.childNodes[0].childNodes;l.childNodes[0].childNodes[0].onCopy=()=>old.push(f.create('leaf'));const c=l.copy();eq(c.childNodes.length);eq(c.childNodes[0].childNodes.length);eq(c.childNodes[0].childNodes[1].parent===c.childNodes[0]);const value={a:1};l.setProperty('alias',value);const alias=l.copy();eq(alias.getProperty('alias')===value);l.childNodes=null;eq(l.copy().childNodes.length);end();
}
for(const prevLevel of [null,undefined,false,true,0,1,-1,NaN,' 1 ','0x1','bad','']){
 const f=begin('spacing_'+cases),n=f.create('plain');n.prevLevel=prevLevel;for(let prev=0;prev<8;prev++)for(let cur=0;cur<8;cur++){n.prevClass=prev;n.texClass=cur;pp(n.texSpacing());}end();
}
{
 const f=begin('spacing_current_level_class_transfer_and_invalid'),a=f.create('plain'),b=f.create('plain');a.texClass=1;a.attributes.set('scriptlevel','0x2');eq(b.setTeXclass(a)===a);opt(b.prevClass);pp(b.prevLevel);b.texClass=2;pp(b.texSpacing());a.prevClass=3;a.prevLevel='2';b.updateTeXclass(a);opt(b.texClass);opt(b.prevClass);pp(b.prevLevel);opt(a.prevClass);pp(a.prevLevel);b.prevClass=0;b.texClass=99;pp(b.texSpacing());b.prevClass=99;throws(()=>b.texSpacing());b.prevClass=null;pp(b.texSpacing());end();
}
{
 const f=begin('inheritance_policy_explicit_and_sticky_prime'),n=f.create('fixed',{displaystyle:true,scriptlevel:undefined}),a={mathcolor:['mstyle','red'],scriptminsize:['mstyle','6px'],unknown:['mstyle','u']};const child=f.create('leaf');n.appendChild(child);let trace=[];child.onInherit=(attrs,d,l,p)=>trace.push([d,l,p]);n.setInheritedAttributes(a,false,'2',true);eq(n.childNodes.length);pp(n.attributes.get('mathcolor'));pp(n.attributes.getExplicit('displaystyle'));pp(n.attributes.getInherited('scriptlevel'));pp(n.attributes.getInherited('scriptminsize'));pp(n.attributes.getInherited('unknown'));for(let i=0;i<3;i++)pp(trace[0][i]);n.setInheritedAttributes({},false,0,false);pp(n.getProperty('texprimestyle'));
  const pnode=f.create('fixed',{},[f.create('leaf'),f.create('leaf'),f.create('leaf')]),old=pnode.childNodes[2];pnode.setInheritedAttributes();eq(pnode.childNodes.length);eq(old.parent===pnode);const unary=f.create('unary',{},[f.create('leaf'),f.create('leaf')]);unary.setInheritedAttributes();eq(unary.childNodes.length);end();
}
{
 const f=begin('inheritance_stop_noinherit_and_indent'),n=f.create('plain'),child=f.create('leaf');n.appendChild(child);let seen;n.kind;N.stopInherit.plain={mathcolor:true};N.noInherit.mstyle.plain={dir:true};const a={mathcolor:['mstyle','red'],dir:['mstyle','rtl']};child.onInherit=x=>seen=x;n.setInheritedAttributes(a);pp(n.attributes.getInherited('dir'));eq(Object.hasOwn(seen,'mathcolor'));eq(Object.hasOwn(a,'mathcolor'));delete N.stopInherit.plain;delete N.noInherit.mstyle.plain;
 const container=f.create('container',{'data-align':'right'},[f.create('leaf')]);let ind;container.childNodes[0].onInherit=x=>ind=x;container.setInheritedAttributes();list(Object.keys(ind));for(const key of Object.keys(ind)){pp(ind[key][0]);pp(ind[key][1]);}end();
}
{
 const f=begin('add_inherit_from_and_prototype'),a=f.create('plain'),b=f.create('plain');a.attributes.set('mathsize','2em');a.attributes.setInherited('displaystyle',true);a.attributes.setInherited('scriptlevel','3');a.setProperty('texprimestyle','yes');b.inheritAttributesFrom(a);pp(b.attributes.get('mathsize'));pp(b.attributes.getInherited('scriptlevel'));pp(b.getProperty('texprimestyle'));const props={x:7,displaystyle:true,scriptlevel:4,style:'x'};const result=b.addInheritedAttributes({},props);list(Object.keys(result));pp(result.x[1]);const strange=Object.fromEntries([['__proto__',8]]);const pro=b.addInheritedAttributes({},strange);list(Object.keys(pro));eq(Object.getPrototypeOf(pro)[0]);end();
}
{
  const f=begin('token_text_identity_walk_and_presence'),t=f.create('token'),x=f.create('text');x.setText('a\ud800');const y=f.create('vtext');y.setText('😀');const rich=f.create('rich');rich.textContent=()=>'<b>';t.setChildren([x,y,f.create('leaf'),rich]);assertions++;direct(t.getText());eq(t.isEmpty);let seen=[];t.walkTree(n=>seen.push(n.kind));list(seen);rich.textContent='bad';throws(()=>t.getText());delete rich.textContent;t.childNodes.push(null);throws(()=>t.getText());t.setChildren([f.create('text')]);eq(t.isEmpty);end();
}
{
  const f=begin('token_inheritance_live_walk_and_class_predicate'),t=f.create('token'),x=f.create('text'),a=f.create('leaf'),late=f.create('leaf');t.setChildren([x,a]);let visits=0;a.onInherit=()=>visits++;t.setInheritedAttributes({},true,2,true);eq(visits);eq(x.attributes===null);const original=t.childNodes;let seen=[];const data={n:0};const result=t.walkTree((n,d)=>{seen.push(n.kind);d.n++;if(n===a){original.push(late);t.childNodes=[];}},data);list(seen);eq(data.n);eq(result===data);end();
}
{
  const f=begin('layout_base_class_transfer_and_snapshot_scripts'),prev=f.create('leaf');prev.texClass=3;const mi=f.create('mi');mi.texClass=1;const script=f.create('leaf'),base=f.create('base',{},[mi,script]);let calls=[];mi.onClass=pnode=>calls.push(pnode===prev?'prev':'other');script.onClass=pnode=>calls.push(pnode===null?'null':'other');const result=base.setTeXclass(prev);eq(result===mi);list(calls);opt(base.texClass);opt(base.prevClass);opt(mi.prevClass);eq(base.core()===mi);eq(base.coreMO()===mi);
  const l=f.create('layout',{},[f.create('leaf')]);l.childNodes[0].texClass=2;const lres=l.setTeXclass(prev);eq(lres===l.childNodes[0]);opt(l.texClass);eq(l.core()===l.childNodes[0]);eq(l.isSpacelike);end();
}
{
 const f=begin('verify_error_short_full_wrapping_and_root_partial');for(const [full,short]of [[false,false],[false,true],[true,false]]){const parent=f.create('plain'),n=f.create('plain');parent.appendChild(n);const err=n.mError('message',{fullErrors:full},short);eq(parent.childNodes[0]===err);pp(err.attributes.getExplicit('data-mjx-message'));pp(err.attributes.getExplicit('title'));eq(n.parent===err);eq(err.childNodes[0].kind);if(full||short){assertions++;direct(err.childNodes[0].childNodes[0].getText());}if(full||short)throws(()=>n.mError('again',{}));else eq(n.mError('again',{})===null);}const n=f.create('plain');throws(()=>n.mError('root',{},true));end();
}
{
 const f=begin('verify_rules_arity_variant_and_child_traversal'),parent=f.create('plain'),n=f.create('fixed',{unknown:'a','data-x':'a',id:'i',href:'h',mathvariant:'bad'});parent.appendChild(n);n.appendChild(f.create('leaf'));const options={checkArity:true,checkAttributes:true,checkMathvariants:true,fullErrors:false};n.verifyTree(options);eq(parent.childNodes[0].kind);pp(parent.childNodes[0].attributes.getExplicit('data-mjx-message'));eq(n.parent.kind);const detached=f.create('fixed');detached.verifyTree();eq(detached.childNodes.length);const good=f.create('plain',{mathvariant:'not-valid'});good.setProperty('ignore-variant',true);good.verifyTree({checkMathvariants:true});eq(good.parent===null);end();
}
{
 const f=begin('empty_text_readonly_virtual_copy_and_utf16'),n=f.create('vtext');n.setText('x\ud800😀');n.setProperty('id','notcopied');const c=n.copy(true);assertions++;direct(c.getText());pp(c.getProperty('id'));eq(c.sets);eq(n.gets);eq(n.isEmpty);eq(n.isToken);eq(n.arity);opt(n.texClass);opt(n.prevClass);pp(n.prevLevel);eq(n.attributes===null);eq(n.childPosition());eq(n.setTeXclass(c)===c);pp(n.texSpacing());throws(()=>{n.texClass=1;});throws(()=>{n.prevClass=1;});throws(()=>{n.prevLevel=1;});throws(()=>{n.attributes={};});eq(n.replaceChild(c,n)===n);eq(n.appendChild(c)===c);end();
}
{
 const f=begin('xml_adaptor_clone_order_and_omission'),n=f.create('XML'),x={value:'a'},trace=[],adaptor={clone(o){trace.push('clone');return {...o};},serializeXML(o){trace.push('serialize');return '<'+o.value+'/>';}};n.setXML(x,adaptor);const c=n.copy();eq(n.getXML()===x);eq(c.getXML()===x);list(trace);eq(n.getSerializedXML());throws(()=>c.getSerializedXML());eq(n.toString());const blank=f.create('XML');throws(()=>blank.copy());end();
}
{
 begin('root_scalar_null_and_lossless_bridge');const values=[undefined,null,false,true,0,-0,1,NaN,Infinity,' 0x1 ','-2','bad','',Object.prototype.toString,Object.prototype,{}];for(const v of values){pp(Boolean(v));pp(Number(v));assertions++;direct(String(v));}const dict={x:null};eq(Object.hasOwn(dict,'x'));pp(dict.x);Object.setPrototypeOf(dict,null);pp(dict.toString);dict.__proto__='own';pp(dict.__proto__);for(const s of ['a\ud800','\udc00','😀','\ud800\ud800\udc00']){assertions++;direct(s);}end();
}
{
 begin('static_defaults_and_exports');own(N.defaults);own(TN.defaults);eq(LN.defaults===N.defaults);eq(BN.defaults===N.defaults);assertions++;direct([...MATHVARIANTS]);list(indentAttributes);list(TEXCLASSNAMES);end();
}
{
 const f=begin('null_explicit_and_empty_core_partial_state');
 const n=f.create('plain',{displaystyle:null,scriptlevel:null});n.setInheritedAttributes({},true,3,true);

 pp(n.attributes.getExplicit('displaystyle'));pp(n.attributes.getExplicit('scriptlevel'));
 pp(n.attributes.getInherited('displaystyle'));pp(n.attributes.getInherited('scriptlevel'));
 const a=f.create('plain'),text=f.create('text');
 throws(()=>a.getPrevClass(text));opt(a.prevClass);pp(a.prevLevel);
 throws(()=>a.updateTeXclass(text));opt(a.prevClass);pp(a.prevLevel);end();
}
{
 const f=begin('live_inheritance_verify_and_base_script_snapshot');
 const n=f.create('plain'),a=f.create('leaf'),late=f.create('leaf');n.appendChild(a);const original=n.childNodes;let seen=[];
 a.onInherit=()=>{seen.push('a');original.push(late);n.childNodes=[];};late.onInherit=()=>seen.push('late');n.setInheritedAttributes();
 list(seen);
 const pnode=f.create('plain'),v=f.create('leaf'),extra=f.create('leaf');pnode.appendChild(v);const array=pnode.childNodes;seen=[];
 v.onVerify=()=>{seen.push('v');array.push(extra);pnode.childNodes=[];};extra.onVerify=()=>seen.push('extra');pnode.verifyTree({});
 list(seen);
 const base=f.create('base',{},[f.create('leaf'),f.create('leaf'),f.create('leaf')]),script=base.childNodes[1],third=base.childNodes[2];seen=[];
 script.onClass=()=>{seen.push('script');base.childNodes.splice(2,1);};third.onClass=()=>seen.push('third');base.setTeXclass(null);
 list(seen);end();
}
{
 const f=begin('defaults_missing_metadata_and_null_children');
 class NoDefaults extends Plain{static defaults=undefined;}
 f.setNodeClass('plain',NoDefaults);const n=f.create('plain');
 eq(n.attributes.getDefaultNames().length);pp(n.attributes.get('mathcolor'));
 class BadMath extends MathNode{static defaults=undefined;}
 f.setNodeClass('math',BadMath);throws(()=>f.create('plain'));
 f.setNodeClass('math',MathNode);
 const pnode=f.create('plain');pnode.childNodes=[null];
 throws(()=>pnode.copy());throws(()=>pnode.setInheritedAttributes());throws(()=>pnode.verifyTree({}));end();
}
{
 const f=begin('normal_isEmpty_captures_array_identity');
 class EmptyProbe extends Plain{get kind(){return 'emptyProbe';}get isEmpty(){if(this.onEmpty)this.onEmpty();return true;}}
 f.setNodeClass('emptyProbe',EmptyProbe);const n=f.create('plain'),probe=f.create('emptyProbe');n.setChildren([probe,f.create('leaf')]);probe.onEmpty=()=>{n.childNodes=[];};

 eq(n.isEmpty);eq(n.childNodes.length);end();
}
{
 const f=begin('computed_texclass_uses_virtual_source_property');
 class Computed extends Plain{get kind(){return 'computed';}get texClass(){return 2;}set texClass(v){}}
 f.setNodeClass('computed',Computed);const n=f.create('computed');n.prevClass=0;

 pp(n.texSpacing());eq(n.setTeXclass(null)===n);end();
}
{
 const f=begin('layout_ignores_core_override_for_class_update');
 class CoreOverride extends Layout{get kind(){return 'coreOverride';}core(){this.calls=(this.calls||0)+1;return this.fake;}}
 f.setNodeClass('coreOverride',CoreOverride);const n=f.create('coreOverride');n.childNodes[0].texClass=1;n.fake=f.create('leaf');n.fake.texClass=3;

 n.setTeXclass(null);opt(n.texClass);eq(n.calls||0);end();
}
{
 const f=begin('finite_arity_null_checks_before_virtual_children');
 class Observe extends Fixed{verifyChildren(){(this.trace||(this.trace=[])).push('verify');}setChildInheritedAttributes(){(this.trace||(this.trace=[])).push('inherit');}}
 f.setNodeClass('fixed',Observe);const n=f.create('fixed');n.childNodes=null;

 throws(()=>n.verifyTree({checkArity:true}));list(n.trace||[]);
 throws(()=>n.setInheritedAttributes());list(n.trace||[]);end();
}
{
 const f=begin('text_toString_raw_field_and_xml_factory_clone_order');
 const n=f.create('vtext');n.setText('a\ud800');const string=n.toString();

 // A lossless internal bridge: each unpaired unit must survive generic join.
 assertions++;direct(string);eq(n.gets||0);
 const x=f.create('XML'),trace=[],adaptor={clone(o){trace.push('clone');return {};},serializeXML(){return '';}};
 class OrderedXML extends XML{constructor(...args){trace.push('create');super(...args);}}
 x.setXML({},adaptor);f.setNodeClass('XML',OrderedXML);x.copy();
 list(trace);end();
}
{
 const f=begin('invalid_spacing_column_keeps_level_evaluation');const n=f.create('plain');n.prevClass=0;n.texClass=99;n.prevLevel=0;n.attributes=null;

 throws(()=>n.texSpacing());n.prevLevel=1;pp(n.texSpacing());end();
}
{
 const f=begin('token_excludes_overridden_empty_methods');
 class SpyText extends Text{setInheritedAttributes(){this.inherits=(this.inherits||0)+1;}walkTree(){this.walks=(this.walks||0)+1;}}
 f.setNodeClass('text',SpyText);const t=f.create('token'),x=f.create('text');t.setChildren([x]);t.setInheritedAttributes();t.walkTree(()=>{});

 eq(x.inherits||0);eq(x.walks||0);end();
}
{
 const f=begin('short_error_replacement_nulls_attributes_after_mutation');
 class ParentProbe extends Plain{replaceChild(n,o){const r=super.replaceChild(n,o);n.attributes=null;return r;}}
 f.setNodeClass('plain',ParentProbe);const parent=f.create('plain'),n=f.create('leaf');parent.appendChild(n);

 throws(()=>n.mError('broken',{},true));eq(parent.childNodes[0].kind);eq(n.parent===null);eq(parent.childNodes[0].attributes===null);end();
}
{
 const f=begin('variant_validation_without_other_error_masking');
 for(const value of ['bad',42,true,'normal','',0,false,null]){const parent=f.create('plain'),n=f.create('token',{mathvariant:value});parent.appendChild(n);n.verifyTree({checkMathvariants:true});

 eq(parent.childNodes[0].kind);pp(parent.childNodes[0].attributes.getExplicit('data-mjx-message'));}end();
}
{
 const f=begin('explicit_undefined_inheritance_arguments_use_source_defaults'),parent=f.create('plain'),child=f.create('leaf');parent.appendChild(child);let seen;child.onInherit=(a,d,l,p)=>seen=[d,l,p];parent.setInheritedAttributes({},undefined,undefined,undefined);

 seen.forEach((v,i)=>pp(v));end();
}
// Literal resource-adaptation checks are native-only, not JS stack-capacity claims.
cases++;assertions+=7;

module.exports={assertions,cases,sourceProfile:encode(sourceProfile),directProfile};
if(require.main===module)require('./mml-node-source-profile.cjs').check(module.exports);
