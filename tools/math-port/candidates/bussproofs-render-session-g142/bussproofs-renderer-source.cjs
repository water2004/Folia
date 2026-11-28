'use strict';
// Public-output reference for Bussproofs renderer registration. This is a new
// supplementary witness; the locked package/MML oracle remains unchanged.
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const assert=require('node:assert/strict');

const root=path.resolve(__dirname,'../../../..');
const sourceRoot=path.join(root,'.reference/MathJax-src');
const cjsRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const cjs=path.join(cjsRoot,'cjs');
const packages=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax');
const lock=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/locks/bussproofs-source-lock.json'),'utf8'));
const hash=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for(const file of lock.files){
  assert.equal(hash(path.join(sourceRoot,file.ts)),file.tsSha256,`Source ${file.ts}`);
  assert.equal(hash(path.join(cjsRoot,file.cjs)),file.cjsSha256,`Source CJS ${file.cjs}`);
}
const load=name=>require(path.join(cjs,name));
const {mathjax}=load('mathjax.js');
const {TeX}=load('input/tex.js');
const {SVG}=load('output/svg.js');
const {liteAdaptor}=load('adaptors/liteAdaptor.js');
const {RegisterHTMLHandler}=load('handlers/html.js');
const {MathJaxNewcmFont}=require(path.join(packages,'mathjax-newcm-font/cjs/svg.js'));
const {MathJaxBboldxFontExtension}=require(path.join(packages,'mathjax-bboldx-font-extension/cjs/svg.js'));
const {MathJaxBbmFontExtension}=require(path.join(packages,'mathjax-bbm-font-extension/cjs/svg.js'));
const {MathJaxDsfontFontExtension}=require(path.join(packages,'mathjax-dsfont-font-extension/cjs/svg.js'));
load('input/tex/bussproofs/BussproofsConfiguration.js');

const adaptor=liteAdaptor({fontSize:16});
RegisterHTMLHandler(adaptor);
const input=new TeX({packages:['base','bussproofs']});
const output=new SVG({fontData:MathJaxNewcmFont,fontCache:'none',exFactor:.5,
  displayOverflow:'linebreak',linebreaks:{inline:true}});
output.addExtension(MathJaxBboldxFontExtension);
output.addExtension(MathJaxBbmFontExtension);
output.addExtension(MathJaxDsfontFontExtension);
const document=mathjax.document('',{InputJax:input,OutputJax:output});
const inputs={
  unary:String.raw`\begin{prooftree}\AxiomC{A}\UnaryInfC{B}\end{prooftree}`,
  binary:String.raw`\begin{prooftree}\AxiomC{P}\AxiomC{Q}\BinaryInfC{P\land Q}\end{prooftree}`,
  ordinary:'x+1'
};
const results=Object.fromEntries(Object.entries(inputs).map(([name,tex])=>{
  const node=document.convert(tex,{display:false,em:16,ex:8,containerWidth:1280});
  const svg=adaptor.outerHTML(node);
  assert.match(svg,/<svg\b/);
  return [name,{svg,sha256:crypto.createHash('sha256').update(svg).digest('hex')}];
}));
assert.notEqual(results.unary.svg,results.ordinary.svg);
assert.notEqual(results.binary.svg,results.unary.svg);
process.stdout.write(JSON.stringify({source:'MathJax 4.1.3 CJS SVG renderer',lockFiles:lock.files.length,inputs,results},null,2)+'\n');
