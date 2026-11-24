'use strict';
// Pinned Mathtools tag component. Native fixtures use typed state actions and pure TeX only.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [file,sha]of [
 ['.reference/MathJax-src/ts/input/tex/mathtools/MathtoolsTags.ts','2437a2beea0f0c04fac8697ab407a09672b2395d483b1942aefa51e9df057e47'],
 ['.reference/MathJax-src/ts/input/tex/mathtools/MathtoolsMethods.ts','ad6562e23ba65cafdb480afc33387982c7f7b14a2886ae7844dfbe64ec44e918'],
 ['.reference/MathJax-src/ts/input/tex/mathtools/MathtoolsConfiguration.ts','7b14b1d6e4324397ab315fa012ceada7999df6cbf3b4c9f09fc3742f8b5a30ae'],
 ['.reference/MathJax-src/ts/input/tex/tagformat/TagFormatConfiguration.ts','fdb658f76cd044e64b233b999e216b93720c83ca2d0ede3b033d5759f4755a42'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/mathtools/MathtoolsTags.js','bcdd1c9f8592d5640fa2b8bb8cade19c29e2d8f732e19a2dfbcb5415135e3e2d'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/mathtools/MathtoolsMethods.js','ec67f90855dec0ab60f4933beffe081799247a2ac01572429b696e4e02ed8a5a'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/mathtools/MathtoolsConfiguration.js','863415dfbc384852e5b3b39724d50cc453e1bb6e54f8f6a1073b9e5e43b8734d'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/tagformat/TagFormatConfiguration.js','54cb07c08095308bd78d232998366afe0053d74abfe1829cdf2b640921ab29bd']
])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=f=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',f));
const {TeX}=load('input/tex.js'),{AbstractMathItem}=load('core/MathItem.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js');
const {MathtoolsTagFormat}=load('input/tex/mathtools/MathtoolsTags.js'),{MathtoolsConfiguration}=load('input/tex/mathtools/MathtoolsConfiguration.js');
const {TagFormatConfiguration}=load('input/tex/tagformat/TagFormatConfiguration.js'),{TagsFactory}=load('input/tex/Tags.js');
const TexParser=load('input/tex/TexParser.js').default,{MathtoolsMethods}=load('input/tex/mathtools/MathtoolsMethods.js');
const forms=()=>({box:['[',']',''],bold:['<','>','\\mathbf']});let compiles=0;
function nodes(root,kind){const a=[];root.walkTree(n=>{if(n.kind===kind)a.push(n)});return a;}
function text(root){let s='';root.walkTree(n=>{if(n.isToken)s+=n.getText()});return s;}
function fixture(mode='ams',tagformat=false,reverse=false,tagforms=forms(),explicitRef=false,mathtools=true){
 let error='';TagsFactory.setDefault('none');
 const jax=new TeX({packages:['base','ams','cases',...(mathtools?['mathtools']:[]),...(tagformat?['tagformat']:[])],tags:mode,maxMacros:1000,...(mathtools?{mathtools:{tagforms}}:{}),...(tagformat?{tagformat:{number:n=>'N'+n,tag:t=>['<',t,'>'],ref:explicitRef?t=>'REF:'+t:''}}:{}),formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());
 if(reverse){
  // Config callbacks select a factory. Materialize it exactly as TeX construction does;
  // generated class names and global registration growth are not output contracts.
  MathtoolsTagFormat(jax.configuration,jax);TagsFactory.setDefault(jax.parseOptions.options.tags);jax.parseOptions.tags=TagsFactory.getDefault();jax.parseOptions.tags.configuration=jax.parseOptions;
 }
 return {jax,compile(source){++compiles;error='';const tree=jax.compile(new AbstractMathItem(source,jax,true),reference.document);return {error,tree}}};
}
function ok(f,source){const r=f.compile(source);assert.equal(r.error,'',source);assert.equal(f.jax.parseOptions.parsers.length,0);return r.tree;}
function snapshot(root){let s='';const add=v=>{v=String(v);s+=v.length+':'+v};function visit(n){if(!n){add('null');return}add(n.kind);const attrs=Object.entries(n.attributes.getAllAttributes()).sort(([a],[b])=>a<b?-1:a>b?1:0);add(attrs.length);for(const [key,v]of attrs){add(key);add(v===null||v===undefined?'null':typeof v);add(v===null||v===undefined?'':String(v))}add(n.isToken?n.getText():'');const children=n.isToken?[]:n.childNodes;add(children.length);for(const c of children)visit(c)}visit(root);return s;}
function select(f,id){const t=f.jax.parseOptions.tags;t.mtCurrent=id?t.mtFormats.get(id):null;}
assert.equal(MathtoolsConfiguration.configMethod[1],5);assert.equal(TagFormatConfiguration.configMethod[1],10);
const trees=[],steps=[];
for(const mode of ['none','all','ams','cases'])for(const [tagformat,reverse]of [[false,false],[true,false],[true,true]]){
 for(const selected of ['','box','bold']){
  const f=fixture(mode,tagformat,reverse);select(f,selected);const source='\\begin{equation}x\\label{row}\\end{equation}';trees.push({mode,tagformat,reverse,selected,source,snapshot:snapshot(ok(f,source))});
 }
 for(const [selected,source]of [['box','x'],['bold','\\begin{align}a&=b\\\\c&=d\\end{align}'],['box','\\begin{subnumcases}{f=}a & one\\\\b & two\\end{subnumcases}']]){
  const f=fixture(mode,tagformat,reverse);select(f,selected);trees.push({mode,tagformat,reverse,selected,source,snapshot:snapshot(ok(f,source))});
 }
}
{
 const f=fixture('all');
 const sequence=[[0,'x\\label{first}'],[1,'y\\label{box}'],[2,'z\\label{old}'],[3,'q\\label{new}'],[4,'r\\label{clear}'],[5,'s\\label{bold}'],[0,'\\ref{first}+\\eqref{box}+\\eqref{old}+\\eqref{new}+\\eqref{clear}+\\eqref{bold}'],[4,'\\eqref{box}']];
 for(const [action,source]of sequence){
  const t=f.jax.parseOptions.tags;if(action===1||action===3)select(f,'box');if(action===2)t.mtFormats.set('box',['(',')','']);if(action===4)select(f,'');if(action===5)select(f,'bold');
  if(action===2)assert.deepEqual(t.mtCurrent,['[',']','']);if(action===3)assert.deepEqual(t.mtCurrent,['(',')','']);
  steps.push({action,source,snapshot:snapshot(ok(f,source))});
 }
 assert.deepEqual(Object.fromEntries(Object.entries(f.jax.parseOptions.tags.allLabels).map(([k,v])=>[k,v.tag])),{first:'1',box:'2',old:'3',new:'4',clear:'5',bold:'6'});
}
// Source-only macro assertions: these methods are not yet connected by the native tags packet.
let methodErrors=0;
for(const reverse of [false,true]){
 const f=fixture('ams',true,reverse);ok(f,'\\begin{equation*}\\usetagform{box}\\end{equation*}');
 const t=f.jax.parseOptions.tags;assert.deepEqual(t.mtCurrent,['[',']','']);
 ok(f,'\\begin{equation*}\\renewtagform{box}{(}{)}\\end{equation*}');assert.deepEqual(t.mtCurrent,['[',']','']);assert.deepEqual(t.mtFormats.get('box'),['(',')','']);
 ok(f,'\\begin{equation*}\\usetagform{box}\\end{equation*}');assert.deepEqual(t.mtCurrent,['(',')','']);
 ok(f,'\\begin{equation*}\\renewtagform{absent}{A}{B}\\usetagform{absent}\\end{equation*}');assert.deepEqual(t.mtCurrent,['A','B','']);
 ok(f,'\\begin{equation*}\\usetagform{}\\end{equation*}');assert.equal(t.mtCurrent,null);assert.deepEqual(t.formatTag('Q'),['<','Q','>']);
 for(const [source,id]of [['\\newtagform{}{[}{]}','InvalidTagFormID'],['\\newtagform{box}{(}{)}','DuplicateTagForm'],['\\newtagform{box}{(}','MissingArgFor'],['\\usetagform{missing}','UndefinedTagForm']]){assert.equal(f.compile(source).error,id);++methodErrors;}
}
for(const reverse of [false,true])for(const explicitRef of [false,true]){
 const f=fixture('ams',true,reverse,forms(),explicitRef);select(f,'bold');const t=f.jax.parseOptions.tags;
 assert.deepEqual(t.formatTag('Q'),reverse?['<','\\mathbf{Q}','>']:['<','Q','>']);
 assert.deepEqual(t.formatRef('Q'),explicitRef?'REF:Q':t.formatTag('Q'));assert.equal(t.formatNumber(7,2),'N7');select(f,'');assert.deepEqual(t.formatTag('Q'),['<','Q','>']);
}
for(const mode of ['none','all','ams','cases']){
 const f=fixture(mode,true),t=f.jax.parseOptions.tags;t.start('subnumcases',true,true);t.autoTag();assert.equal(t.counter,mode==='none'?0:1);assert.equal(t.currentTag.tag,mode==='none'?null:'N1');t.clearTag();t.autoTag();assert.equal(t.counter,mode==='none'?0:mode==='cases'?1:2);t.end();
}
{
 const f=fixture('ams',false,false,forms(),false,false),p=new TexParser('',{},f.jax.parseOptions);
 assert.throws(()=>MathtoolsMethods.NewTagForm(p,'\\newtagform'),e=>e.id==='TagsNotMT');assert.throws(()=>MathtoolsMethods.UseTagForm(p,'\\usetagform'),e=>e.id==='TagsNotMT');p.mml();methodErrors+=2;
}
let invalidShapes=0;for(const bad of ['x',['a','b'],['a','b','c','d']]){assert.throws(()=>fixture('ams',false,false,{bad}),e=>e.id==='InvalidTagFormDef');++invalidShapes;}
// Source validates array shape, not each element's type. The native std::array<string,3>
// boundary intentionally cannot express this untyped option mutation.
{
 const f=fixture('ams',false,false,{numeric:[1,2,3]});select(f,'numeric');assert.deepEqual(f.jax.parseOptions.tags.formatTag('Q'),[1,'3{Q}',2]);
}
const {utf16Field, byteField, writeTables} = require('./fixture-data.cjs');
writeTables(path.join(root, 'tests/math/fixtures/packages/mathtools_tags'), {
  'trees': {columns: ["mode_utf16", "tagformat_boolean", "reverse_boolean", "selected_utf16", "source_utf16", "snapshot_utf16"],
    rows: trees.map(c => [utf16Field(c.mode), +!!c.tagformat, +!!c.reverse, utf16Field(c.selected), utf16Field(c.source), utf16Field(c.snapshot)])},
  'sequence': {columns: ["action_integer", "source_utf16", "snapshot_utf16"],
    rows: steps.map(c => [c.action, utf16Field(c.source), utf16Field(c.snapshot)])},
});
console.log(JSON.stringify({compiles,trees:trees.length,steps:steps.length,methodErrors,invalidShapes,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
