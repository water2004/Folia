// Root independent original Math.verifyTree virtual callback/defaulting witness.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory}=require('./inheritance-reference.cjs');
const packageLock=require(path.join(root,'tools/math-port/mo-pattern-reference.cjs')).lock;
assert.equal(packageLock.version,'4.1.3');
assert.equal(packageLock.node,process.version);
assert.deepEqual(packageLock.compiled_package,{files:424,sha256:'773bf3ae1c075baa36c02b6958f7e3bed5fb116d0818ecf99bb70c5337d9e537'});
const source='ts/core/MmlTree/MmlNodes/math.ts';
const sourceBytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));
const bundledSource=fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8');
assert.equal(crypto.createHash('sha256').update(sourceBytes).digest('hex'),'ca0769f26d00145b2c520ac7bcfb1ac1f3e3eb34c387d0eeb8e4444e7d382ea0');
assert.equal(bundledSource.replaceAll('\r\n','\n'),sourceBytes.toString('utf8').replaceAll('\r\n','\n'));
const runtime=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNodes/math.js');
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(runtime)).digest('hex'),'44debed089b254bc09e73e22d4c30336db13ff8dad0af140e915a320bdda8e71');
const {MmlMath}=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNodes/math.js'));
const inputs=[];for(const option of ['U','N','object','false','raw','array'])for(const parent of [false,true])for(const mode of ['','attrs-throw','attrs-parent-null','children-throw','children-parent-null','children-options-set'])inputs.push({option,parent,mode});
const scalar=v=>v===undefined?'U':v===null?'N':typeof v==='boolean'?String(v):typeof v==='string'?'s:'+v:Array.isArray(v)?'a/'+String(v.checkArity):'o/'+String(v.checkArity);
function run(i){let active=false,events=[],opts,n,p;const note=s=>{if(active)events.push(s);},fail=()=>{throw Object.assign(new Error('witness'),{name:'WitnessError'});};
 class Probe extends MmlMath{verifyAttributes(o){note('attrs/'+scalar(o)+'/'+Number(o===opts));if(i.mode==='attrs-throw')fail();if(i.mode==='attrs-parent-null')n.parent=null;}verifyChildren(o){note('children/'+scalar(o)+'/'+Number(o===opts));if(i.mode==='children-throw')fail();if(i.mode==='children-parent-null')n.parent=null;if(i.mode==='children-options-set'&&o&&typeof o==='object')o.checkArity=false;}mError(s,o,short){note('error/'+s+'/'+scalar(o)+'/'+Number(o===opts)+'/'+String(short));return p;}}
 const f=new MmlFactory();f.setNodeClass('probe-math',Probe);n=f.create('probe-math');p=f.create('mrow');n.parent=i.parent?p:null;n.childNodes=[];opts=i.option==='U'?undefined:i.option==='N'?null:i.option==='object'?{checkArity:true}:i.option==='false'?false:i.option==='raw'?'raw':Object.assign([],{checkArity:true});
 let error='ok';active=true;try{n.verifyTree(opts);}catch(e){error=e.name+'|'+e.message;}active=false;return error+'|'+scalar(opts)+'|'+Number(n.parent===p)+'|'+events.join(',');
}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows)+'\n';
const profile=path.join(root,'tools/math-port/profiles/evidence/p35-math-verification-source.json');
assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);
console.log(JSON.stringify({source_calls:rows.length,source_errors:rows.filter(r=>!r.expected.startsWith('ok|')).length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
