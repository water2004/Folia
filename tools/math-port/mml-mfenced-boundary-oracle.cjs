// Root original concat/sparse snapshot and nullable text-setter observations.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory,MmlMfenced,core}=require('./mfenced-reference.cjs');
let profileMode,profileTarget,profileLate,profileLog;
class Record extends core.AbstractMmlNode {get kind(){return 'boundary-record';}setInheritedAttributes(){profileLog.push(this.label);if(profileMode==='rebind'&&this.label==='open'){profileTarget.close=null;profileTarget.separators=[];profileTarget.childNodes=[profileLate];}}}
class Probe extends MmlMfenced {addFakeNodes(){}hook(){this.setChildInheritedAttributes({},false,0,false);}fake(){return this.fakeNode('ζ');}}
class NullSetter extends core.TextNode {setText(t){super.setText(t);return null;}}
class Factory extends MmlFactory {create(kind,...args){if(kind==='text'&&profileMode==='text-null')return null;if(kind==='text'&&profileMode==='text-wrong')return super.create('mi');if(kind==='text'&&profileMode==='setter-null')return super.create('null-setter',...args);if(kind==='mo'&&profileMode==='mo-null')return null;return super.create(kind,...args);}}
function run(family,mode){profileMode=mode;profileLog=[];const f=new Factory();f.setNodeClass('boundary-record',Record);f.setNodeClass('boundary-fenced',Probe);f.setNodeClass('null-setter',NullSetter);const n=f.create('boundary-fenced');let result='THREW',error='ok';try{if(family==='snapshot'){const make=label=>{const r=f.create('boundary-record');r.label=label;return r;};const o=make('open'),c=make('close'),s=make('separator'),r=make('real');profileLate=make('late');profileTarget=n;n.open=o;n.close=c;n.childNodes=[r];if(mode==='null')n.separators=null;else if(mode==='empty')n.separators=[];else if(mode==='sparse'){n.separators=new Array(3);n.separators[1]=null;n.separators[2]=s;}else if(mode.startsWith('prototype-')){n.separators=new Array(mode==='prototype-inside'?2:0);const p=[];p[0]=s;Object.setPrototypeOf(n.separators,p);}else n.separators=[s];n.hook();result=profileLog.join(',');}else{const node=n.fake(),child=node.childNodes[0];result=String(node.texClass)+'|'+(node.parent===n?'parent':'other')+'|'+(child===null?'N':child===undefined?'U':child.getText());}}catch(e){error=e.name+'|'+e.message;}return error+'|'+result;}
const rows=[];for(const mode of ['null','empty','sparse','prototype-inside','prototype-outside','rebind'])rows.push({family:'snapshot',mode,expected:run('snapshot',mode)});for(const mode of ['normal','setter-null','text-null','text-wrong','mo-null'])rows.push({family:'fake',mode,expected:run('fake',mode)});
const data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'tools/math-port/profiles/evidence/p39-mfenced-boundary-source.json');
const normalize=text=>text.replaceAll('\r\n','\n');
if(process.argv.includes('--check')) assert.equal(normalize(fs.readFileSync(profile,'utf8')),data);
else {assert.ok(!fs.existsSync(profile));fs.writeFileSync(profile,data);}
console.log(JSON.stringify({source_calls:rows.length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
