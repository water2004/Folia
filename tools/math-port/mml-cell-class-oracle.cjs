// Root observation of original cell literal-slot class dispatch and errors.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory,classes,core}=require('./table-reference.cjs');
const inputs=[];for(const previous of [false,true])for(const mode of ['null-array','empty','hole','null','child','prototype','throw'])inputs.push({previous,mode});
function run(i){let active=false,trace=[];const note=s=>{if(active)trace.push(s);};class Cell extends classes.mtd{getPrevClass(prev){note('previous/'+(prev?'node':'N'));super.getPrevClass(prev);}}class Child extends core.AbstractMmlNode{get kind(){return 'class-child';}setTeXclass(prev){note('child/'+String(prev));if(i.mode==='throw')throw Object.assign(new Error('witness'),{name:'WitnessError'});return this;}}
 const f=new MmlFactory();f.setNodeClass('mtd',Cell);f.setNodeClass('class-child',Child);const cell=f.create('mtd'),child=f.create('class-child'),prev=f.create('mi');prev.texClass=3;const a=i.mode==='null-array'?null:i.mode==='empty'?[]:i.mode==='hole'?new Array(1):i.mode==='null'?[null]:i.mode==='prototype'?Object.assign(new Array(1),{}):[child];if(i.mode==='prototype')Object.setPrototypeOf(a,{0:child});cell.childNodes=a;
 let error='ok',result='THREW';active=true;try{result=cell.setTeXclass(i.previous?prev:null)===cell?'cell':'other';}catch(e){error=e.name+'|'+e.message;}active=false;return [error,result,String(cell.prevClass),String(cell.prevLevel),trace.join(',')].join('|');}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'docs/math-port/evidence/p36-cell-class-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);}else{assert.ok(!fs.existsSync(profile));fs.writeFileSync(profile,data);}
console.log(JSON.stringify({source_calls:rows.length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
