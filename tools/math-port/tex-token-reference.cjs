// Root actual pinned CJS source loader. Development-only, never native runtime.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {root}=require('./mo-pattern-reference.cjs');
const scope=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json')));
const names=['Token','Types','HandlerTypes','TexConstants'],hashes=[];
for(const name of names){const source='ts/input/tex/'+name+'.ts',entry=scope.files.find(f=>f.source===source),bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));hashes.push({source,sha256:entry.source_sha256});}
const base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex');
module.exports={root,hashes,...require(path.join(base,'Token.js')),...require(path.join(base,'HandlerTypes.js')),...require(path.join(base,'TexConstants.js'))};
