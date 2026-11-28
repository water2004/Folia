'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const installed=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const ts='ts/input/tex/TokenMap.ts',cjs='cjs/input/tex/TokenMap.js';
const cloned=fs.readFileSync(path.join(root,'.reference/MathJax-src',ts));
assert.equal(fs.readFileSync(path.join(installed,ts),'utf8').replaceAll('\r\n','\n'),cloned.toString('utf8').replaceAll('\r\n','\n'));
const {CharacterMap}=require(path.join(installed,cjs));
const marker=Symbol('marker'),peer=Symbol('marker');
const map=new CharacterMap('native-symbol-marker',undefined,{});
map.add('x',marker);
const stored=map.lookup('x');
const error=operation=>{try{operation();return null;}catch(e){return{name:e.name,message:e.message};}};
const cases={
  identity:{same:marker===marker,peerDistinct:marker!==peer,truthy:!!marker},
  realMap:{present:map.contains('x'),retainsMarker:stored===marker},
  string:error(()=>''+marker),number:error(()=>Number(marker))
};
map.remove('x');cases.realMap.removed=!map.contains('x');
assert.deepEqual(cases.identity,{same:true,peerDistinct:true,truthy:true});
assert.deepEqual(cases.realMap,{present:true,retainsMarker:true,removed:true});
assert.deepEqual(cases.string,{name:'TypeError',message:'Cannot convert a Symbol value to a string'});
assert.deepEqual(cases.number,{name:'TypeError',message:'Cannot convert a Symbol value to a number'});
const hash=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const result={schema:'property-symbol-v1',hashes:{[ts]:hash(cloned),[cjs]:hash(fs.readFileSync(path.join(installed,cjs)))},cases};
const evidence=path.join(root,'tools/math-port/profiles/evidence/property-symbol-source.json');
if(process.argv.includes('--check')){
  assert.deepEqual(result,JSON.parse(fs.readFileSync(evidence,'utf8')));
  console.log('Pinned Symbol marker identity, TokenMap storage and coercion agree');
}else fs.writeFileSync(evidence,JSON.stringify(result,null,2)+'\n');
