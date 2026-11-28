'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),pkg=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax');
const digest=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const scopePath=path.join(root,'tools/math-port/profiles/scope.json'),scope=JSON.parse(fs.readFileSync(scopePath));
assert.equal(digest(scopePath),'9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1');
assert.equal(digest(path.join(root,scope.profile.file)),scope.profile.sha256);
const lock=JSON.parse(fs.readFileSync(path.join(__dirname,'common-layout-reference/profile-source-lock.json')));
for(const item of lock.dependencies)assert.equal(digest(path.join(pkg,item.file)),item.sha256,item.file);
const src=path.join(pkg,'src/cjs'),load=file=>require(path.join(src,file));
const {mathjax}=load('mathjax.js'),{TeX}=load('input/tex.js'),{SVG}=load('output/svg.js');
const {liteAdaptor}=load('adaptors/liteAdaptor.js'),{RegisterHTMLHandler}=load('handlers/html.js');
const profile=fs.readFileSync(path.join(root,scope.profile.file),'utf8');
for(const match of profile.matchAll(/require\('@mathjax\/src\/js\/(input\/tex\/[^']+Configuration\.js)'\)/g))load(match[1]);
const {MathJaxNewcmFont}=require(path.join(pkg,'mathjax-newcm-font/cjs/svg.js'));
const adaptor=liteAdaptor({fontSize:16});RegisterHTMLHandler(adaptor);
const input=new TeX({packages:scope.profile.tex_packages});
const output=new SVG({...scope.profile.svg_options,fontData:MathJaxNewcmFont,
  dynamicPrefix:path.join(pkg,'mathjax-newcm-font/cjs/svg/dynamic')});
for(const [name,field]of[['bboldx','MathJaxBboldxFontExtension'],['bbm','MathJaxBbmFontExtension'],['dsfont','MathJaxDsfontFontExtension']])
 output.addExtension(require(path.join(pkg,`mathjax-${name}-font-extension/cjs/svg.js`))[field]);
mathjax.asyncIsSynchronous=true;
mathjax.asyncLoad=name=>require(path.isAbsolute(name)?name:path.resolve(src,name));
const document=mathjax.document('',{InputJax:input,OutputJax:output});
const casesPath=path.join(__dirname,'math-api-cases.json'),cases=JSON.parse(fs.readFileSync(casesPath)).cases;
const records=cases.map(item=>{
 let svg=null,error=null;
 try{svg=adaptor.serializeXML(document.convert(item.tex,{display:item.display??false,
   em:item.em??16,ex:(item.em??16)*.5,containerWidth:item.width??1280}));}
 catch(caught){error={name:caught.name,message:String(caught.message)};}
 return{name:item.name,svg,error};
});
const destination=process.argv[2]||path.join(root,'build/math-port/math-api-original.json');
fs.mkdirSync(path.dirname(destination),{recursive:true});
fs.writeFileSync(destination,JSON.stringify({schema:1,corpusSha256:digest(casesPath),records},null,2)+'\n');
console.log(JSON.stringify({destination,records:records.length,sha256:digest(destination)}));
