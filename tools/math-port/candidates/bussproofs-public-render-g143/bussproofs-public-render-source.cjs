'use strict';
// MathJax 4.1.3 full-profile SVG source witness using one TeX Jax, one SVG Jax,
// one HTMLMathDocument, and sequential calls to that document's convert().
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const assert=require('node:assert/strict');
const cp=require('node:child_process');

const root=path.resolve(__dirname,'../../../..');
const packages=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax');
const src=path.join(packages,'src');
const cjs=path.join(src,'cjs');
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scopePath=path.join(root,'tools/math-port/profiles/scope.json');
const scope=JSON.parse(fs.readFileSync(scopePath,'utf8'));
assert.equal(scope.upstream.version,'4.1.3');
assert.equal(scope.upstream.commit,'fb987178c2d279a99b0db5ea02e751691703955e');
assert.equal(cp.execFileSync('git',['rev-parse','HEAD'],{cwd:path.join(root,'.reference/MathJax-src'),encoding:'utf8'}).trim(),scope.upstream.commit);
assert.equal(digest(scopePath),'9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1');
assert.equal(digest(path.join(root,scope.profile.file)),scope.profile.sha256);
const dependencyLockPath=path.join(root,'tools/mathjax-bundle/package-lock.json');
const dependencyLock=JSON.parse(fs.readFileSync(dependencyLockPath,'utf8'));
assert.equal(digest(dependencyLockPath),scope.dependency_lock_sha256);
assert.equal(require(path.join(packages,'src/package.json')).version,scope.upstream.version);
for(const font of scope.font_packages){
  const actual=require(path.join(root,'tools/mathjax-bundle/node_modules',font.package,'package.json'));
  assert.equal(actual.version,font.version,font.package);
}
const bussproofsLock=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/locks/bussproofs-source-lock.json'),'utf8'));
const sourceRoot=path.join(root,'.reference/MathJax-src');
for(const file of bussproofsLock.files){
  assert.equal(digest(path.join(sourceRoot,file.ts)),file.tsSha256,file.ts);
  assert.equal(digest(path.join(src,file.cjs)),file.cjsSha256,file.cjs);
}
const load=file=>require(path.join(cjs,file));
const {mathjax}=load('mathjax.js');
const {TeX}=load('input/tex.js');
const {SVG}=load('output/svg.js');
const {liteAdaptor}=load('adaptors/liteAdaptor.js');
const {RegisterHTMLHandler}=load('handlers/html.js');
const {MathJaxNewcmFont}=require(path.join(packages,'mathjax-newcm-font/cjs/svg.js'));
const profile=fs.readFileSync(path.join(root,scope.profile.file),'utf8');
for(const match of profile.matchAll(/require\('@mathjax\/src\/js\/(input\/tex\/[^']+Configuration\.js)'\)/g))
  load(match[1]);
const adaptor=liteAdaptor({fontSize:16});
RegisterHTMLHandler(adaptor);
const input=new TeX({packages:scope.profile.tex_packages});
const output=new SVG({...scope.profile.svg_options,fontData:MathJaxNewcmFont,
  dynamicPrefix:path.join(packages,'mathjax-newcm-font/cjs/svg/dynamic')});
for(const [name,field] of [['bboldx','MathJaxBboldxFontExtension'],['bbm','MathJaxBbmFontExtension'],
  ['dsfont','MathJaxDsfontFontExtension']])
  output.addExtension(require(path.join(packages,`mathjax-${name}-font-extension/cjs/svg.js`))[field]);
mathjax.asyncIsSynchronous=true;
mathjax.asyncLoad=name=>require(path.isAbsolute(name)?name:path.resolve(cjs,name));
const document=mathjax.document('',{InputJax:input,OutputJax:output});
const casesPath=path.join(__dirname,'bussproofs-public-render-cases.json');
const corpus=JSON.parse(fs.readFileSync(casesPath,'utf8'));
const mmlError=svg=>svg.includes('data-mjx-error=')||svg.includes('data-mml-node="merror"');
const records=corpus.cases.map(item=>{
  let svg=null,error=null;
  try{svg=adaptor.serializeXML(document.convert(item.tex,{display:item.display,
    em:item.em,ex:item.em*.5,containerWidth:item.width}));}
  catch(caught){error={name:caught.name||caught.constructor.name,message:String(caught.message)};}
  const containsMmlError=svg===null?false:mmlError(svg);
  assert.notEqual(svg,null,`${item.name} must return SVG`);
  assert.match(svg,/<svg\b/,`${item.name} emits an SVG element`);
  if(item.name==='ordinary-after-proof'){
    assert.match(svg,/data-c="1D465"/,'ordinary x is Source math-italic U+1D465');
    assert.match(svg,/data-c="31"/,'ordinary 1 is present after the proof');
  }
  assert.equal(containsMmlError,false,`${item.name} must not be converted into an MML error`);
  assert.equal(error,null,`${item.name} must not throw`);
  return{name:item.name,tex:item.tex,svg,mmlError:containsMmlError,error};
});
const outputValue={schema:1,source:{version:scope.upstream.version,commit:scope.upstream.commit,
  scopeSha256:digest(scopePath),profileSha256:digest(path.join(root,scope.profile.file)),
  dependencyLockSha256:digest(dependencyLockPath),bussproofsLockSha256:digest(path.join(root,'tools/math-port/locks/bussproofs-source-lock.json')),
  bussproofsCjs:Object.fromEntries(bussproofsLock.files.map(file=>[file.cjs,file.cjsSha256]))},
  corpusSha256:digest(casesPath),records};
const destination=process.argv[2]||path.join(__dirname,'bussproofs-public-render-source.json');
fs.writeFileSync(destination,JSON.stringify(outputValue,null,2)+'\n');
console.log(JSON.stringify({destination,version:scope.upstream.version,commit:scope.upstream.commit,
  records:records.length,svgSha256:Object.fromEntries(records.map(item=>[item.name,
    crypto.createHash('sha256').update(item.svg).digest('hex')]))}));
