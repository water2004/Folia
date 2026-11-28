'use strict';
// Runs the pinned Source document with actual TeX parsing and actual SVG
// wrapper construction, recording method receivers, arguments and results.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');
const packages=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax');
const src=path.join(packages,'src');
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json')));
const sourceLock=JSON.parse(fs.readFileSync(path.join(__dirname,'common-layout-reference/profile-source-lock.json')));
const scopeFile=path.join(root,'tools/math-port/profiles/scope.json');
if(digest(scopeFile)!=='9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1')
  throw Error('MathJax scope lock changed');
if(digest(path.join(root,scope.profile.file))!==scope.profile.sha256)
  throw Error('MathJax profile lock changed');
if(sourceLock.scopeHash!==digest(path.join(root,'tools/math-port/profiles/scope.json')))
  throw Error('MathJax source dependency lock does not match the profile');
for(const dependency of sourceLock.dependencies){
  const file=dependency.file.startsWith('src/cjs/')?
    path.join(src,'cjs',dependency.file.slice('src/cjs/'.length)):
    path.join(packages,dependency.file);
  if(digest(file)!==dependency.sha256)throw Error(`Pinned Source dependency changed: ${dependency.file}`);
}
const load=file=>require(path.join(src,'cjs',file));
const {mathjax}=load('mathjax.js');
const {TeX}=load('input/tex.js');
const {SVG}=load('output/svg.js');
const {SvgWrapper}=load('output/svg/Wrapper.js');
const {SvgWrappers}=load('output/svg/Wrappers.js');
const {liteAdaptor}=load('adaptors/liteAdaptor.js');
const {RegisterHTMLHandler}=load('handlers/html.js');
require(path.join(src,'cjs/input/tex/base/BaseConfiguration.js'));
require(path.join(src,'cjs/input/tex/enclose/EncloseConfiguration.js'));
const {MathJaxNewcmFont}=require(path.join(packages,'mathjax-newcm-font/cjs/svg.js'));
const adaptor=liteAdaptor({fontSize:16});RegisterHTMLHandler(adaptor);
const input=new TeX({packages:['base','enclose']});
const output=new SVG({fontData:MathJaxNewcmFont,fontCache:'none',exFactor:.5,
  displayOverflow:'linebreak',linebreaks:{inline:true}});
for(const [name,exportName] of [
  ['bboldx','MathJaxBboldxFontExtension'],['bbm','MathJaxBbmFontExtension'],
  ['dsfont','MathJaxDsfontFontExtension']]){
  try{output.addExtension(require(path.join(packages,`mathjax-${name}-font-extension/cjs/svg.js`))[exportName]);}
  catch(error){if(error.code!=='MODULE_NOT_FOUND')throw error;}
}
const document=mathjax.document('',{InputJax:input,OutputJax:output});
function htmlContract(){
  let prototype=Object.getPrototypeOf(output);
  while(prototype&&!Object.prototype.hasOwnProperty.call(prototype,'html'))prototype=Object.getPrototypeOf(prototype);
  const descriptor=prototype&&Object.getOwnPropertyDescriptor(prototype,'html');
  const method=descriptor&&descriptor.value;
  if(typeof method!=='function')throw Error('Source CommonOutputJax.html is unavailable');
  const name=Object.getOwnPropertyDescriptor(method,'name');
  const length=Object.getOwnPropertyDescriptor(method,'length');
  const methodDescriptor={writable:descriptor.writable,enumerable:descriptor.enumerable,configurable:descriptor.configurable};
  const originalOwn=Object.getOwnPropertyDescriptor(adaptor,'node');
  const originalNode=adaptor.node;
  const marker={kind:'html-return'};
  let calledThis=null,captured=null;
  let defaultResult;
  let defaultError=null;
  let rawResult;
  let replaced=null;
  let exception=null;
  let result;
  try{
    const firstNode=function(...args){calledThis=this;captured=args;return marker;};
    firstNode.call=function(){throw Object.assign(new Error('function.call property must not be read'),{name:'HtmlCallPropertyError'});};
    adaptor.node=firstNode;
    try{defaultResult=output.html('mjx-probe');}
    catch(error){defaultError={name:error.name,message:String(error.message)};}
    const defaultArgs=captured;
    adaptor.node=function(...args){calledThis=this;captured=args;return marker;};
    rawResult=output.html('mjx-probe',null,null,null);
    adaptor.node=function(...args){replaced={thisValue:this,args};return marker;};
    const replacementResult=output.html('mjx-probe');
    const replacementObserved=replaced&&replaced.thisValue===adaptor&&replacementResult===marker;
    adaptor.node=function(){const error=Object.assign(new Error('node method test exception'),{name:'HtmlProbeError'});throw error;};
    try{output.html('mjx-probe');}catch(error){exception={name:error.name,message:String(error.message)};}
    result={
      name:method.name,length:method.length===4,
      prototypeDescriptor:!!methodDescriptor.writable&&!!methodDescriptor.enumerable&&!!methodDescriptor.configurable,
      nameDescriptor:!!name&&!name.writable&&!name.enumerable&&name.configurable,
      lengthDescriptor:!!length&&!length.writable&&!length.enumerable&&length.configurable,
      defaultArgs:Array.isArray(defaultArgs)&&defaultArgs.length===4&&defaultArgs[0]==='mjx-probe'&&
        defaultArgs[1]&&Object.getPrototypeOf(defaultArgs[1])===Object.prototype&&
        Array.isArray(defaultArgs[2])&&defaultArgs[3]===undefined,
      receiver:calledThis===adaptor,returnIdentity:defaultResult===marker,
      callPropertyIgnored:defaultError===null&&defaultResult===marker,
      rawNulls:rawResult===marker&&captured.length===4&&captured[1]===null&&captured[2]===null&&captured[3]===null,
      dynamicReplacement:replacementObserved&&replaced.args.length===4,
      exception:exception&&exception.name==='HtmlProbeError'&&exception.message==='node method test exception'
    };
  }finally{
    if(originalOwn)Object.defineProperty(adaptor,'node',originalOwn);
    else delete adaptor.node;
  }
  result.restored=adaptor.node===originalNode;
  return result;
}
const htmlContractResult=htmlContract();
const ids=new WeakMap();let nextId=1,active=null,failFirstSvg=true;
function identity(value){if(!value||typeof value!=='object'&&typeof value!=='function')return null;
  if(!ids.has(value))ids.set(value,nextId++);return ids.get(value);}
