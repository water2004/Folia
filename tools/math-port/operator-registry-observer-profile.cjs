// Original profile entry point; canonical Source owners now expose independent profiles.
'use strict';
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const tools=process.env.FOLIA_SOURCE_CHECK_TOOLS||path.join(root,'tools/math-port');
function replay(entry){const profile=path.join(root,entry.profile);assert.equal(crypto.createHash('sha256').update(fs.readFileSync(profile)).digest('hex'),entry.sha256,'Frozen Source profile changed: '+entry.profile);const result=cp.spawnSync(process.execPath,[path.join(tools,entry.owner),'--check'],{cwd:root,encoding:'utf8',windowsHide:true,timeout:60000,maxBuffer:1024*1024});assert.ifError(result.error);assert.equal(result.status,0,entry.owner+' '+result.stderr);console.log(result.stdout.trim());}
const packets={
  "mo-values": {
    "owner": "mo-values-oracle.cjs",
    "profile": "docs/math-port/evidence/p24-mo-values-source.json",
    "sha256": "d7a2eecfc27538ccf9f3adfa4a8259c1d9332e3b33783eacab32eab7b81f24aa",
    "legacy": {
      "path": "docs/math-port/evidence/p24-mo-values-observer-profile.json",
      "sha256": "aa4a959edcd2a6ac8bdfa74c27ea0d06c4fbfcc3a68f8df55e8b54620d530af3"
    }
  },
  "mml-registry": {
    "owner": "mml-registry-oracle.cjs",
    "profile": "docs/math-port/evidence/mml-registry-source.json",
    "sha256": "47b1d8da20d41129a08beba3e5db8619a44c2b96a20208b0549890438c8d05a0",
    "legacy": {
      "path": "docs/math-port/evidence/p24-mml-registry-observer-profile.json",
      "sha256": "be28edd78005b1e8ad31b814b01400e89d35a40da9704d3c6c9872a0018de072"
    }
  },
  "mo": {
    "owner": "mo-oracle.cjs",
    "profile": "docs/math-port/evidence/p24-mo-source.json",
    "sha256": "24a7e882ed10881e970f3a9f997ad973c47dc8753b68e0c78b3657c1865693b1",
    "legacy": {
      "path": "docs/math-port/evidence/p24-mo-observer-profile.json",
      "sha256": "0af4f4633e9ddaa760a6db50ed046c36363e0c968eb6ffe61d3bc7c425150f98"
    }
  }
};
const packet=process.argv.find(x=>x.startsWith('--packet='))?.slice(9);assert.ok(Object.hasOwn(packets,packet),'Select an existing --packet: '+Object.keys(packets).join(', '));
const entry=packets[packet],legacy=fs.readFileSync(path.join(root,entry.legacy.path));assert.equal(crypto.createHash('sha256').update(legacy).digest('hex'),entry.legacy.sha256,'Original observer profile bytes changed');
replay(entry);
console.log(JSON.stringify({packet,legacyProfile:entry.legacy.path,legacyProfileSha256:entry.legacy.sha256,profile:entry.profile,mode:'check'}));
