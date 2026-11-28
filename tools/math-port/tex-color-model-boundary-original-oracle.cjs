'use strict';
// Supplementary actual ColorModel boundaries; validates the full pinned source
// lock before loading the original implementation.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
const sha=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
if(sha(path.join(root,scope.profile.file))!==scope.profile.sha256)throw Error('profile lock mismatch');
for(const item of scope.files){const p=path.join(root,'.reference/MathJax-src',item.source);if(sha(p)!==item.source_sha256)throw Error(`Source lock mismatch: ${item.source}`);}
const {ColorModel}=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/color/ColorUtil.js'));
const m=new ColorModel();
const cases=[
  ['rgb-js-whitespace',()=>m.getColor('rgb','\u00a00.5\u2003,\ufeff0,\u30001\u00a0')],
  ['gray-js-whitespace',()=>m.getColor('gray','\ufeff0.5\u2003')],
  ['rgb-overflow-decimal',()=>m.getColor('rgb','9'.repeat(400)+',0,0')],
  ['rgb-user-defined',()=>{m.defineColor('RGB','custom','32,64,128');return m.getColor('named','custom');}],
  ['userColors-object-shape',()=>({isMap:m.userColors instanceof Map,keys:Object.keys(m),prototype:Object.getOwnPropertyNames(ColorModel.prototype).map(k=>[k,ColorModel.prototype[k].name,ColorModel.prototype[k].length])})],
];
const observations=cases.map(([name,run])=>{try{return{name,result:run(),error:null};}catch(e){return{name,result:null,error:{name:e.name,id:e.id??null,message:String(e.message)}};}});
const out=path.join(root,'build/math-port/tex-color-model-boundary-original.json');fs.mkdirSync(path.dirname(out),{recursive:true});fs.writeFileSync(out,JSON.stringify({sourceLocks:scope.files.length,cases:observations},null,2)+'\n');
console.log(JSON.stringify({sourceLocks:scope.files.length,cases:observations.length,output:path.relative(root,out)}));
