// Independent additional calls to the pinned original verifyTree method.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {run}=require('./mml-verification-oracle.cjs');
const rows=[];
for(const options of ['object','array'])for(const arity of ['-Inf','-1','-0','0','1','2','Inf','NaN'])for(const shape of ['','a','ab','null'])for(const check of ['N','0','-0','1','NaN','empty','raw','obj','array']){
 const input={op:'tree',options,arity,shape,check,mode:''};rows.push({input,expected:run(input)});
}
const data=JSON.stringify(rows)+'\n',file=path.resolve(__dirname,'../../tools/math-port/profiles/evidence/p32-verification-flags-source.json');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(file,'utf8').replaceAll('\r\n','\n'),data);
else{assert.ok(!fs.existsSync(file));fs.writeFileSync(file,data);}
console.log(JSON.stringify({source_calls:rows.length,source_errors:rows.filter(r=>!r.expected.startsWith('ok|')).length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
