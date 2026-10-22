// Root-owned pinned source loader; never linked into the native runtime.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root,MmlFactory}=require('./arithmetic-reference.cjs');
const source='ts/core/MmlTree/MmlNodes/mmultiscripts.ts',sha256='bb8d8d2180ef483b86b0b1570b919ba424ed54db1f30ef6e0e697cfab0bf5cf7';
const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));
assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),sha256);
assert.equal(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));
const kinds=['mmultiscripts','mprescripts','none'],classes={};
for(const kind of kinds)classes[kind]=new MmlFactory().getNodeClass(kind);
const core=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNode.js'));
module.exports={root,MmlFactory,kinds,classes,source,sha256,core};
