'use strict';
// Pinned SetOptionsConfiguration and actual package TeX compilation; development only.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [file,sha]of [
 ['.reference/MathJax-src/ts/input/tex/setoptions/SetOptionsConfiguration.ts','ccaa243137c68208e7f442013aa5dc6cc04b735ac3e0ca099ea8f30be557790d'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/setoptions/SetOptionsConfiguration.js','dac7cec87b390b199257df54f4942a14f7c5ef0cbef54e75ff4f471551a41852']
])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=f=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',f));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const {SetOptionsConfiguration,SetOptionsUtil:util}=load('input/tex/setoptions/SetOptionsConfiguration.js');
const {Configuration}=load('input/tex/Configuration.js'),{CommandMap}=load('input/tex/TokenMap.js'),{Macro}=load('input/tex/Token.js');
const TexParser=load('input/tex/TexParser.js').default;
let cases=0,checks=0;
function fixture(options={},extra=[],packages){let error='';const jax=new TeX({packages:packages||['base','setoptions',...extra],...options,formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());return {jax,compile(s){++cases;error='';const tree=jax.compile(new AbstractMathItem(s,jax,false),reference.document);return {tree,error}}};}
function nodes(root,kind){const a=[];root.walkTree(n=>{if(n.kind===kind)a.push(n)});return a;}
function text(root){let s='';root.walkTree(n=>{if(n.isToken)s+=n.getText()});return s;}
function ok(f,s){const r=f.compile(s);assert.equal(r.error,'',s);assert.equal(f.jax.parseOptions.parsers.length,0);return r.tree;}
function error(f,s,id){assert.equal(f.compile(s).error,id,s);ok(f,'x');}
const defaults=SetOptionsConfiguration.options.setoptions;
assert.equal(SetOptionsConfiguration.priority,3);assert.equal(SetOptionsConfiguration.configMethod[1],3);
assert.deepEqual({...defaults.allowOptions},{tex:{FindTeX:false,formatError:false,package:false,baseURL:false,tags:false,maxBuffer:false,maxMaxros:false,macros:false,environments:false},setoptions:false,autoload:false,require:false,configmacros:false,tagformat:false});
assert.equal(defaults.allowPackageDefault,true);assert.equal(defaults.allowOptionsDefault,true);
for(const name of ['filterPackage','filterOption','filterValue'])assert.equal(defaults[name]===util[name],true);
// Actual Source defaults define the complete finite native field spelling contract.
{
 const f=fixture({},[],reference.provenance.packages),p=new TexParser('',{},f.jax.parseOptions);p.mml();
 const config=p.options.setoptions,permissions=config.allowOptions;config.allowOptions={};
 for(const [key,value]of Object.entries(p.options)){
  assert.equal(util.filterOption(p,'tex',key),true);++checks;
  if(value&&Object.getPrototypeOf(value)===Object.prototype&&key!=='active'&&key!=='macros'&&key!=='environments')
   for(const nested of Object.keys(value)){assert.equal(util.filterOption(p,key,nested),true);++checks;}
 }
 config.allowOptions=permissions;
}
{
 const f=fixture({},['bbm','dsfont','bboldx','units']);
 for(const [pkg,key,command,variant]of [['bbm','bold','\\mathbbm{A}','-bbm-bold'],['dsfont','sans','\\mathds{A}','-ds-ss'],['bboldx','light','\\mathbb{A}','-bboldx-light']]){
  const root=ok(f,'\\setOptions['+pkg+']{'+key+'}'+command);assert.equal(nodes(root,'mi')[0].attributes.get('mathvariant'),variant);assert.equal(f.jax.parseOptions.options[pkg][key],true);
 }
 let r=ok(f,'\\setOptions[units]{loose,ugly}\\units[2]{kg}+\\nicefrac{m}{s}');assert.equal(nodes(r,'mtext')[0].getText(),'\u00a0');assert.equal(nodes(r,'mfrac')[0].attributes.get('bevelled'),false);
 ok(f,'{\\setOptions[units]{loose=false}}');assert.equal(f.jax.parseOptions.options.units.loose,false);f.jax.reset();assert.equal(nodes(ok(f,'\\units[2]{kg}'),'mspace').length,1);
 ok(f,'\\setOptions[bboldx]{bfbb,light=false}');assert.equal(nodes(ok(f,'\\mathbb{A}'),'mi')[0].attributes.get('mathvariant'),'-bboldx-bold');
 const independent=fixture({},['units']);assert.equal(independent.jax.parseOptions.options.units.loose,false);
}
{
 const f=fixture({},['ams','color','noundefined']);
 assert.equal(nodes(ok(f,'\\setOptions[ams]{multlineWidth=73%,multlineIndent=2em}\\begin{multline}a\\\\b\\end{multline}'),'mtable')[0].attributes.get('width'),'73%');
 const r=ok(f,'\\setOptions[color]{padding=3px,borderWidth=4px}\\colorbox{red}{x}');assert.equal(nodes(r,'mpadded')[0].attributes.get('height'),'+3px');
 const bad=ok(f,'\\setOptions[noundefined]{color=blue,background=false,size=false}\\unknown');assert.equal(nodes(bad,'mtext')[0].attributes.get('mathcolor'),'blue');assert.equal(nodes(bad,'mtext')[0].attributes.isSet('mathbackground'),false);
}
{
 const f=fixture();const o=f.jax.parseOptions.options;
 ok(f,'\\setOptions[]{tagIndent=2em,useLabelIds=false,mathStyle=literal,numberPattern={/^[0-9]+/},initialDigit={/^[A]/},digits={/a/}}12A');
 assert.equal(o.tagIndent,'2em');assert.equal(o.useLabelIds,false);assert.equal(o.mathStyle,'literal');assert.equal(o.numberPattern.source,'^[0-9]+');assert.equal(o.initialDigit.source,'^[A]');assert.equal(o.digits.source,'a');
 // initialDigit and mathStyle maps were captured during construction, unlike numberPattern.
 assert.equal(nodes(ok(f,'12A'),'mn').length,1);assert.equal(nodes(ok(f,'12A'),'mi').length,1);
 for(const [s,id]of [['\\setOptions[absent]','MissingArgFor'],['\\setOptions[absent]{x}','NotAPackage'],['\\setOptions[tagformat]{x}','PackageNotSettable'],['\\setOptions{tags=all}','TeXOptionNotSettable'],['\\setOptions{unknown}','InvalidTexOption'],['\\setOptions[units]{loose}','InvalidOptionKey']])error(f,s,id);
 error(f,'\\setOptions{tagIndent=5em,unknown=x}','InvalidTexOption');assert.equal(o.tagIndent,'5em');
 ok(f,'\\setOptions{tagIndent=1em,tagIndent=4em}');assert.equal(o.tagIndent,'4em');
 const r=ok(f,'a\\setOptions{}b');assert.equal(text(r),'ab');
 o.setoptions.allowOptionsDefault=false;error(f,'\\setOptions{unknown}','TeXOptionNotSettable');o.setoptions.allowOptions.tex.tagIndent=true;ok(f,'\\setOptions{tagIndent=7em}');
 o.setoptions.allowPackageDefault=false;error(f,'\\setOptions[units]{loose}','PackageNotSettable');o.setoptions.allowOptions.units=true;error(f,'\\setOptions[units]{loose}','OptionNotSettable');
}
{
 let packages=0,options=0,values=0;
 const f=fixture({setoptions:{filterPackage(p,e){++packages;return e!=='skip'},filterOption(p,e,k){++options;return k!=='skip'},filterValue(p,e,k,v){++values;return typeof v==='string'?v.toUpperCase():v}}});
 ok(f,'\\setOptions[skip]{tagIndent=bad}');assert.deepEqual([packages,options,values],[1,0,0]);
 ok(f,'\\setOptions{tagIndent=abc,skip=no,useLabelIds=false}');assert.deepEqual([packages,options,values],[2,3,2]);assert.equal(f.jax.parseOptions.options.tagIndent,'ABC');assert.equal(f.jax.parseOptions.options.useLabelIds,false);
}
{
 const f=fixture({},['newcommand']);ok(f,'\\let\\savedOptions\\setOptions');ok(f,'\\savedOptions{tagIndent=9em}');assert.equal(f.jax.parseOptions.options.tagIndent,'9em');
 assert.throws(()=>f.compile('\\setOptions{numberPattern={/[/}}'),SyntaxError);ok(f,'x');
}





// Enumerate real full-profile fields, then execute each writable scalar against Source.
const domainTypes={textmacros:'TextMacrosOptions',setoptions:'SetOptionsOptions',noundefined:'NoUndefinedOptions',bboldx:'BboldxOptions',units:'UnitsOptions',tagformat:'TagFormatOptions',physics:'PhysicsOptions',mathtools:'MathtoolsOptions',dsfont:'DsfontOptions',color:'ColorOptions',begingroup:'BegingroupOptions',bbm:'BbmOptions',amscd:'AmscdOptions',ams:'AmsOptions'};
const memberNames={FindTeX:'findTeX','multlined-gap':'multlinedGap','multlined-pos':'multlinedPos','multlined-width':'multlinedWidth','firstline-afterskip':'firstlineAfterskip','lastline-preskip':'lastlinePreskip','smallmatrix-align':'smallmatrixAlign',shortvdotsadjustabove:'shortvdotsAdjustAbove',shortvdotsadjustbelow:'shortvdotsAdjustBelow','centercolon-offset':'centercolonOffset','thincolon-dx':'thincolonDx','thincolon-dw':'thincolonDw','use-unicode':'useUnicode','prescript-sub-format':'prescriptSubFormat','prescript-sup-format':'prescriptSupFormat','prescript-arg-format':'prescriptArgFormat','allow-mathtoolsset':'allowMathtoolsset'};
const full=fixture({},[],reference.provenance.packages),fields=[];let scalar=0;
full.jax.parseOptions.options.setoptions.allowOptions={};
function addField(pkg,key,value){
 const nativeComplex=key==='ref'&&pkg==='tagformat';
 const kind=nativeComplex?'complex':(typeof value==='boolean'?'boolean':typeof value==='string'?'string':typeof value==='number'?'budget':value instanceof RegExp?'regexp':'complex');
 const input=kind==='boolean'?'false':kind==='string'?'set-value':kind==='budget'?'2048':kind==='regexp'?'/^Q/':'';
 if(kind!=='complex'){
  const target=pkg==='tex'?full.jax.parseOptions.options:full.jax.parseOptions.options[pkg],original=target[key];
  ok(full,'\\setOptions'+(pkg==='tex'?'':'['+pkg+']')+'{'+key+'='+input+'}');
  if(kind==='boolean')assert.equal(target[key],false);else if(kind==='regexp')assert.equal(target[key].source,'^Q');else assert.equal(target[key],input);
  target[key]=original;++scalar;
 }
 const member=memberNames[key]||key;
 const parameters=pkg==='tex'?'&TexOptionValues::'+member:'&TexOptionValues::'+pkg+', &'+domainTypes[pkg]+'::'+member;
 fields.push({binding:parameters.replaceAll('&','').replaceAll(', ','/'),package:pkg,key,kind,input});
}
for(const [key,value]of Object.entries(full.jax.parseOptions.options)){
 addField('tex',key,value);
 if(domainTypes[key])for(const [nested,v]of Object.entries(value))addField(key,nested,v);
}
const {byteField, utf16Field, writeTables} = require('./fixture-data.cjs');
const fixturePath = path.join(root, 'tests/math/fixtures/packages/setoptions');
writeTables(fixturePath, {
 fields: {columns: ['binding_bytes', 'package_utf16', 'key_utf16', 'kind', 'input_utf16'],
  rows: fields.map(r => [byteField(r.binding), utf16Field(r.package), utf16Field(r.key), r.kind, utf16Field(r.input)])}
});
// A locally compiled require callback tests the real wrapper without loading anything.
{
 const calls=[];new CommandMap('setoptions-local-require',{require:(p,n)=>{calls.push(p.GetArgument(n));p.Push(p.itemFactory.create('null'))},setOptions:(p,n)=>{throw Error('priority-4 map must override this old macro')}});
 Configuration.create('setoptions-local-require',{handler:{macro:['setoptions-local-require']}});
 const f=fixture({},['units','setoptions-local-require']);
 ok(f,'\\require[loose]{units}');assert.deepEqual(calls,['units']);assert.equal(f.jax.parseOptions.options.units.loose,true);
 ok(f,'\\require{units}');assert.deepEqual(calls,['units','units']);
 const original=f.jax.parseOptions.handlers.get('macro').lookup('Require');assert.equal(original.args.length,0);
}
{
 const f=fixture({},[],['setoptions','base']);ok(f,'\\setOptions{tagIndent=3em}');assert.equal(f.jax.parseOptions.options.tagIndent,'3em');
 const p=new TexParser('',{},f.jax.parseOptions);p.mml();
 for(const [input,pattern,flags]of [['/a/i','a','i'],['/a/bad',null,null],['/a\nb/',null,null],['//','(?:)','']]){
  const v=util.filterValue(p,'tex','numberPattern',input);if(pattern===null)assert.equal(v,input);else{assert.equal(v.source,pattern);assert.equal(v.flags,flags)}
 }
 assert.equal(util.filterValue(p,'tex','x',false),false);assert.throws(()=>util.filterValue(p,'tex','x','/a/ii'),SyntaxError);
}
{
 const f=fixture(),config=f.jax.parseOptions.options.setoptions;
 config.filterPackage=(p,e)=>e!=='skip';config.filterOption=null;config.filterValue=null;
 ok(f,'\\setOptions[skip]{tagIndent=x}');ok(f,'\\setOptions{}');
 const order=[];config.filterOption=(p,e,k)=>{order.push(k);return false};
 ok(f,'\\setOptions{20=x,tagIndent=abc,2=y,01=z,0=w}');
 assert.deepEqual(order,['0','2','20','tagIndent','01']);
}
{
 const f=fixture(),tokens=[],{NodeFactory}=load('input/tex/NodeFactory.js');
 f.jax.parseOptions.nodeFactory.set('token',(factory,kind,def,value)=>{const n=NodeFactory.createToken(factory,kind,def,value);tokens.push(n);return n});
 const actual=nodes(ok(f,'a\\setOptions{}b'),'mi');assert.equal(tokens.length,2);assert.equal(actual.length,2);
 assert.equal(actual[0]===tokens[0],true);assert.equal(actual[1]===tokens[1],true);
}
{
 const f=fixture({tagformat:{ref:t=>'custom:'+t},setoptions:{allowOptions:{tagformat:true}}},['tagformat']);
 for(const source of ['\\setOptions[tagformat]{ref=false}','\\setOptions[tagformat]{ref={}}']){
  f.jax.parseOptions.options.tagformat.ref=t=>'custom:'+t;
  assert.equal(f.jax.parseOptions.tags.formatRef('X'),'custom:X');
  ok(f,source);assert.deepEqual(f.jax.parseOptions.tags.formatRef('X'),['(','X',')']);
 }
}
{
 const {ParseUtil}=load('input/tex/ParseUtil.js');const f=fixture(),p=new TexParser('',{},f.jax.parseOptions);p.mml();
 for(const [limit,left,right,expected]of [[0,'','',''],[-1,'','',null],[2.9,'a','b','ab'],[2.9,'ab','c',null],[NaN,'a','b','ab'],[Infinity,'a','b','ab']]){
  p.options.maxBuffer=limit;
  if(expected===null)assert.throws(()=>ParseUtil.addArgs(p,left,right),e=>e.id==='MaxBufferSize');
  else assert.equal(ParseUtil.addArgs(p,left,right),expected);
 }
}
for(const limit of ['-1','0','NaN','Infinity']){
 const f=fixture({setoptions:{allowOptions:{tex:{maxBuffer:true}}}},['newcommand']);ok(f,'\\def\\empty{}');
 const source='\\setOptions{maxBuffer='+limit+'}\\empty';
 if(limit==='-1')assert.equal(f.compile(source).error,'MaxBufferSize');else ok(f,source);
}
// Counter comparison semantics differ for NaN: maxMacros accepts count <= limit.
for(const [limit,two,three]of [['2.9',true,false],['0',false,false],['-Infinity',false,false],['NaN',false,false],['Infinity',true,true]]){
 const f=fixture({},['newcommand']);ok(f,'\\def\\saved{x}');
 const first='\\setOptions{maxMacros='+limit+'}\\saved\\saved';
 if(two)assert.equal(nodes(ok(f,first),'mi').length,2);else error(f,first,'MaxMacroSub1');
 if(three)assert.equal(nodes(ok(f,'\\saved\\saved\\saved'),'mi').length,3);else error(f,'\\saved\\saved\\saved','MaxMacroSub1');
 assert.equal(nodes(ok(f,'\\setOptions{maxMacros=Infinity}\\saved'),'mi').length,1);
}
{
 const f=fixture();
 for(const source of ['\\Gamma','\\setOptions{mathStyle=ISO}\\Gamma'])assert.equal(nodes(ok(f,source),'mi')[0].attributes.get('mathvariant'),'normal');
}
console.log(JSON.stringify({cases,schemaFields:fields.length,scalarFields:scalar,presenceOnly:fields.length-scalar,directPresenceChecks:checks,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
