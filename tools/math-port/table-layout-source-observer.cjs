'use strict';
// Runs each table-focused input through the pinned MathJax SVG output and
// records live wrapper, placement, paint, MathML, and serialized DOM results.
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const packages=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax');
const src=path.join(packages,'src');
const scopePath=path.join(root,'tools/math-port/profiles/scope.json');
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope=JSON.parse(fs.readFileSync(scopePath,'utf8'));
assert.equal(digest(path.join(root,scope.profile.file)),scope.profile.sha256);
const casesPath=path.join(__dirname,'table-layout-cases.json');
const corpus=JSON.parse(fs.readFileSync(casesPath,'utf8'));
assert.equal(corpus.schema,1);
assert.ok(corpus.cases.length>0);
const load=file=>require(path.join(src,'cjs',file));
const {mathjax}=load('mathjax.js');
const {TeX}=load('input/tex.js');
const {SVG}=load('output/svg.js');
const {SvgWrapper}=load('output/svg/Wrapper.js');
const {liteAdaptor}=load('adaptors/liteAdaptor.js');
const {RegisterHTMLHandler}=load('handlers/html.js');
const {MathJaxNewcmFont}=require(path.join(packages,'mathjax-newcm-font/cjs/svg.js'));
const profile=fs.readFileSync(path.join(root,scope.profile.file),'utf8');
for(const match of profile.matchAll(/require\('@mathjax\/src\/js\/(input\/tex\/[^']+Configuration\.js)'\)/g))load(match[1]);
const adaptor=liteAdaptor({fontSize:16});
RegisterHTMLHandler(adaptor);
const input=new TeX({packages:scope.profile.tex_packages});
const output=new SVG({fontData:MathJaxNewcmFont,fontCache:'none',exFactor:.5,
  displayOverflow:'linebreak',linebreaks:{inline:true},
  dynamicPrefix:path.join(packages,'mathjax-newcm-font/cjs/svg/dynamic')});
for(const [name,exportName] of [
  ['bboldx','MathJaxBboldxFontExtension'],['bbm','MathJaxBbmFontExtension'],
  ['dsfont','MathJaxDsfontFontExtension']])
  output.addExtension(require(path.join(packages,`mathjax-${name}-font-extension/cjs/svg.js`))[exportName]);
const document=mathjax.document('',{InputJax:input,OutputJax:output});
const bboxFields=['w','h','d','scale','rscale','L','R','pwidth','ic','oc','sk','dx'];
const plain=value=>value===undefined?{$:'undefined'}:
  typeof value==='number'&&!Number.isFinite(value)?{$:String(value)}:
  typeof value==='number'&&Object.is(value,-0)?{$:'-0'}:
  Array.isArray(value)?value.map(plain):
  value&&typeof value==='object'?Object.fromEntries(Object.entries(value).map(([key,item])=>[key,plain(item)])):value;
const box=value=>Object.fromEntries(bboxFields.map(key=>[key,plain(value[key])]));
let active=null;
function wrapperId(wrapper){
  if(!active.ids.has(wrapper))active.ids.set(wrapper,active.ids.size);
  return active.ids.get(wrapper);
}
const originalPlace=SvgWrapper.prototype.place;
SvgWrapper.prototype.place=function(x,y,element){
  if(active)active.calls.push({method:'place',wrapper:wrapperId(this),x:plain(x),y:plain(y),
    explicitElement:!!element,kind:element?adaptor.kind(element):null});
  return originalPlace.apply(this,arguments);
};
const originalPlaceChar=SvgWrapper.prototype.placeChar;
SvgWrapper.prototype.placeChar=function(n,x,y,parent,variant,buffer){
  const call=active&&{method:'placeChar',wrapper:wrapperId(this),codepoint:n,x:plain(x),y:plain(y),
    variant:plain(variant),buffer:plain(buffer)};
  if(call)active.calls.push(call);
  const result=originalPlaceChar.apply(this,arguments);
  if(call)call.returnedWidth=plain(result);
  return result;
};
const originalSvg=output.svg;
output.svg=function(kind,attributes,children){
  if(active)active.paint.push({kind,attributes:plain(attributes),childCount:children?.length||0});
  return originalSvg.apply(this,arguments);
};
const originalProcess=output.processMath;
output.processMath=function(wrapper,node){
  const result=originalProcess.apply(this,arguments);
  if(active){
    active.root=wrapperId(wrapper);
    active.wrappers=Array.from(this.nodeMap.values(),item=>({id:wrapperId(item),kind:item.node.kind,
      children:item.childNodes.map(wrapperId),variant:plain(item.variant),bbox:box(item.getBBox()),
      outerBBox:box(item.getOuterBBox())}));
  }
  return result;
};
const compile=input.compile;
input.compile=function(){
  const rootNode=compile.apply(this,arguments);
  if(active)active.mml=tree(rootNode);
  return rootNode;
};
function tree(node){
  return node==null?null:{kind:node.kind,
    attributes:plain(node.attributes?.getAllAttributes?.()??null),
    properties:plain(node.getAllProperties?.()??null),
    ...(typeof node.getText==='function'?{text:plain(node.getText())}:{}),
    children:(node.childNodes||[]).map(tree)};
}
mathjax.asyncIsSynchronous=true;
mathjax.asyncLoad=name=>{
  if(active)active.loads.push(String(name).replaceAll(packages,'@mathjax').replaceAll(path.sep,'/'));
  const target=path.isAbsolute(name)?name:path.resolve(src,'cjs',name);
  assert.ok(target.startsWith(packages+path.sep),'Source requested a module outside the pinned packages');
  return require(target);
};
const records=[];
(async()=>{
  for(const item of corpus.cases){
    active={ids:new Map(),calls:[],paint:[],loads:[],wrappers:[],root:null,mml:null};
    let svg=null,error=null;
    try{svg=adaptor.serializeXML(await document.convertPromise(item.tex,{
      display:item.display??false,em:item.em??16,ex:item.ex??8,containerWidth:item.width??1280}));}
    catch(caught){error={name:caught.name||null,message:String(caught.message||caught)};}
    records.push({name:item.name,tex:item.tex,display:item.display??false,
      em:item.em??16,ex:item.ex??8,width:item.width??1280,layoutRoot:active.root,
      wrappers:active.wrappers,calls:active.calls,paint:active.paint,loads:active.loads,
      svg,error,mml:active.mml,warnings:[],intendedFeatures:[]});
    active=null;
  }
  const dependencies=Object.keys(require.cache).filter(file=>file.startsWith(packages+path.sep)).sort().map(file=>({
    file:path.relative(packages,file).replaceAll(path.sep,'/'),sha256:digest(file)}));
  process.stdout.write(JSON.stringify({schema:1,version:scope.upstream.version,
    scopeHash:digest(scopePath),corpusSha256:digest(casesPath),configuredPackages:scope.profile.tex_packages,
    dependencies,records},null,2)+'\n');
})().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