function summary(value,active=new WeakSet()){
  if(value===undefined)return {type:'undefined'};
  if(value===null)return null;
  if(typeof value==='string'||typeof value==='number'||typeof value==='boolean')return value;
  if(Array.isArray(value)){
    const result={type:'array',id:identity(value),length:value.length};
    if(active.has(value))return {...result,circular:true};
    active.add(value);
    result.items=Array.from({length:value.length},(_,index)=>
      Object.prototype.hasOwnProperty.call(value,index)?summary(value[index],active):{type:'hole'});
    active.delete(value);return result;
  }
  if(typeof value==='function')return {type:'function',id:identity(value)};
  const result={type:'object',id:identity(value)};
  if(active.has(value))return {...result,circular:true};
  active.add(value);
  result.fields=Object.fromEntries(Object.keys(value).map(key=>[key,summary(value[key],active)]));
  active.delete(value);return result;
}
function observe(target,key,name,shouldThrow=false){
  const original=target[key];
  if(typeof original!=='function')throw Error(`Source method missing: ${name}`);
  const observed=function(...args){
    const event={method:name,receiver:identity(this),args:args.map(value=>summary(value)),result:null};
    const isMenclosePrimitive=['line','rect','ellipse','path'].includes(args[0]);
    if(shouldThrow&&failFirstSvg&&isMenclosePrimitive){
      failFirstSvg=false;event.result={type:'throws',name:'ProbeInputError',message:'one-time SVG method test input'};
      if(active)active.events.push(event);throw Object.assign(new Error('one-time SVG method test input'),{name:'ProbeInputError'});
    }
    const result=original.apply(this,args);event.result=summary(result);
    if(active)active.events.push(event);
    return result;
  };
  if(name==='LiteAdaptor.setAttribute')observed.call=function(){
    throw Object.assign(new Error('setAttribute.call property must not be read'),{name:'SetAttributeCallPropertyError'});
  };
  target[key]=observed;
}
observe(SvgWrapper.prototype,'svg','SvgWrapper.svg');
observe(SvgWrapper.prototype,'fixed','SvgWrapper.fixed');
for(const name of ['line','box','ellipse','path','fill','arrow'])
  observe(SvgWrappers.menclose.prototype,name,`SvgMenclose.${name}`);
observe(output,'svg','SvgOutputJax.svg',true);
observe(output,'fixed','SvgOutputJax.fixed');
observe(adaptor,'append','LiteAdaptor.append');
observe(adaptor,'setAttribute','LiteAdaptor.setAttribute');
const notations=['top','right','bottom','left','actuarial','madruwb','updiagonalstrike',
  'downdiagonalstrike','horizontalstrike','verticalstrike','box','roundedbox','circle',
  'phasorangle','uparrow','downarrow','leftarrow','rightarrow','updownarrow',
  'leftrightarrow','updiagonalarrow','northeastarrow','southeastarrow','northwestarrow',
  'southwestarrow','northeastsouthwestarrow','northwestsoutheastarrow','longdiv','radical'];
(async()=>{
  const cases=[];
  const tests=[];
  for(const notation of notations){tests.push(notation);if(notation==='top')tests.push('top-retry');}
  for(const testName of tests){
    const notation=testName==='top-retry'?'top':testName;
    active={events:[]};let error=null;
    try{await document.convertPromise(`\\enclose{${notation}}{x}`,
      {display:false,em:16,ex:8,containerWidth:1280});}
    catch(caught){error={name:caught.name||'Error',message:String(caught.message||caught)};}
    cases.push({name:testName,error,events:active.events});active=null;
  }
  const outputPath=process.argv[2]||path.join(root,'build/math-port/menclose-source-observation.json');
  fs.mkdirSync(path.dirname(outputPath),{recursive:true});
  fs.writeFileSync(outputPath,JSON.stringify({schema:1,profile:scope.profile.sha256,
    sourceLock:sourceLock.corpusSha256,packageList:['base','enclose'],
    htmlContract:htmlContractResult,
    notationNames:Array.from(SvgWrappers.menclose.notations.keys()),cases},null,2)+'\n');
  process.stdout.write(`Observed ${cases.length} actual Source Menclose cases -> ${outputPath}\n`);
})().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
