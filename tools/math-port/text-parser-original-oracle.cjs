// Actual pinned TextParser CJS observations with real TeX, ParseOptions, Stack,
// NodeFactory and MmlFactory peers. No native implementation is loaded here.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'), packageRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const scopeBytes=fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'));
const scopeHash=crypto.createHash('sha256').update(scopeBytes).digest('hex');
const scope=JSON.parse(scopeBytes.toString('utf8'));
const locks={
  'ts/input/tex/textmacros/TextParser.ts':'ec2fa0145f5e90ef231b023a38ed7d9c1064a8cc58a46bb683d46439bd8fb400',
  'ts/input/tex/ParseUtil.ts':'adafa18cefad39e8408bcc0e4eb7103dceb6d16c9e7ea2eb013f23602c3f8941',
  'ts/input/tex/TexParser.ts':'106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
  'input/tex/textmacros/TextParser.js':'ec2fa0145f5e90ef231b023a38ed7d9c1064a8cc58a46bb683d46439bd8fb400',
  'input/tex/ParseUtil.js':'adafa18cefad39e8408bcc0e4eb7103dceb6d16c9e7ea2eb013f23602c3f8941',
  'input/tex/TexParser.js':'106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
  'input/tex.js':'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
  'input/tex/textmacros/TextMacrosConfiguration.js':'5803611b187a6256cae6289cfc2f61fafa7f93395432d3a7b365f8089334b5cf',
  'input/tex/StackItem.js':'53baa91d7c27475a91a9b6993235071ab32b04a29736c5920bdda2836ab5a91b',
  'input/tex/base/BaseItems.js':'5310d0429b7b62d405ae9b93c8d21ad17b787e5eb3fd1f01439c62f2094f7d1a',
  'input/tex/NodeUtil.js':'4a55841fd22167d25f505e8f41adea072715ef024e00ea5ac6df3f199f2cb809',
  'core/MmlTree/MmlFactory.js':'d0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'core/MmlTree/MmlNode.js':'86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
  'core/Tree/Factory.js':'739db2e5760c29d7bb4f1bc7baa08ff8d727f4d117522aff5ede017f812cffef',
};
const modules=[];
for(const [file,expected] of Object.entries(locks)){
  const cjsFile=file.startsWith('ts/')?file.slice(3).replace(/\.ts$/,'.js'):file;
  const bytes=fs.readFileSync(path.join(packageRoot,'cjs',cjsFile));
  const hash=crypto.createHash('sha256').update(bytes).digest('hex');assert.equal(hash,expected,`${file} CJS hash`);
  if(file.startsWith('ts/')){
    const lock=scope.files.find(x=>x.source===file);assert.ok(lock,`${file} absent from scope`);
    const original=fs.readFileSync(path.join(root,'.reference/MathJax-src',file));
    assert.equal(crypto.createHash('sha256').update(original).digest('hex'),lock.source_sha256,`${file} Source hash`);
    const installed=fs.readFileSync(path.join(packageRoot,file));
    const normalize=bytes=>bytes.toString('utf8').replaceAll('\r\n','\n');
    assert.equal(normalize(installed),normalize(original),`${file} installed TS differs beyond line endings`);
    const crlf=(bytes)=>(bytes.toString('utf8').match(/\r\n/g)||[]).length;
    modules.push({source:file,source_sha256:lock.source_sha256,installed_ts_sha256:crypto.createHash('sha256').update(installed).digest('hex'),
      normalized_ts_sha256:crypto.createHash('sha256').update(normalize(installed)).digest('hex'),source_crlf_lines:crlf(original),installed_crlf_lines:crlf(installed),cjs:cjsFile,cjs_sha256:hash});
  }else modules.push({cjs:file,cjs_sha256:hash});
}
const load=file=>require(path.join(packageRoot,'cjs',file));
const {TeX}=load('input/tex.js'),TexParser=load('input/tex/TexParser.js').default;
const {ParseUtil}=load('input/tex/ParseUtil.js'),{TextParser}=load('input/tex/textmacros/TextParser.js');
const {MmlFactory}=load('core/MmlTree/MmlFactory.js');
// Importing the actual package registers its Source Configuration before TeX is made.
load('input/tex/textmacros/TextMacrosConfiguration.js');
const rows=[];
function plain(v,seen=new WeakSet()){
  if(v===undefined)return{$:'undefined'};if(v===null||typeof v==='string'||typeof v==='boolean')return v;
  if(typeof v==='number')return Number.isNaN(v)?{$:'NaN'}:Object.is(v,-0)?{$:'-0'}:v;
  if(typeof v==='function')return{$:'function',name:v.name||'',length:v.length};if(typeof v!=='object')return String(v);
  if(seen.has(v))return{$:'cycle'};seen.add(v);
  if(Array.isArray(v))return Array.from({length:v.length},(_,i)=>Object.hasOwn(v,i)?plain(v[i],seen):{$:'hole'});
  if(v instanceof Map)return [...v].map(([k,x])=>[plain(k,seen),plain(x,seen)]);
  const out={};for(const k of Reflect.ownKeys(v))out[typeof k==='symbol'?`symbol:${k.description||''}`:k]=plain(v[k],seen);return out;
}
function err(fn){try{return{result:plain(fn()),error:null};}catch(e){return{result:null,error:{name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)}};}}
function record(label,fn){rows.push({label,...err(fn)});}
function jax(){const value=new TeX({packages:['base','textmacros']});value.setMmlFactory(new MmlFactory());return value;}
function node(n){return !n?null:{kind:n.kind,attrs:plain(n.attributes?.getAllAttributes?.()||{}),properties:plain(n.getAllProperties?.()||{}),children:(n.childNodes||[]).map(node)};}
function session(){
  const j=jax(),parser=new TexParser('',{},j.parseOptions),pkg=j.parseOptions.packageData.get('textmacros');
  pkg.parseOptions.nodeFactory.setMmlFactory(j.mmlFactory);
  return{jax:j,parser,pkg,configuration:pkg.parseOptions};
}
record('textparser-class-descriptors-and-texparser-inheritance',()=>({
  class:{name:TextParser.name,length:TextParser.length,ownKeys:Reflect.ownKeys(TextParser).map(String),prototypeDescriptor:{writable:Object.getOwnPropertyDescriptor(TextParser,'prototype').writable,enumerable:Object.getOwnPropertyDescriptor(TextParser,'prototype').enumerable,configurable:Object.getOwnPropertyDescriptor(TextParser,'prototype').configurable}},
  prototypeKeys:Reflect.ownKeys(TextParser.prototype).map(String),parent: Object.getPrototypeOf(TextParser.prototype)===TexParser.prototype,
  methods:['texParser','tags','constructor','mml','copyLists','Parse','saveText','Push','PushMath','addAttributes','ParseTextArg','ParseArg','Error'].map(name=>{
    const d=Object.getOwnPropertyDescriptor(TextParser.prototype,name);return{name,enumerable:d?.enumerable,configurable:d?.configurable,
      get:d?.get?{name:d.get.name,length:d.get.length}:null,value:typeof d?.value==='function'?{name:d.value.name,length:d.value.length}:null};
  })
}));
record('actual-textmacros-configuration-and-internalmath-callback',()=>{
  const s=session(),before={keys:Object.keys(s.jax.parseOptions.packageData.get('textmacros')),sharedOptions:s.pkg.parseOptions.options===s.jax.parseOptions.options,
    sharedPackageMap:s.pkg.parseOptions.packageData===s.jax.parseOptions.packageData};
  const callback=s.pkg.parseOptions.options.internalMath;
  const result=ParseUtil.internalMath(s.parser,'plain text',2,'bold');
  return{before,callback:{name:callback.name,length:callback.length},output:result.map(node),texParserIsMain:s.pkg.texParser===s.parser,innerParserCount:s.configuration.parsers.length,
    optionsInternalMathSame:s.configuration.options.internalMath===s.jax.parseOptions.options.internalMath};
});
record('internalmath-real-segments-and-source-error',()=>{
  const out=[];for(const source of ['alpha beta','a\\(x\\)z','$x$','\\U{1F600}','\\ref{label}']){
    const s=session();out.push({source,result:err(()=>ParseUtil.internalMath(s.parser,source).map(node)),registeredMainParser:s.pkg.texParser===s.parser,
      innerParserCount:s.configuration.parsers.length,innerRoot:node(s.configuration.root)});
  }return out;
});
record('textparser-mml-level-empty-single-and-multiple',()=>{
  const out=[];for(const [label,source,level] of [['empty','',undefined],['one','abc',undefined],['multiple','ab',undefined],['level-number','abc',2],['level-string','abc','-1']]){
    const s=session(),p=new TextParser(source,{},s.configuration,level);s.pkg.texParser=s.parser;
    const result=err(()=>node(p.mml()));out.push({label,result,parserCount:s.configuration.parsers.length,
      nodeListKeys:Object.keys(s.configuration.nodeLists)});
  }return out;
});
record('textparser-saveText-and-push-token-attributes',()=>{
  const s=session(),p=new TextParser('',{mathcolor:'red',mathsize:'2em',mathvariant:'bold'},s.configuration);s.pkg.texParser=s.parser;
  const token=p.create('node','mi',[],{},p.create('text','x'));p.text=' text ';
  p.Push(token);return{text:p.text,nodes:p.nodes.map(node),top:s.configuration.parser.stack.Top().kind};
});
record('textparser-Push-stop-style-and-other-base-stackitem',()=>{
  const s=session(),p=new TextParser('',{color:'blue',mathcolor:'red'},s.configuration);s.pkg.texParser=s.parser;
  p.text='a';const stop=s.configuration.itemFactory.create('stop');p.Push(stop);const afterStop={text:p.text,top:p.stack.Top().kind,height:p.stack.height};
  const style=s.configuration.itemFactory.create('style');p.Push(style);const afterStyle={environment:plain(p.stack.env),nodes:p.nodes.map(node)};
  return{afterStop,afterStyle,afterStyleStack:{height:p.stack.height,top:p.stack.Top().kind}};
});
record('textparser-PushMath-wrapper-attributes-and-inferred-row',()=>{
  const s=session(),p=new TextParser('',{mathsize:'2em',mathcolor:'red'},s.configuration);s.pkg.texParser=s.parser;
  const token=p.create('node','mi',[],{},p.create('text','x'));
  const layout=p.create('node','mrow',[],{},p.create('text','y'));
  const inferred=p.create('node','mrow',[p.create('node','mi',[],{},p.create('text','z'))]);
  p.PushMath(token);p.PushMath(layout);p.PushMath(inferred);
  return{nodes:p.nodes.map(node),explicit:token.attributes.getExplicit('mathcolor')};
});
record('textparser-copyLists-transfers-real-child-array-membership',()=>{
  const s=session(),main=s.jax.parseOptions,inner=s.configuration;
  const marker=s.parser.create('node','mi',[],{},s.parser.create('text','x'));
  inner.addNode('custom',marker);inner.addNode('mi',marker);s.pkg.texParser=s.parser;
  const p=new TextParser('',{},inner);const before={innerKeys:Object.keys(inner.nodeLists),mainKeys:Object.keys(main.nodeLists)};
  p.copyLists();return{before,afterInnerKeys:Object.keys(inner.nodeLists),afterMainKeys:Object.keys(main.nodeLists),
    sameNode:main.nodeLists.custom?.[0]===marker,mainMi:main.nodeLists.mi?.length};
});
record('textparser-copyLists-object-entries-values-snapshot-before-addNode',()=>{
  const s=session(),p=new TextParser('',{},s.configuration);s.pkg.texParser=s.parser;
  const first=s.parser.create('node','mi',[],{},s.parser.create('text','a'));
  const captured=s.parser.create('node','mi',[],{},s.parser.create('text','b'));
  const replacement=s.parser.create('node','mi',[],{},s.parser.create('text','c'));
  let current=captured;const gets=[];const added=[];
  const lists={alpha:[first]};Object.defineProperty(lists,'beta',{enumerable:true,configurable:true,get(){gets.push('beta');return current;}});
  Object.defineProperty(lists,'alpha',{enumerable:true,configurable:true,get(){gets.push('alpha');return [first];}});
  s.configuration.nodeLists=lists;
  const target=s.jax.parseOptions,original=target.addNode;
  target.addNode=function(name,node){added.push({name,which:node===first?'first':node===captured?'captured':'replacement'});if(name==='alpha')current=[replacement];return original.call(this,name,node);};
  try{p.copyLists();}finally{target.addNode=original;}
  return{gets,added,mainKeys:Object.keys(target.nodeLists),innerKeys:Object.keys(s.configuration.nodeLists)};
});
record('textparser-copyLists-array-iterator-reads-live-length',()=>{
  const s=session(),p=new TextParser('',{},s.configuration);s.pkg.texParser=s.parser;
  const first=s.parser.create('node','mi',[],{},s.parser.create('text','a'));
  const appended=s.parser.create('node','mi',[],{},s.parser.create('text','b'));
  const list=[];Object.defineProperty(list,0,{enumerable:true,configurable:true,get(){list.push(appended);return first;}});list.length=1;
  s.configuration.nodeLists={live:list};const added=[];const target=s.jax.parseOptions,original=target.addNode;
  target.addNode=function(name,node){added.push(node===first?'first':'appended');return original.call(this,name,node);};
  try{p.copyLists();}finally{target.addNode=original;}
  return{added,sourceLengthAfter:list.length,targetLength:target.nodeLists.live?.length};
});
record('textparser-ParseArg-and-ParseTextArg-real-argument-consumption',()=>{
  const out=[];for(const method of ['ParseArg','ParseTextArg']){
    const s=session(),p=new TextParser('',{},s.configuration);s.pkg.texParser=s.parser;p.string='{abc}tail';p.i=0;
    out.push({method,result:err(()=>node(method==='ParseArg'?p.ParseArg('probe'):p.ParseTextArg('probe',{mathcolor:'blue'}))),index:p.i,
      nestedParserCount:s.configuration.parsers.length,mainParserCount:s.jax.parseOptions.parsers.length});
  }return out;
});
record('textparser-Error-real-TexError-substitution',()=>{
  const s=session(),p=new TextParser('',{},s.configuration);s.pkg.texParser=s.parser;
  return{single:err(()=>p.Error('Demo','bad %1',['x'])),missing:err(()=>p.Error('Demo','bad %1',[]))};
});

const output={schema:1,source:{path:'ts/input/tex/textmacros/TextParser.ts',sha256:locks['ts/input/tex/textmacros/TextParser.ts']},
  scope_sha256:scopeHash,modules,observations:rows};
const outPath=path.join(root,'tools/math-port/profiles/evidence/text-parser-original-source.json');
const serialized=JSON.stringify(output,null,2)+'\n';
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(outPath,'utf8').replaceAll('\r\n','\n'),serialized);
else fs.writeFileSync(outPath,serialized);
console.log(JSON.stringify({observations:rows.length,modules:modules.length,output:path.relative(root,outPath)}));
