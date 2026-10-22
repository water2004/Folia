// Source-only check; original profile bytes and all Source fields are preserved.
// Root real Options/MML identity observations; no native parser substitute.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory}=require('./simple-mml-reference.cjs'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p46-parser-options-source.json'))),hashes=[];
for(const source of ['ts/util/Options.ts','ts/core/Tree/Factory.ts','ts/core/MmlTree/MmlFactory.ts','ts/core/MmlTree/MmlNode.ts','ts/core/MmlTree/MmlNodes/mi.ts']){const entry=lock.hashes.find(x=>x.source===source);assert.ok(entry,source);const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(base,source.replace(/^ts\//,'cjs/').replace(/\.ts$/,'.js')))).digest('hex'),entry.cjs_sha256);hashes.push(entry);}
const {defaultOptions,copy}=require(path.join(base,'cjs/util/Options.js')),rows=[];
function add(label,run){const factory=new MmlFactory(),node=factory.create('mi'),type=factory.getNodeClass('mi');rows.push({label,expected:run({factory,node,type})});}
add('same-node-canonical-handle',e=>String(e.node===e.node)+':'+String(e.node===e.node));
add('distinct-native-nodes',e=>{const other=e.factory.create('mi');return String(e.node!==other)+':'+String(other===other);});
add('actual-factory-class-identity',e=>String(new MmlFactory().getNodeClass('mi')===e.type));
add('node-alias-through-option-copy',e=>{const source={node:e.node,array:[e.node,e.node]},result=defaultOptions({},source);return[result.node===e.node,result.array[0]===e.node,result.array!==source.array].join(':');});
add('factory-class-alias-through-option-copy',e=>{const source={items:{mi:e.type}},result=defaultOptions({},source);return String(result.items!==source.items)+':'+String(result.items.mi===e.type);});
add('same-node-graph-observed-through-alias',e=>{const result=copy({node:e.node});result.node.setProperty('probe',9);return String(e.node.getProperty('probe'));});
add('ordinary-node-list-array-and-inherited-key',e=>{const dict={mi:[e.node,e.node]};return String(dict.constructor===Object)+':'+String(dict.mi[0]===dict.mi[1]);});
add('dictionary-enumeration-order',e=>Object.keys({z:e.node,2:e.node,a:e.node,1:e.node}).join(','));
add('node-list-array-rebinding-retains-captured-array',e=>{const dict={mi:[e.node]},old=dict.mi;dict.mi=[];return String(old!==dict.mi)+':'+String(old[0]===e.node);});
const report={stage:'original Options/MML reference observations, not parser acceptance',hashes,rows};
const profile=path.join(root,'docs/math-port/evidence/p49a-native-identities-source.json');
const sourceGold=JSON.parse(fs.readFileSync(profile,'utf8'));
const projected={...sourceGold,rows:sourceGold.rows.map(({cpp,...row})=>row)};
assert.deepEqual(report,projected);
console.log(JSON.stringify({sourceCases:rows.length,mode:'read-only-source-check'}));
