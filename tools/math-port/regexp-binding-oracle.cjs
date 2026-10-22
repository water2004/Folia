// Root original Options / RegExp identity and cursor observations.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src'),lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p46-parser-options-source.json'))),source='ts/util/Options.ts',entry=lock.hashes.find(x=>x.source===source),bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(base,'cjs/util/Options.js'))).digest('hex'),entry.cjs_sha256);
const {copy}=require(path.join(base,'cjs/util/Options.js')),rows=[];
function add(label,run){let expected;try{expected=run();}catch(x){expected=x.name;}rows.push({label,expected});}
add('canonical-same-native-identity',()=>{const r=/x/g;return String(r===r);});
add('distinct-new-expressions',()=>String(new RegExp('x','g')!==new RegExp('x','g')));
add('option-copy-retains-actual-regexp',()=>{const r=/x/g,o={r},c=copy(o);return[c!==o,c.r===r].join(':');});
add('copied-end-tuple-keeps-pattern-identity',()=>{const r=/x/g,t=['x',false,r],c=copy(t);return[c!==t,c[2]===r].join(':');});
add('cursor-update-observed-through-copied-alias',()=>{const r=/x/g,c=copy({r});r.lastIndex=1;const m=c.r.exec('xxx');return[m.index,r.lastIndex,c.r.lastIndex].join(':');});
add('independent-pattern-cursors',()=>{const a=/x/g,b=/x/g;a.lastIndex=1;const ma=a.exec('xxx'),mb=b.exec('xxx');return[ma.index,mb.index,a.lastIndex,b.lastIndex].join(':');});
add('global-failure-resets-shared-cursor',()=>{const r=/x/g,t=['x',false,r];r.lastIndex=4;return[!t[2].exec('x'),r.lastIndex].join(':');});
add('actual-regexp-constructor-metadata',()=>{const r=/x/g;return[r.constructor===RegExp,r.constructor.name,r.constructor.length].join(':');});
const report={stage:'native RegExp identities in Context; not FindTeX',node:process.version,hashes:[entry],rows},sourceGold=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p51b-regexp-binding-source.json'),'utf8'));const projected={...sourceGold,rows:sourceGold.rows.map(({cpp,...row})=>row)};assert.ok(process.argv.includes('--check'));assert.deepEqual(report,projected);console.log(JSON.stringify({rows:rows.length,mode:'check'}));
