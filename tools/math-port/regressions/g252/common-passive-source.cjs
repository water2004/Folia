'use strict';
// Observe actual MathJax wrapper geometry and placement calls BEFORE SVG
// serialization. No native renderer or parsed SVG participates in this oracle.
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../../../..');
const sourceRoot=path.join(root,'.reference/MathJax-src');
const packages=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax');
const src=path.join(packages,'src');
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scopeFile=path.join(root,'tools/math-port/profiles/scope.json');
assert.equal(digest(scopeFile),'9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1');
const oldCapturePath=path.join(root,'build/math-port/common-layout-profile-root-locked.json');
assert.equal(digest(oldCapturePath),'9f58d09411512b4682b1f0049e40d6a47844ddffb9794eda52e2700998e0d740');
const baseObserverPath=path.join(root,'tools/math-port/common-layout-original-oracle.cjs');
assert.equal(digest(baseObserverPath),'aa317e4f5e6674e74033e780d9340c56e4b5e4902442e5f191615842476b9c6e');
const passiveOverlayPath=path.resolve(process.env.FOLIA_COMMON_PASSIVE_HOST_PATH||path.join(root,'tools/math-port/regressions/g252/passive-event-host.cjs'));
assert.equal(digest(passiveOverlayPath),'6dc22675a447e76cd14541ea25c9fc6fd56d2651b957cc280a365cf92aed5207');
const passiveRunnerPath=path.join(root,'tools/math-port/drafts/g213-maction-passive-host/source-passive-host-profile-75.cjs');
assert.equal(digest(passiveRunnerPath),'d0883c7f3ec6121b6ef907fb834cb7f2afffdc209283b65569e318ec2e97b6ee');
const passiveCapturePath=path.join(root,'tools/math-port/drafts/g213-maction-passive-host/source-passive-host-profile-75-capture.json');
assert.equal(digest(passiveCapturePath),'47ee5698ebcdaaf919cb7efe3c3aa6a87d0f19b51ff50d86361710e9f0f1224d');
const passiveListenersPath=path.join(root,'tools/math-port/drafts/g213-maction-passive-host/source-passive-host-profile-75-listeners.json');
assert.equal(digest(passiveListenersPath),'c366305d8361d70c95850c396153ba82fa631feff0d2ee5bfa49b96d67521ced');
const scope=JSON.parse(fs.readFileSync(scopeFile));
assert.equal(scope.upstream.version,'4.1.3');
assert.equal(scope.upstream.commit,'fb987178c2d279a99b0db5ea02e751691703955e');
assert.equal(digest(path.join(root,scope.profile.file)),scope.profile.sha256);
const cjsLocks={
  'src/cjs/output/svg.js':'1495e23a9da0bf6b0e8b44d71f54271fa12e902c11444e4f18cdd749ad3b800e',
  'src/cjs/output/common.js':'b8a58d983e465f21ed36574472140bfd65e2696bb72f6c5647e0a2ae47eb1bd9',
  'src/cjs/output/svg/Wrapper.js':'3ba6e35cf799f0cffe5c555be3f81c12d8a8f81fe36257685ddccb96f4671ee4',
  'src/cjs/output/common/Wrapper.js':'9e21f886b91624d170ff89df0eecef31e789ffecda001abc3e5649a38419412b',
  'src/cjs/output/svg/Wrappers/mfrac.js':'55068c351d419d68b459eca706653c1a95ad6947cbce406eeaabddd627a0a9a6',
  'src/cjs/output/common/Wrappers/mfrac.js':'4deba461704fca1492f630a9e0f712c6e02f4ab9abc9507290be05b2d4df06ea',
  'mathjax-newcm-font/cjs/svg.js':'709d6e85113ef44645ad802b8424508d5f836dd61e4d1f2797b5e011a9693734'
};
for(const [file,hash] of Object.entries(cjsLocks))assert.equal(digest(path.join(packages,file)),hash,file);
for(const packageName of ['src','mathjax-newcm-font','mathjax-bbm-font-extension','mathjax-bboldx-font-extension','mathjax-dsfont-font-extension'])
  assert.equal(JSON.parse(fs.readFileSync(path.join(packages,packageName,'package.json'))).version,'4.1.3');
