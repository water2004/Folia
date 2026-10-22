// Root observation of the pinned original mrow class callback boundaries.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory,core}=require('./inheritance-reference.cjs');
const source='ts/core/MmlTree/MmlNodes/mrow.ts',scope=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json'))),entry=scope.files.find(x=>x.source===source);
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'.reference/MathJax-src',source))).digest('hex'),entry.source_sha256);
const RowBase=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNodes/mrow.js')).MmlMrow;
const inputs=[];for(const delimited of [false,true])for(const previous of [false,true])for(const mode of ['','empty','null','rebind','grow','return-prev','throw'])inputs.push({delimited,previous,mode});
function run(i){let active=false,trace=[],row,a,b,captured;const note=s=>{if(active)trace.push(s);},tag=n=>n===undefined?'U':n===null?'N':n===row?'row':n===a?'a':n===b?'b':'prev';
 class Child extends core.AbstractMmlNode{get kind(){return 'row-child';}setTeXclass(prev){note('child/'+tag(this)+'/'+tag(prev));if(this===a){if(i.mode==='empty')row.childNodes=[];if(i.mode==='null')row.childNodes=null;if(i.mode==='rebind')row.childNodes=[b];if(i.mode==='grow')captured.push(b);if(i.mode==='throw')throw Object.assign(new Error('witness'),{name:'WitnessError'});if(i.mode==='return-prev')return prev;}return this;}}
 class Row extends RowBase{getProperty(name){note('property/'+name);return super.getProperty(name);}getPrevClass(prev){note('previous/'+tag(prev));super.getPrevClass(prev);}updateTeXclass(child){note('update/'+tag(child));super.updateTeXclass(child);}}
 const f=new MmlFactory();f.setNodeClass('row-child',Child);f.setNodeClass('mrow',Row);row=f.create('mrow');a=f.create('row-child');b=f.create('row-child');const previous=f.create('mi');previous.texClass=1;row.childNodes=[a];captured=row.childNodes;a.texClass=2;b.texClass=3;a.prevClass=4;a.prevLevel=5;row.texClass=null;if(i.delimited)row.setProperty('open','');
 let error='ok',result='THREW';active=true;try{result=tag(row.setTeXclass(i.previous?previous:null));}catch(e){error=e.name+'|'+e.message;}active=false;
 const state=n=>[n.texClass,n.prevClass,n.prevLevel].map(String).join(','),array=x=>x===null?'N':x.map(tag).join(',');return [error,result,state(row),state(a),state(b),array(row.childNodes),array(captured),trace.join(',')].join('|');}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'docs/math-port/evidence/p34-row-class-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);}else{assert.ok(!fs.existsSync(profile));fs.writeFileSync(profile,data);}
console.log(JSON.stringify({source_calls:rows.length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
