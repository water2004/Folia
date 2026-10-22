'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root}=require(path.join(__dirname,'mo-pattern-reference.cjs'));
const base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js')),{MmlMaction}=require(path.join(base,'core/MmlTree/MmlNodes/maction.js'));
class Probe extends MmlMaction{hook(){this.setChildInheritedAttributes({},false,0,false);}}
const inputs=[['U',undefined],['N',null],['false',false],['zero',0],['array',[]],['object',{}],['tooltip','TOOLTIP'],['toggle','toggle']];
const rows=inputs.map(([input,value])=>{const f=new MmlFactory();f.setNodeClass('action-errors',Probe);const n=f.create('action-errors');n.attributes.set('actiontype',value);let expected='ok';try{n.hook();}catch(e){expected=e.name+'|'+e.message;}return {input,expected};});
const profile=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence',"p38-action-errors-source.json"),'utf8'));
assert.deepEqual(rows,profile);
const data=JSON.stringify(rows)+'\n';
assert.equal(data,fs.readFileSync(path.join(root,'docs/math-port/evidence',"p38-action-errors-source.json"),'utf8').replaceAll('\r\n','\n'));
console.log(JSON.stringify({sourceCases:rows.length}));
