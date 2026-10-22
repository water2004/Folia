// Original profile entry point; canonical Source owners now expose independent profiles.
'use strict';
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const tools=process.env.FOLIA_SOURCE_CHECK_TOOLS||path.join(root,'tools/math-port');
function replay(entry){const profile=path.join(root,entry.profile);assert.equal(crypto.createHash('sha256').update(fs.readFileSync(profile)).digest('hex'),entry.sha256,'Frozen Source profile changed: '+entry.profile);const result=cp.spawnSync(process.execPath,[path.join(tools,entry.owner),'--check'],{cwd:root,encoding:'utf8',windowsHide:true,timeout:60000,maxBuffer:1024*1024});assert.ifError(result.error);assert.equal(result.status,0,entry.owner+' '+result.stderr);console.log(result.stdout.trim());}
const packets={
  "script": {
    "owner": "script-oracle.cjs",
    "profile": "docs/math-port/evidence/script-source-profile.json",
    "sha256": "58aa2985bcf14a36b5b135c430a776e59e729132af615f4fcef21036a08a53d1",
    "legacy": {
      "path": "docs/math-port/evidence/p23l-script-source-profile.json",
      "sha256": "cc570115c252cd201bd2a59030cb8cc747ddbf3eb3106481d72df9d0b6cac898"
    }
  },
  "leaf": {
    "owner": "leaf-oracle.cjs",
    "profile": "docs/math-port/evidence/leaf-source-profile.json",
    "sha256": "67769d004e1ab15f32f8c7ea9ba776635d5b0a493366c8a838b2ab944fa5d748",
    "legacy": {
      "path": "docs/math-port/evidence/p23l-leaf-source-profile.json",
      "sha256": "2014a953aba93aae64e7a12be82e08d5cfc4fd44f8bf44ef6e8e83350ab1fce4"
    }
  }
};
const packet=process.argv.find(x=>x.startsWith('--packet='))?.slice(9);assert.ok(Object.hasOwn(packets,packet),'Select an existing --packet: '+Object.keys(packets).join(', '));
const entry=packets[packet],legacy=fs.readFileSync(path.join(root,entry.legacy.path));assert.equal(crypto.createHash('sha256').update(legacy).digest('hex'),entry.legacy.sha256,'Original observer profile bytes changed');
replay(entry);
console.log(JSON.stringify({packet,legacyProfile:entry.legacy.path,legacyProfileSha256:entry.legacy.sha256,profile:entry.profile,mode:'check'}));
