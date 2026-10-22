// Root Source observations for the getForms single-space-length boundary.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,M}=require('./mo-pattern-reference.cjs');
const base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js'));
const {MmlMrow}=require(path.join(base,'core/MmlTree/MmlNodes/mrow.js'));
let formLength,formPosition,formMo,formEvents;
class Row extends MmlMrow {
 get isEmbellished(){return false;}
 nonSpaceLength(){formEvents.push('length');return formLength;}
 firstNonSpace(){formEvents.push('first');return formPosition==='first'?formMo:null;}
 lastNonSpace(){formEvents.push('last');return formPosition==='last'?formMo:null;}
}
function run(length,position){formLength=length;formPosition=position;const f=new MmlFactory();f.setNodeClass('forms-row',Row);formMo=f.create('mo');formMo.parent=f.create('forms-row');formEvents=[];return formMo.getForms().join(',')+'|'+formEvents.join(',');}
const rows=[];for(const length of [0,1,2])for(const position of ['first','last','neither'])rows.push({length,position,expected:run(length,position)});
const data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'docs/math-port/evidence/p37-forms-boundary-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);}else{assert.ok(!fs.existsSync(profile));fs.writeFileSync(profile,data);}
console.log(JSON.stringify({source_calls:rows.length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