const sourceLocks=[];
for(const folder of ['ts/output/common','ts/output/svg']){
  for(const relative of ['Wrapper.ts',...fs.readdirSync(path.join(sourceRoot,folder,'Wrappers')).filter(name=>name.endsWith('.ts')).map(name=>'Wrappers/'+name)]){
    const file=folder+'/'+relative;
    const lock=scope.files.find(entry=>entry.source===file);
    assert.ok(lock,`missing Source scope lock ${file}`);
    assert.equal(digest(path.join(sourceRoot,file)),lock.source_sha256,file);
    sourceLocks.push({file,sha256:lock.source_sha256});
  }
}
const extended=process.argv.includes('--profile-corpus');
let corpus=null;
if(extended){
  const file=path.join(root,'tools/math-port/common-layout-reference/profile-cases.json');
  corpus=JSON.parse(fs.readFileSync(file));
  assert.equal(corpus.schema,1);
  const lock=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/common-layout-reference/profile-source-lock.json')));
  assert.equal(lock.scopeHash,digest(scopeFile));
  assert.equal(lock.corpusSha256,digest(file));
  for(const input of lock.dependencies)
    assert.equal(digest(path.join(packages,input.file)),input.sha256,input.file);
}
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
const passiveHost=require(passiveOverlayPath);
passiveHost.install(adaptor);
RegisterHTMLHandler(adaptor);
const input=new TeX({packages:scope.profile.tex_packages});
const output=new SVG({fontData:MathJaxNewcmFont,fontCache:'none',exFactor:.5,
  displayOverflow:'linebreak',linebreaks:{inline:true},
  dynamicPrefix:path.join(packages,'mathjax-newcm-font/cjs/svg/dynamic')});
for(const [name,exportName] of [
  ['bboldx','MathJaxBboldxFontExtension'],['bbm','MathJaxBbmFontExtension'],['dsfont','MathJaxDsfontFontExtension']])
  output.addExtension(require(path.join(packages,`mathjax-${name}-font-extension/cjs/svg.js`))[exportName]);
const document=mathjax.document('',{InputJax:input,OutputJax:output});
const bboxFields=['w','h','d','scale','rscale','L','R','pwidth','ic','oc','sk','dx'];
const plain=value=>value===undefined?{$:'undefined'}:typeof value==='number'&&!Number.isFinite(value)?{$:String(value)}:
  typeof value==='number'&&Object.is(value,-0)?{$:'-0'}:Array.isArray(value)?value.map(plain):
  value&&typeof value==='object'?Object.fromEntries(Object.entries(value).map(([key,item])=>[key,plain(item)])):value;
