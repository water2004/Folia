// Root exact original delimiter string-method failures.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');const {root,MmlFactory,MmlMfenced}=require('./mfenced-reference.cjs');
class Probe extends MmlMfenced{add(){this.addFakeNodes();}}
const rows=[];for(const field of ['open','close','separators'])for(const [tag,value] of [['U',undefined],['N',null],['F',false]]){const f=new MmlFactory();f.setNodeClass('string-errors',Probe);const n=f.create('string-errors');n.attributes.set(field,value);let expected='ok';try{n.add();}catch(e){expected=e.name+'|'+e.message;}rows.push({field,tag,expected});}
const data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'tools/math-port/profiles/evidence/p39-mfenced-string-errors-source.json');
const normalize=text=>text.replaceAll('\r\n','\n');
if(process.argv.includes('--check')) assert.equal(normalize(fs.readFileSync(profile,'utf8')),data);
else {assert.ok(!fs.existsSync(profile));fs.writeFileSync(profile,data);}
console.log(JSON.stringify({source_calls:rows.length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
