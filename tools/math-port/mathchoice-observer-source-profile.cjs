// Original MathChoice Source-profile checker over its canonical observation producer.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');
const bytes=fs.readFileSync(path.join(root,'docs/math-port/evidence/p23n-mathchoice-source-profile.json'));
assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),"3dc783dce354ba83003d6949ade951047d83912982f3a2409d42085a3b871cea",'Frozen original p23n bytes changed');
const frozen=JSON.parse(bytes),actual=require('./mathchoice-oracle.cjs');actual.check();
function encode(v){if(v&&v.$type==='undefined')return ['undefined'];if(v&&v.$type==='number'){const n=v.value==='NaN'?NaN:v.value==='-0'?-0:Number(v.value);return number(n);}if(v===null)return ['null'];if(typeof v==='number')return number(v);if(typeof v!=='object')return [typeof v,v];return [Array.isArray(v)?'array':'object',Array.isArray(v)?v.length:null,Object.keys(v).map(k=>[k,encode(v[k])])];}
function number(v){const bytes=Buffer.alloc(8);bytes.writeDoubleLE(v);return ['number',bytes.toString('hex')];}
assert.equal(actual.records.length,410);assert.equal(frozen.assertions,412);assert.equal(frozen.cases,410);assert.deepStrictEqual(actual.records.map(r=>r.name),frozen.names);
const sourceProfile=actual.sourceMetadata.map((metadata,i)=>[...metadata,encode(actual.records[i].observations[0])]);
assert.equal(sourceProfile.length,409);for(let i=0;i<sourceProfile.length;i++)assert.equal(JSON.stringify(sourceProfile[i]),JSON.stringify(frozen.sourceProfile[i]),'original Source record '+i);
console.log(JSON.stringify({assertions:frozen.assertions,cases:frozen.cases,sourceCalls:sourceProfile.length,originalValueDeclarations:frozen.expectedValues.length,originalProfileSha256:crypto.createHash('sha256').update(bytes).digest('hex'),mode:'check'}));
