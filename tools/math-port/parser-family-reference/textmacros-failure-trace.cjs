'use strict';
// Focused original-only trace for the currently failing native TextMacros,
// TextParser and ParseUtil paths. It creates real pinned TeX/MML peers and
// writes no gold or generated artifacts.
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const root=path.resolve(__dirname,'../../..');
const packageRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
const locks={
  'ts/input/tex/textmacros/TextMacrosConfiguration.ts':'5d712a805bfe3ff3db425f24c1887eab300782a8570363abbbe77641222faefc',
  'ts/input/tex/textmacros/TextMacrosMethods.ts':'15b736370eed35d3e65af14d772bfe1b1ad3287844ed44f908f6a9b14004d9c3',
  'ts/input/tex/textmacros/TextParser.ts':'538059c8482022c1a4bc6f8e763fdc21993fa6353f1956655ae11c9271788a30',
  'ts/input/tex/ParseUtil.ts':'b43646071997f88f27df51610c7972a8cb5791ea76194e7318d17d1a2f81f76a',
  'input/tex.js':'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
  'input/tex/textmacros/TextMacrosConfiguration.js':'5803611b187a6256cae6289cfc2f61fafa7f93395432d3a7b365f8089334b5cf',
  'input/tex/textmacros/TextMacrosMethods.js':'59e0abbeffeb9c859b2150d705abb1dc0a59a7ec71fb830ae6379d887c499817',
  'input/tex/textmacros/TextParser.js':'ec2fa0145f5e90ef231b023a38ed7d9c1064a8cc58a46bb683d46439bd8fb400',
  'input/tex/ParseUtil.js':'adafa18cefad39e8408bcc0e4eb7103dceb6d16c9e7ea2eb013f23602c3f8941'
};
for(const [file,expected] of Object.entries(locks)){
  const bytes=fs.readFileSync(file.startsWith('ts/')?
    path.join(root,'.reference/MathJax-src',file):
    path.join(packageRoot,'cjs',file));
  if(file.startsWith('ts/')){
    const item=scope.files.find(x=>x.source===file);
    if(!item)throw Error(`${file} missing from scope lock`);
    if(crypto.createHash('sha256').update(bytes).digest('hex')!==item.source_sha256)
      throw Error(`${file} pinned source hash mismatch`);
  }else if(crypto.createHash('sha256').update(bytes).digest('hex')!==expected)
    throw Error(`${file} CJS hash mismatch`);
}
const load=file=>require(path.join(packageRoot,'cjs',file));
const {TeX}=load('input/tex.js');
const TexParser=load('input/tex/TexParser.js').default;
const {TextParser}=load('input/tex/textmacros/TextParser.js');
const {ParseUtil}=load('input/tex/ParseUtil.js');
const {MmlFactory}=load('core/MmlTree/MmlFactory.js');
load('input/tex/textmacros/TextMacrosConfiguration.js');
const rows=[];
function plain(value,seen=new WeakSet()){
  if(value===undefined)return {$:'undefined'};
  if(value===null||typeof value==='string'||typeof value==='boolean')return value;
  if(typeof value==='number')return Number.isNaN(value)?{$:'NaN'}:Object.is(value,-0)?{$:'-0'}:value;
  if(typeof value==='function')return {$:'function',name:value.name||'',length:value.length};
  if(typeof value!=='object')return String(value);
  if(seen.has(value))return {$:'cycle'};seen.add(value);
  if(Array.isArray(value))return {$:'array',items:Array.from({length:value.length},(_,i)=>Object.hasOwn(value,i)?plain(value[i],seen):{$:'hole'})};
  if(value instanceof Map)return {$:'map',entries:[...value].map(([k,v])=>[plain(k,seen),plain(v,seen)])};
  const result={};for(const key of Reflect.ownKeys(value))result[typeof key==='symbol'?`symbol:${key.description||''}`:key]=plain(value[key],seen);
  return result;
}
function node(value){return !value?null:{kind:value.kind,attrs:plain(value.attributes?.getAllAttributes?.()||{}),properties:plain(value.getAllProperties?.()||{}),children:(value.childNodes||[]).map(node)};}
function capture(fn){try{return{value:plain(fn()),error:null};}catch(error){return{value:null,error:{name:error.name||error.constructor.name,id:error.id??null,message:String(error.message)}};}}
function session(packages=['base','textmacros']){const jax=new TeX({packages});jax.setMmlFactory(new MmlFactory());const parser=new TexParser('',{},jax.parseOptions);const data=jax.parseOptions.packageData.get('textmacros');if(data)data.parseOptions.nodeFactory.setMmlFactory(jax.mmlFactory);return{jax,parser,data};}
function record(name,fn){rows.push({name,...capture(fn)});}
record('configuration-preprocessor-owner-and-clear-order',()=>{
  const s=session(),data=s.data,inner=data.parseOptions,sharedFactory=inner.nodeFactory;
  inner.nodeLists.trace=[s.parser.create('node','mi',[],{},s.parser.create('text','x'))];
  inner.maxMacros=17;
  const result=s.jax.executeFilters(s.jax.preFilters,{}, {},s.jax.parseOptions);
  return {returnedSameParseOptions:result===s.jax.parseOptions,packageRecordStillSame:s.jax.parseOptions.packageData.get('textmacros')===data,
    nodeFactorySame:inner.nodeFactory===sharedFactory,nodeFactoryUsesJaxMmlFactory:inner.nodeFactory.mmlFactory===s.jax.mmlFactory,
    traceListAfterClear:plain(inner.nodeLists.trace),maxMacrosAfterClear:inner.maxMacros,parserCount:inner.parsers.length};
});
record('textparser-failing-native-method-branch-corpus',()=>{
  const inputs=['\\%','\\dagger',"\\'{e}",'\\frak{x}','\\quad','\\hspace{1em}','\\rule{1em}{2pt}',
    '\\ref{missing}','\\underline{x}','\\llap{x}','\\phantom{x}','\\smash{x}','\\mmlToken{mi}{x}',
    '\\emph emphatic','\\textbf{bold}','\\textit{italic}','\\texttt{mono}'];
  return inputs.map(source=>{const s=session();s.data.texParser=s.parser;const p=new TextParser(source,{},s.data.parseOptions);
    return {stage:'TextParser.mml',input:source,outcome:capture(()=>({tree:node(p.mml()),index:p.i,text:p.text,envStack:plain(p.envStack),remainingParsers:s.data.parseOptions.parsers.length}))};});
});
record('parseutil-internalmath-failure-inputs-and-outcomes',()=>{
  const s=session(['base']);const inputs=['','plain text','a $x$ b','x\\(y\\)z','x\\ref{tag}','x\\eqref {tag}',
    'a\\U{1F600}','\\$x\\$','\\{x\\}','\\\\x','a { b }','bad $x','\\mbox{ab}'];
  return inputs.map(input=>({stage:'ParseUtil.internalMath',input,outcome:capture(()=>({children:ParseUtil.internalMath(s.parser,input).map(node),macroCount:s.parser.macroCount}))}));
});
record('parseutil-internalmath-live-callback-receiver-args-result',()=>{
  const s=session(['base']),calls=[];const options=s.parser.configuration.options;const old=options.internalMath;
  options.internalMath=function(...args){calls.push({thisIsOptions:this===options,args:args.map((value,index)=>index===0?value===s.parser:plain(value))});return ['callback-result'];};
  const outcome=capture(()=>ParseUtil.internalMath(s.parser,' a   b ',0,'italic'));
  options.internalMath=old;
  return {outcome,calls,postRestoreFunctionSame:options.internalMath===old};
});
process.stdout.write(JSON.stringify({source:'pinned MathJax 4.1.3 CJS runtime; actual TeX/ParseOptions/TextParser/MML owners',observations:rows},null,2)+'\n');
