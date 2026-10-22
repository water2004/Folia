// Replay the same canonical Source owners against complete frozen Source profiles.
'use strict';
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const tools=process.env.FOLIA_SOURCE_CHECK_TOOLS||path.join(root,'tools/math-port');
function replay(entry){const profile=path.join(root,entry.profile);assert.equal(crypto.createHash('sha256').update(fs.readFileSync(profile)).digest('hex'),entry.sha256,'Frozen Source profile changed: '+entry.profile);const result=cp.spawnSync(process.execPath,[path.join(tools,entry.owner),'--check'],{cwd:root,encoding:'utf8',windowsHide:true,timeout:60000,maxBuffer:1024*1024});assert.ifError(result.error);assert.equal(result.status,0,entry.owner+' '+result.stderr);console.log(result.stdout.trim());}
const entries=[
  {
    "owner": "mo-order-oracle.cjs",
    "profile": "docs/math-port/evidence/p24-mo-order-source.json",
    "sha256": "c733452b9d20788f5fd86eb2f9b07c3ae3edc35a43ea8b0cae16ff548d9073ad"
  },
  {
    "owner": "arithmetic-oracle.cjs",
    "profile": "docs/math-port/evidence/source-arithmetic-profile.json",
    "sha256": "d9e33d8e54cdbc8015834be3cd27fee76d1c6e7cf658a0c71c679cfbda014f55"
  },
  {
    "owner": "arithmetic-order-oracle.cjs",
    "profile": "docs/math-port/evidence/source-arithmetic-order-profile.json",
    "sha256": "9d7afae4ae5c2100b0b4c9f2a2672209707a574f6dae4555d4aa4eddd848a6fc"
  },
  {
    "owner": "table-oracle.cjs",
    "profile": "docs/math-port/evidence/table-source-profile.json",
    "sha256": "390573beb4e780eea0c806cae5ed68e974264f5c76499f9892df1a1cd3686de5"
  },
  {
    "owner": "atom-semantics-oracle.cjs",
    "profile": "docs/math-port/evidence/source-atom-semantics-profile.json",
    "sha256": "73a09c4d0efce2a407f858aebf9991840323ec2372bc93d505311b040fffdfa6"
  },
  {
    "owner": "multiscript-oracle.cjs",
    "profile": "docs/math-port/evidence/multiscript-source-profile.json",
    "sha256": "d70c7f2afacf34a333c4581e65c0d9f45df2f7c558646b1c25a7ebc2691b0997"
  }
];
for(const entry of entries)replay(entry);
