// Root original protected-method boundary observations, including actual native stub diagnostics.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory,core}=require('./inheritance-reference.cjs');class Probe extends core.AbstractMmlNode{get kind(){return 'probe';}}
const rows=[];for(const op of ['setTeXclass','updateTeXclass','getPrevClass']){const f=new MmlFactory();f.setNodeClass('probe',Probe);const n=f.create('probe'),p=f.create('probe');n.texClass=0;n.prevClass=11;n.prevLevel=7;p.texClass=2;p.prevClass=5;p.prevLevel=3;p.attributes.set('scriptlevel',3);const result=n[op](op==='setTeXclass'?null:p);rows.push({op,self:result===n,n:[n.texClass,n.prevClass,n.prevLevel],p:[p.texClass,p.prevClass,p.prevLevel]});}
const profile=JSON.stringify(rows,null,2)+'\n',profileFile=path.join(root,'docs/math-port/evidence/p24-base-class-boundary-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(profileFile,'utf8').replaceAll('\r\n','\n'),profile);}else{assert.ok(!fs.existsSync(profileFile));fs.writeFileSync(profileFile,profile);}
console.log(JSON.stringify({original_calls:rows.length,sha256:crypto.createHash('sha256').update(profile).digest('hex')}));
