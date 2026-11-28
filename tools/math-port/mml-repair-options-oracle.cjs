// Root original raw verification-option reads and preceding graph effects.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory,classes,core}=require('./table-reference.cjs');
const source='ts/core/MmlTree/MmlNodes/mmultiscripts.ts';
const sourceBytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));
const bundledSource=fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8');
assert.equal(crypto.createHash('sha256').update(sourceBytes).digest('hex'),'bb8d8d2180ef483b86b0b1570b919ba424ed54db1f30ef6e0e697cfab0bf5cf7');
assert.equal(bundledSource.replaceAll('\r\n','\n'),sourceBytes.toString('utf8').replaceAll('\r\n','\n'));
const runtime=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNodes/mmultiscripts.js');
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(runtime)).digest('hex'),'db40a3543cffbd706c0070ae0877285ce22b07db3a7e530e56f42eaa7b2d922e');
const Multi=new MmlFactory().getNodeClass('mmultiscripts');
const inputs=[];for(const kind of ['table','row','multi'])for(const child of [false,true])for(const option of ['U','N','object'])inputs.push({kind,child,option});
function run(i){let active=false,trace=[],nodes=[],original;const note=s=>{if(active)trace.push(s);};const Probe=Base=>class extends Base{hook(o){super.verifyChildren(o);}};class Leaf extends core.AbstractMmlNode{get kind(){return 'verify-leaf';}verifyTree(o){note('leaf/'+(o===original?'original':'other'));}}class Factory extends MmlFactory{create(k,...args){note('factory/'+k+'/'+args.length);const n=super.create(k,...args);if(n&&!nodes.includes(n))nodes.push(n);return n;}}
 const f=new Factory();f.setNodeClass('mtable',Probe(classes.mtable));f.setNodeClass('mtr',Probe(classes.mtr));f.setNodeClass('mmultiscripts',Probe(Multi));f.setNodeClass('verify-leaf',Leaf);const target=f.create({table:'mtable',row:'mtr',multi:'mmultiscripts'}[i.kind]),leaf=f.create('verify-leaf');target.childNodes=i.child?[leaf]:[];if(i.child)leaf.parent=target;original=i.option==='U'?undefined:i.option==='N'?null:{fixMtables:true,fixMmultiscripts:true};let error='ok';active=true;try{target.hook(original);}catch(e){error=e.name+'|'+e.message;}active=false;const tag=n=>n===undefined?'U':n===null?'N':String(nodes.indexOf(n));return error+'|'+nodes.map(n=>n.kind+'/'+tag(n.parent)+'/'+(n.childNodes===null?'N':n.childNodes.map(tag).join(','))).join(';')+'|'+trace.join(',');}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'tools/math-port/profiles/evidence/p36-repair-options-source.json');
assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);
console.log(JSON.stringify({source_calls:rows.length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
