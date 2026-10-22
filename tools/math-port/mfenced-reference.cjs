// Root actual pinned CJS loader for the complete mfenced semantic source file.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root,MmlFactory,core}=require('./simple-mml-reference.cjs');
const source='ts/core/MmlTree/MmlNodes/mfenced.ts',sha256='64b3f4d89d47765eb83034b480b843b2769d8de12e31d7571c50e5c4bb2bd397',bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));
assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),sha256);assert.equal(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));
const MmlMfenced=new MmlFactory().getNodeClass('mfenced');module.exports={root,MmlFactory,core,MmlMfenced,source,sha256};
