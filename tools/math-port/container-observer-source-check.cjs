// Replay the same canonical Source owners against complete frozen Source profiles.
'use strict';
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const tools=process.env.FOLIA_SOURCE_CHECK_TOOLS||path.join(root,'tools/math-port');
function replay(entry){const profile=path.join(root,entry.profile);assert.equal(crypto.createHash('sha256').update(fs.readFileSync(profile)).digest('hex'),entry.sha256,'Frozen Source profile changed: '+entry.profile);const result=cp.spawnSync(process.execPath,[path.join(tools,entry.owner),'--check'],{cwd:root,encoding:'utf8',windowsHide:true,timeout:60000,maxBuffer:1024*1024});assert.ifError(result.error);assert.equal(result.status,0,entry.owner+' '+result.stderr);console.log(result.stdout.trim());}
const entries=[
  {
    "owner": "simple-mml-oracle.cjs",
    "profile": "docs/math-port/evidence/simple-mml-source.json",
    "sha256": "d1e05bb92ff3bf94f435a642cd6fa8febf3fc689ece40c3754441782c2556ede"
  },
  {
    "owner": "mfenced-oracle.cjs",
    "profile": "docs/math-port/evidence/source-mfenced-profile.json",
    "sha256": "869b0e7fe7d550566b3e7b814403be902790eca7b9f7345b2e7b0af8508be6a6"
  },
  {
    "owner": "maction-oracle.cjs",
    "profile": "docs/math-port/evidence/source-maction-profile.json",
    "sha256": "e4a2c760c9a6a5ead9624db4b2d1c26d45eedaa77c042a1c31f3407de511ea0d"
  }
];
for(const entry of entries)replay(entry);
