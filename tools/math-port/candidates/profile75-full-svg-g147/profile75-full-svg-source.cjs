'use strict';
// Locked MathJax 4.1.3 full-profile SVG capture for all 75 cases. The
// continuous group uses one HTMLMathDocument; the fresh group gets one new
// TeX/SVG/document stack for every individual case.
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const cp=require('node:child_process');
const assert=require('node:assert/strict');

const root=path.resolve(__dirname,'../../../..');
const sourceRoot=path.join(root,'.reference/MathJax-src');
const packages=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax');
const cjsRoot=path.join(packages,'src/cjs');
const scopePath=path.join(root,'tools/math-port/profiles/scope.json');
const scope=JSON.parse(fs.readFileSync(scopePath,'utf8'));
const profilePath=path.join(root,scope.profile.file);
const corpusPath=path.join(root,'tools/math-port/common-layout-reference/profile-cases.json');
const lockPath=path.join(root,'tools/math-port/common-layout-reference/profile-source-lock.json');
const inputCapturePath=path.join(root,'build/math-port/common-layout-profile-root-locked.json');
const lock=JSON.parse(fs.readFileSync(lockPath,'utf8'));
const hashFile=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');

assert.equal(scope.upstream.version,'4.1.3');
assert.equal(scope.upstream.commit,'fb987178c2d279a99b0db5ea02e751691703955e');
assert.equal(cp.execFileSync('git',['rev-parse','HEAD'],{cwd:sourceRoot,encoding:'utf8'}).trim(),scope.upstream.commit);
assert.equal(hashFile(scopePath),lock.scopeHash);
assert.equal(hashFile(profilePath),scope.profile.sha256);
assert.equal(hashFile(corpusPath),lock.corpusSha256);
assert.equal(hashFile(inputCapturePath),'9f58d09411512b4682b1f0049e40d6a47844ddffb9794eda52e2700998e0d740');
assert.equal(hashFile(path.join(root,'tools/mathjax-bundle/package-lock.json')),
  scope.dependency_lock_sha256);
assert.equal(JSON.parse(fs.readFileSync(path.join(packages,'src/package.json'),'utf8')).version,'4.1.3');
for(const dependency of lock.dependencies)
  assert.equal(hashFile(path.join(packages,dependency.file)),dependency.sha256,dependency.file);
for(const packageName of ['mathjax-newcm-font','mathjax-bbm-font-extension',
  'mathjax-bboldx-font-extension','mathjax-dsfont-font-extension'])
  assert.equal(JSON.parse(fs.readFileSync(path.join(packages,packageName,'package.json'),'utf8')).version,
    '4.1.3',packageName);

const corpus=JSON.parse(fs.readFileSync(corpusPath,'utf8'));
assert.equal(corpus.schema,1);
assert.equal(corpus.cases.length,57);
const inputCapture=JSON.parse(fs.readFileSync(inputCapturePath,'utf8'));
assert.equal(inputCapture.version,scope.upstream.version);
assert.equal(inputCapture.scopeHash,hashFile(scopePath));
assert.equal(inputCapture.records.length,75);
assert.equal(inputCapture.configuredPackages.length,scope.profile.tex_packages.length);
assert.deepStrictEqual(inputCapture.configuredPackages,scope.profile.tex_packages);
for(const sourceLock of inputCapture.sourceLocks) {
  const scopeLock=scope.files.find(file=>file.source===sourceLock.file);
  assert.ok(scopeLock,`missing Source scope lock ${sourceLock.file}`);
  assert.equal(sourceLock.sha256,scopeLock.source_sha256,sourceLock.file);
  assert.equal(hashFile(path.join(sourceRoot,sourceLock.file)),sourceLock.sha256,sourceLock.file);
}
for(const [file,sha256] of Object.entries(inputCapture.cjsLocks))
  assert.equal(hashFile(path.join(packages,file)),sha256,file);
const cases=inputCapture.records.map(({name,tex,display,em,ex,width})=>({name,tex,display,em,ex,width}));
assert.equal(new Set(cases.map(item=>item.name)).size,75);
assert.deepStrictEqual(cases.slice(-corpus.cases.length).map(({name,tex,display,width})=>({
  name,tex,...(display?{display:true}:{}),...(width!==1280?{width}:{}),
})),corpus.cases.map(({name,tex,display,width})=>({
  name,tex,...(display?{display:true}:{}),...(width!==undefined?{width}:{}),
})), 'locked 75-case input capture does not end with the 57 profile corpus cases');
for(const item of cases) {
  assert.equal(typeof item.name,'string');assert.equal(typeof item.tex,'string');
  assert.equal(item.ex,item.em*.5,`${item.name}: public Renderer uses the Source ex factor`);
}
for(const item of corpus.cases)
  for(const feature of item.features)assert.ok(scope.profile.tex_packages.includes(feature),feature);
