'use strict';
// Read-only diagnostic for whether fresh captures inherit state from the first group.
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'../../../..'), packages=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax'), cjsRoot=path.join(packages,'src/cjs');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
const profileText=fs.readFileSync(path.join(root,scope.profile.file),'utf8');
const inventory=JSON.parse(fs.readFileSync(path.join(root,'build/math-port/common-layout-profile-root-locked.json'),'utf8')).records;
const mode=process.argv[2], caseName=process.argv[3];
const caseItem=inventory.find(x=>x.name===caseName); if(!caseItem)throw Error('unknown case '+caseName);
const load=file=>require(path.join(cjsRoot,file));
const {mathjax}=load('mathjax.js'),{TeX}=load('input/tex.js'),{SVG}=load('output/svg.js'),{liteAdaptor}=load('adaptors/liteAdaptor.js'),{RegisterHTMLHandler}=load('handlers/html.js');
const {MathJaxNewcmFont}=require(path.join(packages,'mathjax-newcm-font/cjs/svg.js'));
for(const match of profileText.matchAll(/require\('@mathjax\/src\/js\/(input\/tex\/[^']+Configuration\.js)'\)/g))load(match[1]);
const adaptor=liteAdaptor({fontSize:16});RegisterHTMLHandler(adaptor);mathjax.asyncIsSynchronous=true;
mathjax.asyncLoad=name=>{const p=path.isAbsolute(name)?name:path.resolve(cjsRoot,name);if(!p.startsWith(packages+path.sep))throw Error('external '+name);return require(p)};
function makeDoc(){const input=new TeX({packages:scope.profile.tex_packages});const output=new SVG({...scope.profile.svg_options,fontData:MathJaxNewcmFont,dynamicPrefix:path.join(packages,'mathjax-newcm-font/cjs/svg/dynamic')});for(const [n,e] of [['bboldx','MathJaxBboldxFontExtension'],['bbm','MathJaxBbmFontExtension'],['dsfont','MathJaxDsfontFontExtension']])output.addExtension(require(path.join(packages,`mathjax-${n}-font-extension/cjs/svg.js`))[e]);return mathjax.document('',{InputJax:input,OutputJax:output})}
function render(doc,item){try{return{svg:adaptor.serializeXML(doc.convert(item.tex,{display:item.display??false,em:item.em??16,ex:item.ex??8,containerWidth:item.width??1280})),error:null}}catch(e){return{svg:null,error:{name:e.name||e.constructor.name,message:String(e.message||e)}}}}
if(mode==='after-continuous'){const doc=makeDoc();for(const item of inventory)render(doc,item)}
console.log(JSON.stringify(render(makeDoc(),caseItem)));




