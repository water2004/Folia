// Root independent original inheritance repair and partial-effect observations.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory,classes}=require('./table-reference.cjs');
const inputs=[];for(const kind of ['table','row'])for(const shape of ['mi','mi-mi','mi-valid-mi','valid'])for(const mode of ['','factory-null','factory-throw','replace-throw','replace-null','append-throw','append-null','inherit-throw'])inputs.push({kind,shape,mode});
function run(i){let active=false,trace=[],nodes=[],target,captured;const tag=n=>n===null?'N':n===undefined?'U':String(nodes.indexOf(n)),note=s=>{if(active)trace.push(s);},fail=()=>{throw Object.assign(new Error('witness'),{name:'WitnessError'});};
 const probe=Base=>class extends Base{hook(){super.setChildInheritedAttributes({},true,2,'prime');}setInheritedAttributes(a,d,l,p){note('inherit/'+tag(this)+'/'+[d,l,p].map(String).join(','));if(i.mode==='inherit-throw')fail();}replaceChild(n,o){note('replace/'+tag(n)+'/'+tag(o));if(i.mode==='replace-throw')fail();const result=super.replaceChild(n,o);return i.mode==='replace-null'?null:result;}appendChild(n){note('append/'+tag(this)+'/'+tag(n));if(active&&i.mode==='append-throw')fail();const result=super.appendChild(n);return active&&i.mode==='append-null'?null:result;}};
 class Factory extends MmlFactory{create(k,...args){note('factory/'+k+'/'+args.length);if(active&&(k==='mtr'||k==='mtd')){if(i.mode==='factory-throw')fail();if(i.mode==='factory-null')return null;}const n=super.create(k,...args);if(n&&!nodes.includes(n))nodes.push(n);return n;}}
 const f=new Factory();f.setNodeClass('mtable',probe(classes.mtable));f.setNodeClass('mtr',probe(classes.mtr));f.setNodeClass('mtd',probe(classes.mtd));target=f.create(i.kind==='table'?'mtable':'mtr');const slots=i.shape.split('-').map(k=>f.create(k==='valid'?(i.kind==='table'?'mtr':'mtd'):'mi'));target.childNodes=slots;for(const n of slots)n.parent=target;captured=slots;target.attributes.set('columnalign','center');target.attributes.set('rowalign','baseline');target.attributes.set('data-break-align','top');
 let error='ok';active=true;try{target.hook();}catch(e){error=e.name+'|'+e.message;}active=false;
 const array=a=>a===null?'N':a.map(tag).join(','),state=nodes.map(n=>[n.kind,tag(n.parent),array(n.childNodes)].join('/')).join(';');return error+'|'+state+'|captured/'+array(captured)+'|'+trace.join(',');}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';

const profile=process.env.FOLIA_TABLE_REPAIR_PROFILE||path.join(root,'docs/math-port/evidence/p36-table-repair-source.json');
assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data,'Frozen table repair Source profile changed');
console.log(JSON.stringify({sourceCases:rows.length,profileSha256:crypto.createHash('sha256').update(fs.readFileSync(profile)).digest('hex'),mode:'check'}));
