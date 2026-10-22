// Root-owned source loader; no native runtime dependency.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root,MmlFactory}=require('./arithmetic-reference.cjs');
const hashes=[['TeXAtom','bad114224392b94852456a4636b85ad90a5a2c96b63e6c7911d155db299f7028'],['semantics','e609dfc3d7510a996a87804e3929cd4f53a5aa060003e351f45f1fc5dcfb6e96']].map(([name,sha256])=>({name,source:'ts/core/MmlTree/MmlNodes/'+name+'.ts',sha256}));
for(const {source,sha256}of hashes){const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),sha256);assert.equal(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));}
const kinds=['TeXAtom','semantics','annotation-xml','annotation'],classes={};for(const kind of kinds)classes[kind]=new MmlFactory().getNodeClass(kind);
const core=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNode.js')),Mo=new MmlFactory().getNodeClass('mo');
assert.equal(classes.TeXAtom.prototype.adjustTeXclass,Mo.prototype.adjustTeXclass);
module.exports={root,MmlFactory,kinds,classes,core,Mo,hashes};
