// Root locked source/reference loader; never linked into the native runtime.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root}=require('./mo-pattern-reference.cjs');
const {MmlFactory}=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlFactory.js'));
const scope=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json'),'utf8'));
const kinds=['mfrac','msqrt','mroot'];const classes={};const hashes=[];
for(const kind of kinds){const source='ts/core/MmlTree/MmlNodes/'+kind+'.ts',entry=scope.files.find(f=>f.source===source),bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));const f=new MmlFactory();classes[kind]=f.getNodeClass(kind);hashes.push({source,sha256:entry.source_sha256});}
module.exports={root,MmlFactory,kinds,classes,hashes};
if(require.main===module)console.log(JSON.stringify({kinds,hashes}));
