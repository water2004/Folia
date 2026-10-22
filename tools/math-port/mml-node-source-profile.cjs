// Frozen Source checks for the canonical MmlNode observation producer.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
function check(actual){const file=path.join(root,'docs/math-port/evidence/p23k-mml-node-structured-source-profile.json');const frozen=JSON.parse(fs.readFileSync(file,'utf8'));const legacy=fs.readFileSync(path.join(root,frozen.originalProfile.path));assert.equal(sha(legacy),frozen.originalProfile.sha256,'Original p23k profile bytes changed');assert.equal(legacy.length,frozen.originalProfile.bytes);assert.equal(actual.assertions,frozen.profile.assertions);assert.equal(actual.cases,frozen.profile.cases);for(const key of ['sourceProfile','directProfile']){assert.equal(actual[key].length,frozen.profile[key].length);for(let i=0;i<actual[key].length;i++)assert.equal(JSON.stringify(actual[key][i]),JSON.stringify(frozen.profile[key][i]),key+' record '+i);}console.log(JSON.stringify({assertions:actual.assertions,cases:actual.cases,sourceCases:actual.sourceProfile.filter(r=>r[0]==='case').length,helperObservations:actual.sourceProfile.filter(r=>r[0]!=='case').length,directObservations:actual.directProfile.length,originalProfileSha256:sha(legacy),profileSha256:sha(fs.readFileSync(file)),mode:'check'}));}
module.exports={check};
if(require.main===module)check(require('./mml-node-oracle.cjs'));
