'use strict';
// Pinned 4.1.3 CJS full TeX -> common/SVG -> Lite serialization witness.
// Continuous rows reuse one document per identical TeX option set; fresh rows
// use one Node realm per case, with that row's options applied at construction.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),cp=require('node:child_process'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../../../../..'),bundle=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax'),cjsRoot=path.join(bundle,'src/cjs');
const scopePath=path.join(root,'tools/math-port/profiles/scope.json'),profileLockPath=path.join(root,'tools/math-port/common-layout-reference/profile-source-lock.json'),casesPath=path.join(__dirname,'cases.json');
const scope=JSON.parse(fs.readFileSync(scopePath,'utf8')),lock=JSON.parse(fs.readFileSync(profileLockPath,'utf8')),casesFile=JSON.parse(fs.readFileSync(casesPath,'utf8'));
const sha=f=>crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex');
assert.equal(scope.upstream.version,'4.1.3');assert.equal(scope.upstream.commit,'fb987178c2d279a99b0db5ea02e751691703955e');
assert.equal(sha(path.join(root,scope.profile.file)),scope.profile.sha256);assert.equal(sha(path.join(root,'tools/mathjax-bundle/package-lock.json')),scope.dependency_lock_sha256);
assert.equal(JSON.parse(fs.readFileSync(path.join(bundle,'src/package.json'),'utf8')).version,'4.1.3');
for(const source of scope.files)assert.equal(sha(path.join(root,'.reference/MathJax-src',source.source)),source.source_sha256,source.source);
for(const dep of lock.dependencies)assert.equal(sha(path.join(bundle,dep.file)),dep.sha256,dep.file);
assert.equal(casesFile.schema,1);assert.equal(casesFile.cases.length,3);
for(const row of casesFile.cases)assert.ok(scope.profile.tex_packages.includes('textmacros')&&scope.profile.tex_packages.includes('bboldx'),row.name);
const loaded=()=>Object.keys(require.cache).filter(f=>f.startsWith(bundle+path.sep)).sort().map(f=>({file:path.relative(bundle,f).replaceAll(path.sep,'/'),sha256:sha(f)}));
const checkClosure=deps=>{for(const dep of deps){const expected=lock.dependencies.find(x=>x.file===dep.file);assert.ok(expected,'unlocked dependency '+dep.file);assert.equal(dep.sha256,expected.sha256,dep.file);}};
function setup(){
 const load=f=>require(path.join(cjsRoot,f));const {mathjax}=load('mathjax.js'),{TeX}=load('input/tex.js'),{SVG}=load('output/svg.js'),{liteAdaptor}=load('adaptors/liteAdaptor.js'),{RegisterHTMLHandler}=load('handlers/html.js');
 const {MathJaxNewcmFont}=require(path.join(bundle,'mathjax-newcm-font/cjs/svg.js'));
 const profile=fs.readFileSync(path.join(root,scope.profile.file),'utf8');
 for(const m of profile.matchAll(/require\('@mathjax\/src\/js\/([^']+Configuration\.js)'\)/g))load(m[1]);
 const adaptor=liteAdaptor({fontSize:16});RegisterHTMLHandler(adaptor);mathjax.asyncIsSynchronous=true;
 mathjax.asyncLoad=name=>{const target=path.isAbsolute(name)?name:path.resolve(cjsRoot,name);assert.ok(target.startsWith(bundle+path.sep),'external module requested: '+name);return require(target);};
 const makeDocument=options=>{const input=new TeX({...options,packages:scope.profile.tex_packages});const output=new SVG({...scope.profile.svg_options,fontData:MathJaxNewcmFont,dynamicPrefix:path.join(bundle,'mathjax-newcm-font/cjs/svg/dynamic')});for(const [pkg,name] of [['bboldx','MathJaxBboldxFontExtension'],['bbm','MathJaxBbmFontExtension'],['dsfont','MathJaxDsfontFontExtension']])output.addExtension(require(path.join(bundle,`mathjax-${pkg}-font-extension/cjs/svg.js`))[name]);return mathjax.document('',{InputJax:input,OutputJax:output});};
 return {mathjax,adaptor,makeDocument};
}
function capture(document,item,group,index,adaptor){let svg=null,error=null;try{svg=adaptor.serializeXML(document.convert(item.tex,{display:false,em:16,ex:8,containerWidth:1280}));}catch(e){error={name:e.name||e.constructor.name,message:String(e.message||e)};}const paths=[];if(svg!==null)for(const m of svg.matchAll(/<path\b([^>]*)>/g)){const attrs=m[1],d=attrs.match(/\bd="([^"]*)"/),c=attrs.match(/\bdata-c="([^"]*)"/);paths.push({codepoint:c?.[1]??null,d:d?.[1]??null});}return {group,index,name:item.name,tex:item.tex,configuration:item.options||{},display:false,em:16,ex:8,width:1280,svg,mmlError:svg!==null&&(svg.includes('data-mjx-error=')||svg.includes('data-mml-node="merror"')),error,pathInventory:paths};}
if(process.argv[2]==='--worker'){
 const index=Number(process.argv[3]);assert.ok(Number.isInteger(index)&&index>=0&&index<casesFile.cases.length);const {adaptor,makeDocument}=setup(),item=casesFile.cases[index],record=capture(makeDocument(item.options||{}),item,'fresh-session-per-case',index,adaptor),dependencies=loaded();checkClosure(dependencies);process.stdout.write(JSON.stringify({record,dependencies}));
}else{
 const {adaptor,makeDocument}=setup(),documents=new Map(),records=[];
 const documentFor=item=>{const options=item.options||{},key=JSON.stringify(options);if(!documents.has(key))documents.set(key,makeDocument(options));return documents.get(key);};
 casesFile.cases.forEach((item,index)=>records.push(capture(documentFor(item),item,'continuous',index,adaptor)));
 const fresh=[],freshDependencies=[];for(let index=0;index<casesFile.cases.length;++index){const run=cp.spawnSync(process.execPath,[__filename,'--worker',String(index)],{cwd:root,encoding:'utf8',maxBuffer:64*1024*1024,timeout:30000,windowsHide:true});assert.equal(run.status,0,`Source worker ${index}: ${run.stderr||run.error?.message||run.stdout}`);const result=JSON.parse(run.stdout);fresh.push(result.record);checkClosure(result.dependencies);freshDependencies.push(...result.dependencies);}
 const dependencies=loaded();checkClosure(dependencies);const all=new Map([...dependencies,...freshDependencies].map(d=>[d.file,d.sha256]));
 process.stdout.write(JSON.stringify({schema:1,source:{version:scope.upstream.version,commit:scope.upstream.commit,scopeSha256:sha(scopePath),profileSha256:sha(path.join(root,scope.profile.file)),profileLockSha256:sha(profileLockPath),caseSha256:sha(casesPath),dependencies:[...all].sort(([a],[b])=>a.localeCompare(b)).map(([file,sha256])=>({file,sha256}))},groups:['continuous','fresh-session-per-case'],records:[...records,...fresh]}));
}