const box=value=>Object.fromEntries(bboxFields.map(key=>[key,plain(value[key])]));
let active=null;
const instrument=!process.argv.includes('--no-trace');
function wrapperId(wrapper){
  if(!active.ids.has(wrapper))active.ids.set(wrapper,active.ids.size);
  return active.ids.get(wrapper);
}
mathjax.asyncIsSynchronous=true;
mathjax.asyncLoad=name=>{
  if(active)active.loads.push(String(name).replaceAll(packages,'@mathjax').replaceAll(path.sep,'/'));
  const target=path.isAbsolute(name)?name:path.resolve(src,'cjs',name);
  assert.ok(target.startsWith(packages+path.sep),'Source requested a module outside pinned packages');
  return require(target);
};
const originalPlace=SvgWrapper.prototype.place;
SvgWrapper.prototype.place=function(x,y,element){
  if(active&&instrument)active.calls.push({method:'place',wrapper:wrapperId(this),x:plain(x),y:plain(y),explicitElement:!!element,kind:element?adaptor.kind(element):null});
  return originalPlace.apply(this,arguments);
};
const originalPlaceChar=SvgWrapper.prototype.placeChar;
SvgWrapper.prototype.placeChar=function(n,x,y,parent,variant,buffer){
  const call=active&&instrument&&{method:'placeChar',wrapper:wrapperId(this),codepoint:n,x:plain(x),y:plain(y),variant:plain(variant),buffer:plain(buffer)};
  if(call)active.calls.push(call);
  const result=originalPlaceChar.apply(this,arguments);
  if(call)call.returnedWidth=plain(result);
  return result;
};
const originalSvg=output.svg;
output.svg=function(kind,attributes,children){
  if(active&&instrument)active.paint.push({kind,attributes:plain(attributes),childCount:children?.length||0});
  return originalSvg.apply(this,arguments);
};
const originalProcess=output.processMath;
output.processMath=function(wrapper,node){
  const result=originalProcess.apply(this,arguments);
  if(active&&instrument){
    active.root=wrapperId(wrapper);
    active.wrappers=Array.from(this.nodeMap.values(),item=>({id:wrapperId(item),kind:item.node.kind,
      children:item.childNodes.map(wrapperId),variant:plain(item.variant),bbox:box(item.getBBox()),outerBBox:box(item.getOuterBBox())}));
  }
  return result;
};
const cases=[
  ['plain','x+y',false,1280],['mathbb','\\mathbb{R}^n',false,1280],
  ['fraction-inline','\\frac{a+b}{c}',false,1280],['fraction-display','\\frac{a+b}{c}',true,1280],
  ['atop','{a+b\\atop c}',true,1280],['root','\\sqrt{x^2+1}',true,1280],
  ['indexed-root','\\sqrt[3]{x}',false,1280],['scripts','x_i^{j^2}',false,1280],
  ['sum-limits','\\sum_{i=1}^{n} i',true,1280],['stretch-fences','\\left(\\frac{1}{x}\\right)',true,1280],
  ['accent','\\widehat{abc}+\\overline{x}',false,1280],['matrix','\\begin{pmatrix}a&b\\\\c&d\\end{pmatrix}',true,1280],
  ['arrow','\\xrightarrow[a]{b}',true,1280],['text','\\text{hello }x',false,1280],
  ['color','\\color{red}{x+1}',false,1280],['bbox','\\bbox[2px,border:1px solid blue]{x}',false,1280],
  ['phantom','a\\phantom{x}+b',false,1280],['linebreak','a+b+c+d+e+f+g+h+i+j',true,100]
];
if(extended){
  assert.equal(new Set(corpus.cases.map(item=>item.name)).size,corpus.cases.length);
  for(const item of corpus.cases){
    assert.equal(typeof item.tex,'string');
    assert.ok(!cases.some(row=>row[0]===item.name));
    for(const feature of item.features)assert.ok(scope.profile.tex_packages.includes(feature),feature);
    cases.push([item.name,item.tex,item.display??false,item.width??1280]);
  }
  // Observe the actual compile boundary, before Common creates wrappers.
  // This extra observation is enabled only for the extended corpus, preserving
  // the already frozen 18-case baseline byte for byte.
  const tree=node=>node==null?null:{kind:node.kind,
    attributes:plain(node.attributes?.getAllAttributes?.()??null),
    properties:plain(node.getAllProperties?.()??null),
    ...(typeof node.getText==='function'?{text:node.getText()}:{}),
    children:(node.childNodes||[]).map(tree)};
  const compile=input.compile;
  input.compile=function(){
    const root=compile.apply(this,arguments);
    if(active)active.mml=tree(root);
    return root;
  };
  const warn=console.warn;
  console.warn=function(...args){
    if(active)active.warnings.push(args.map(value=>value instanceof Error?
      {name:value.name,message:value.message}:plain(value)));
    return warn.apply(this,args);
  };
}
(async()=>{
  const records=[];
  const passiveHostRecords=[];
  for(const [name,tex,display,width] of cases){
    active={ids:new Map(),calls:[],paint:[],loads:[],wrappers:[],root:null,warnings:[]};
    let svg,error=null;
    const hostRequest=passiveHost.beginRequest();
    try{svg=adaptor.serializeXML(await document.convertPromise(tex,{display,em:16,ex:8,containerWidth:width}));}
    catch(caught){error={name:caught.name||null,message:String(caught.message||caught)};}
    hostRequest.assertCallbacksRetained();
    passiveHostRecords.push({name,mode:'continuous',listeners:hostRequest.snapshot()});
    hostRequest.retire();
    const {calls,paint,loads,wrappers,root:layoutRoot}=active;
    records.push({name,tex,display,em:16,ex:8,width,layoutRoot,wrappers,calls,paint,loads,svg:svg||null,error,
      ...(extended?{mml:active.mml??null,warnings:active.warnings,
        intendedFeatures:corpus.cases.find(item=>item.name===name)?.features||[]}:{} )});
    active=null;
  }
  for(const [name,types] of [['action-toggle',['click']],['action-tooltip',['mouseover','mouseout']]]) {
    const eventRow=passiveHostRecords.find(row=>row.name===name);
    assert.deepEqual(eventRow.listeners.map(row=>row.type),types,`${name} listener order`);
    const result=records.find(row=>row.name===name);
    assert.equal(result.error,null,`${name} completed under passive host`);
    assert.equal(typeof result.svg,'string',`${name} SVG exists under passive host`);
  }
  assert.equal(passiveHostRecords.filter(row=>row.listeners.length).length,2,'only the two action records register events');
  const dependencies=Object.keys(require.cache).filter(file=>file.startsWith(packages+path.sep)).sort().map(file=>({
    file:path.relative(packages,file).replaceAll(path.sep,'/'),sha256:digest(file)}));
  if(extended){
    const lock=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/common-layout-reference/profile-source-lock.json')));
    assert.deepEqual(dependencies,lock.dependencies,'extended profile executable closure changed');
  }
  process.stdout.write(JSON.stringify({schema:1,version:'4.1.3',upstream:scope.upstream,scopeHash:digest(scopeFile),
    sourceLocks,cjsLocks,dependencies,records,
    witness:{observerSha256:digest(__filename),baseObserverSha256:digest(baseObserverPath),overlaySha256:digest(passiveOverlayPath),profileRunnerSha256:digest(passiveRunnerPath),profileCaptureSha256:digest(passiveCapturePath),listenerCaptureSha256:digest(passiveListenersPath),oldCaptureSha256:digest(oldCapturePath)},
    passiveHost:{records:passiveHostRecords},
    ...(extended?{corpusSha256:digest(path.join(root,'tools/math-port/common-layout-reference/profile-cases.json')),
      configuredPackages:scope.profile.tex_packages}:{} )},null,2)+'\n');
})().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