const load=file=>require(path.join(cjsRoot,file));
const {mathjax}=load('mathjax.js');
const {TeX}=load('input/tex.js');
const {SVG}=load('output/svg.js');
const {liteAdaptor}=load('adaptors/liteAdaptor.js');
const {RegisterHTMLHandler}=load('handlers/html.js');
const {MathJaxNewcmFont}=require(path.join(packages,'mathjax-newcm-font/cjs/svg.js'));
const profile=fs.readFileSync(profilePath,'utf8');
for(const match of profile.matchAll(/require\('@mathjax\/src\/js\/(input\/tex\/[^']+Configuration\.js)'\)/g))
  load(match[1]);
const adaptor=liteAdaptor({fontSize:16});
RegisterHTMLHandler(adaptor);
mathjax.asyncIsSynchronous=true;
mathjax.asyncLoad=name=>{
  const target=path.isAbsolute(name)?name:path.resolve(cjsRoot,name);
  assert.ok(target.startsWith(packages+path.sep),`Source requested an external module: ${name}`);
  return require(target);
};
const makeDocument=()=>{
  const input=new TeX({packages:scope.profile.tex_packages});
  const output=new SVG({...scope.profile.svg_options,fontData:MathJaxNewcmFont,
    dynamicPrefix:path.join(packages,'mathjax-newcm-font/cjs/svg/dynamic')});
  for(const [name,exportName] of [
    ['bboldx','MathJaxBboldxFontExtension'],['bbm','MathJaxBbmFontExtension'],
    ['dsfont','MathJaxDsfontFontExtension']])
    output.addExtension(require(path.join(packages,`mathjax-${name}-font-extension/cjs/svg.js`))[exportName]);
  return mathjax.document('',{InputJax:input,OutputJax:output});
};
const mmlError=svg=>svg!==null&&(svg.includes('data-mjx-error=')||svg.includes('data-mml-node="merror"'));
function capture(document,item,group,index) {
  const em=item.em??16,ex=item.ex??em*.5,width=item.width??1280,display=item.display??false;
  let svg=null,error=null;
  try{svg=adaptor.serializeXML(document.convert(item.tex,{display,em,ex,containerWidth:width}));}
  catch(caught){error={name:caught.name||caught.constructor.name,message:String(caught.message||caught)};}
  return{group,index,name:item.name,tex:item.tex,display,em,ex,width,svg,
    mmlError:!!mmlError(svg),error};
}
const records=[];
const continuousDocument=makeDocument();
cases.forEach((item,index)=>records.push(capture(continuousDocument,item,'continuous',index)));
cases.forEach((item,index)=>records.push(capture(makeDocument(),item,'fresh-session-per-case',index)));

// The closure is checked after all lazy font requests and all cases have run.
const actualDependencies=Object.keys(require.cache).filter(file=>file.startsWith(packages+path.sep)).sort().map(file=>({
  file:path.relative(packages,file).replaceAll(path.sep,'/'),sha256:hashFile(file)}));
assert.deepStrictEqual(actualDependencies,lock.dependencies,'full-profile CJS dependency closure changed');
const source={version:scope.upstream.version,commit:scope.upstream.commit,
  scopeSha256:hashFile(scopePath),profileSha256:hashFile(profilePath),
  sourceLockSha256:hashFile(lockPath),corpusSha256:hashFile(corpusPath),
  caseInventorySha256:hashFile(inputCapturePath),
  dependencyLockSha256:scope.dependency_lock_sha256,dependencies:actualDependencies,
  configuredPackages:[...scope.profile.tex_packages]};
const output={schema:1,source,groups:['continuous','fresh-session-per-case'],records};
const destination=process.argv[2]||path.join(__dirname,'profile75-full-svg-source.json');
fs.mkdirSync(path.dirname(destination),{recursive:true});
fs.writeFileSync(destination,JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify({destination,version:source.version,commit:source.commit,
  cases:cases.length,records:records.length,errors:records.filter(row=>row.error).length,
  mmlErrors:records.filter(row=>row.mmlError).length,corpusSha256:source.corpusSha256,
  continuousFreshDifferences:cases.reduce((count,_item,index)=>{
    const first=records[index],fresh=records[index+cases.length];
    return count+(JSON.stringify({svg:first.svg,mmlError:first.mmlError,error:first.error})===
      JSON.stringify({svg:fresh.svg,mmlError:fresh.mmlError,error:fresh.error})?0:1);
  },0)}));
