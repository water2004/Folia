// Root pinned actual source loader; never a native runtime service.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root,MmlFactory,core}=require('./mfenced-reference.cjs');
const scope=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json')));
const names=['core/MmlTree/MmlNode',...['mi','mo','mstyle','mtable','mtr','mathchoice'].map(n=>'core/MmlTree/MmlNodes/'+n)],hashes=[];
for(const name of names){const source='ts/'+name+'.ts',entry=scope.files.find(x=>x.source===source),bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));hashes.push({source,sha256:entry.source_sha256});}
const MathChoice=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNodes/mathchoice.js')).MathChoice;
module.exports={root,MmlFactory,core,hashes,MathChoice};
