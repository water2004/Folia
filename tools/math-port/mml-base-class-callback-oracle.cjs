// Root actual original base methods under virtual callbacks, aliasing and tagged nullish inputs.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');const{root,MmlFactory,core}=require('./inheritance-reference.cjs');
const modes=['','throw-n-P','throw-n-L','throw-n-C','throw-p-L','throw-p-P','mutate-n-P','attrs-null','attrs-rebind','skip-hook','null-hook','get-current-change'];
const inputs=[];for(const op of ['set','update','previous'])for(const value of ['N','U','self','p'])for(const mode of modes)for(const current of ['0','N','U'])inputs.push({op,value,mode,current});
const scalar=v=>v===undefined?'U':v===null?'N':String(v),raw=v=>v==='N'?null:v==='U'?undefined:Number(v);
function run(i){let active=false,trace=[],n,p;function note(s){if(active)trace.push(s);}function action(who,key){if(!active)return;if(i.mode==='throw-'+who+'-'+key)throw Object.assign(new Error('witness'),{name:'WitnessError'});if(i.mode==='mutate-n-P'&&who==='n'&&key==='P')p.pl=9;}
 class Probe extends core.AbstractMmlNode{
  get kind(){return 'probe';}get texClass(){note(this.id+'.c');const v=this.cur;if(active&&i.mode==='get-current-change'&&this.id==='n')this.cur=null;return v;}set texClass(v){note(this.id+'.C='+scalar(v));this.cur=v;action(this.id,'C');}
  get prevClass(){note(this.id+'.p');return this.pc;}set prevClass(v){note(this.id+'.P='+scalar(v));this.pc=v;action(this.id,'P');}
  get prevLevel(){note(this.id+'.l');return this.pl;}set prevLevel(v){note(this.id+'.L='+scalar(v));this.pl=v;action(this.id,'L');}
  get attributes(){note(this.id+'.a');const a=this.a;if(active&&i.mode==='attrs-rebind'&&this.id==='p')this.a=null;return a;}set attributes(v){this.a=v;}
  getPrevClass(v){note(this.id+'.hook');if(active&&i.mode==='skip-hook')return;if(active&&i.mode==='null-hook'){this.cur=null;return;}return core.AbstractMmlNode.prototype.getPrevClass.call(this,v);}
 }
 const f=new MmlFactory();f.setNodeClass('probe',Probe);n=f.create('probe');p=f.create('probe');n.id='n';p.id='p';n.cur=raw(i.current);n.pc=11;n.pl=7;p.cur=2;p.pc=5;p.pl=3;p.a.set('scriptlevel',3);if(i.mode==='attrs-null')p.a=null;
 const arg=i.value==='N'?null:i.value==='U'?undefined:i.value==='self'?n:p;let result='U',error='ok';active=true;
 try{if(i.op==='set'){const v=n.setTeXclass(arg);result=v===n?'self':v===p?'p':v===undefined?'U':v===null?'N':'UNBOUND';}else n[i.op==='update'?'updateTeXclass':'getPrevClass'](arg);}catch(e){error=e.name+'|'+e.message;result='THREW';}active=false;
 return error+'|'+result+'|'+[n.cur,n.pc,n.pl,p.cur,p.pc,p.pl].map(scalar).join(',')+'|'+Number(n.a!==null)+','+Number(p.a!==null)+'|'+trace.join(',');
}
const rows=inputs.map(input=>({input,expected:run(input)})),data=JSON.stringify(rows,null,2)+'\n',profile=path.join(root,'docs/math-port/evidence/p24-base-class-callback-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(profile,'utf8').replaceAll('\r\n','\n'),data);}else{assert.ok(!fs.existsSync(profile));fs.writeFileSync(profile,data);}
console.log(JSON.stringify({source_calls:rows.length,original_errors:rows.filter(r=>!r.expected.startsWith('ok|')).length,sha256:crypto.createHash('sha256').update(data).digest('hex')}));
