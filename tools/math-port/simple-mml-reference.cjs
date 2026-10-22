// Root pinned actual source loader; no native runtime reference dependency.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root,MmlFactory}=require('./arithmetic-reference.cjs');
const scope=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json'),'utf8'));
const kinds=['menclose','mpadded','maligngroup','malignmark','mglyph'],classes={},hashes=[];
for(const kind of kinds){const source='ts/core/MmlTree/MmlNodes/'+kind+'.ts',item=scope.files.find(f=>f.source===source),bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),item.source_sha256);assert.equal(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));classes[kind]=new MmlFactory().getNodeClass(kind);hashes.push({source,sha256:item.source_sha256});}
const core=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNode.js'));
module.exports={root,MmlFactory,kinds,classes,hashes,core};
