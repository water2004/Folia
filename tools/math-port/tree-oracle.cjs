// Root expectations from pinned executable sources, before native implementation.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),npm=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const hashes={Node:'532000186fc65db8f227b7e69f1ed331ebdd58bca2d1984aa099d81f3cbcd1d0',Wrapper:'8729a810aabd6eb3ad22021464f2f27d86116fae9edc0f818527f82e517bf7d9'};
for(const [name,hash]of Object.entries(hashes)){const p='ts/core/Tree/'+name+'.ts',src=fs.readFileSync(path.join(root,'.reference/MathJax-src',p));assert.equal(crypto.createHash('sha256').update(src).digest('hex'),hash);assert.equal(fs.readFileSync(path.join(npm,p),'utf8').replaceAll('\r\n','\n'),src.toString().replaceAll('\r\n','\n'));}
const runtime={Node:'94174004b89f19bf3c98e8eec5fabdd0bcbf1e13e0b740963fa69554ad8a12a7',Wrapper:'8a0c647c4f22bba75dec4164f03889a29534ddff0129ebffe36439d2f81b7276'};
for(const [name,hash]of Object.entries(runtime)){const resolved=require.resolve('@mathjax/src/js/core/Tree/'+name+'.js',{paths:[path.join(root,'tools/mathjax-bundle')]});assert.equal(fs.realpathSync(resolved),fs.realpathSync(path.join(npm,'cjs/core/Tree',name+'.js')));assert.equal(crypto.createHash('sha256').update(fs.readFileSync(resolved)).digest('hex'),hash);}
const {AbstractNode:N,AbstractEmptyNode:EN}=require(path.join(npm,'cjs/core/Tree/Node.js'));
const {AbstractNodeFactory:NF}=require(path.join(npm,'cjs/core/Tree/NodeFactory.js'));
const {AbstractWrapper:W}=require(path.join(npm,'cjs/core/Tree/Wrapper.js'));
const {AbstractWrapperFactory:WF}=require(path.join(npm,'cjs/core/Tree/WrapperFactory.js'));
class A extends N{get kind(){return 'a';}}class D extends A{get kind(){return 'd';}}class E extends EN{get kind(){return 'e';}}
class V extends A{get kind(){return 'v';}record(x){(this.trace||(this.trace=[])).push(x);}setProperty(n,v){this.record('p:'+n);super.setProperty(n,v);}setChildren(c){this.record('children');super.setChildren(c);}appendChild(c){this.record('append');return super.appendChild(c);}}
class C extends A{get kind(){return 'c';}copy(){if(this.onCopy)this.onCopy();return super.copy();}}
class S extends A{get kind(){return 's';}appendChild(c){const result=super.appendChild(c);if(this.onAppend)this.onAppend(c);return result;}}
class T extends E{get kind(){return 't';}toString(){if(this.onString)this.onString();return 't';}}
class K extends A{get kind(){if(this.onKind)this.onKind();return 'k';}}
class Plain extends N{}class F extends A{get kind(){return 'f';}setProperty(n,v){F.leaked=this;super.setProperty(n,v);if(n==='stop')throw new TypeError('setter');}}
const cases=[];let currentCase;function begin(name){currentCase={name,observations:[]};cases.push(currentCase);return new NF({a:A,d:D,e:E,v:V});}function expect(v){currentCase.observations.push(['eq',v]);}function equalList(v){currentCase.observations.push(['list',[...v]]);}function throws(fn){let e;try{fn();}catch(error){e=error;}assert.ok(e,'Expected upstream throw');currentCase.observations.push(['error',e.name]);if(e.message.includes('is not iterable'))currentCase.observations.push(['message',e.message]);currentCase.observations.push(['caught',true]);}function end(){}
{
 const f=begin('initialization_virtual_and_properties'),c=f.create('a');;const props={'10':10,'2':2,z:7,a:9},r=f.create('v',props,[c]);;equalList(r.trace);equalList(r.getPropertyNames());expect(r.parent===null);expect(c.parent===r);expect(r.getAllProperties()===props);
 const live=r.getAllProperties();live.z=8;;expect(r.getProperty('z'));delete live['2'];r.setProperty('2',99);;equalList(r.getPropertyNames());
 r.removeProperty('z','missing');;equalList(r.getPropertyNames());expect(r.getProperty('missing')===undefined);expect(r.getProperty('toString')===Object.prototype.toString);end();
}
{
 const f=begin('parent_aliases_and_replacements');const p=f.create('a'),q=f.create('a'),a=f.create('a'),b=f.create('d'),old=p.childNodes;;p.appendChild(a);q.appendChild(a);;expect(p.childNodes[0]===a);expect(a.parent===q);p.replaceChild(b,a);;expect(b.parent===p);expect(a.parent===q);expect(p.replaceChild(a,q)===a);
 const supplied=[a,b];p.setChildren(supplied);;expect(p.childNodes===supplied);expect(p.childNodes===old);expect(old.length);expect(a.parent===p);
 const current=p.childNodes;p.setChildren(current);;expect(current===p.childNodes);expect(p.childNodes.length);expect(p.replaceChild(a,a)===a);;;expect(a.parent===null);q.appendChild(b);p.removeChild(b);;expect(b.parent===null);expect(q.childNodes[1]===b);expect(p.childIndex(q)===null);end();
}
{
 const f=begin('null_partial_mutations');const p=f.create('a'),a=f.create('a');;throws(()=>p.appendChild(null));expect(p.childNodes.length);expect(p.childIndex(null));throws(()=>p.replaceChild(a,null));expect(p.childNodes[0]===a);expect(a.parent===p);throws(()=>p.replaceChild(null,a));expect(a.parent===p);throws(()=>p.removeChild(null));expect(p.childNodes.length);
 p.appendChild(a);;throws(()=>p.setChildren(null));expect(p.childNodes.length);expect(a.parent===p);expect(p.removeChild(null)===null);throws(()=>f.create('a',null));throws(()=>f.create('a',{},null));end();
}
{
 const f=begin('empty_overrides_and_class_identity');const a=f.create('a'),d=f.create('d'),e=f.create('e',{p:7},[a]);;expect(e.childNodes.length);expect(a.parent===null);expect(e.appendChild(null)===null);expect(e.replaceChild(d,a)===a);expect(e.childIndex(null)===null);expect(e.toString());expect(d.isKind('a'));expect(e.isKind('a'));f.setNodeClass('a',D);;expect(a.isKind('a'));expect(d.isKind('a'));throws(()=>a.isKind('missing'));
 e.childNodes=[a];;let seen=[];e.walkTree(n=>seen.push(n.kind));equalList(seen);expect(e.childIndex(a)===null);e.removeChild(a);;expect(e.childNodes.length);end();
}
{
 const f=begin('copy_spread_and_null_children');const p=f.create('a',{v:7}),a=f.create('d'),e=f.create('e');p.appendChild(a);p.childNodes.push(null,e);Object.defineProperty(p.properties,'__proto__',{value:13,enumerable:true,writable:true,configurable:true});;const c=p.copy();expect(c!==p);expect(c.parent===null);expect(c.properties!==p.properties);expect(Object.hasOwn(c.properties,'__proto__'));expect(c.getProperty('__proto__')===Object.prototype);expect(c.childNodes.length);;expect(c.childNodes[0]!==a);expect(c.childNodes[0].parent===c);expect(c.childNodes[1].toString());;expect(p.toString());p.childNodes=null;;expect(p.copy().childNodes.length);throws(()=>p.toString());end();
}
{
 const f=begin('live_walk_array_replacement');const p=f.create('a'),a=f.create('d'),e=f.create('e'),late=f.create('v');p.setChildren([a,e]);;const original=p.childNodes,data={n:0};let seen=[];const result=p.walkTree((n,d)=>{seen.push(n.kind);d.n++;if(n===a){original.push(late);p.childNodes=[];}},data);;equalList(seen);expect(data.n);expect(result===data);expect(p.childNodes.length);
 p.childNodes=null;;let visits=0;throws(()=>p.walkTree(()=>visits++));expect(visits);end();
}
{
 const f=begin('walk_reads_array_after_self_and_find');const p=f.create('a'),a=f.create('d'),e=f.create('e');;let seen=[];p.walkTree(n=>{seen.push(n.kind);if(n===p)p.childNodes=[a,null,e];});;equalList(seen);const found=p.findNodes('a');expect(found.length);expect(found[0]===p);expect(found[1]===a);throws(()=>p.findNodes('missing'));end();
}
{
 const f=begin('wrapper_storage_and_dispatch');const a=f.create('a'),e=f.create('e');class Spy extends WF{constructor(){super({a:W,e:W});this.calls=[];}wrap(n,...args){this.calls.push(args.length);return super.wrap(n,...args);}}const wf=new Spy(),w=wf.wrap(a);;expect('childNodes' in w);expect(w.node===a);expect(w.kind);const other=w.wrap(e,'dropped');;expect(wf.calls[1]);expect(other.node===e);w.node=e;;expect(w.kind);w.node=null;;throws(()=>w.kind);end();
}
{
 const f=begin('wrapper_live_walk_presence');const a=f.create('a'),e=f.create('e');const wf=new WF({a:W,e:W});const w=wf.wrap(a),x=wf.wrap(e),late=wf.wrap(a),data={v:0};let seen=[];w.walkTree(()=>seen.push('self'));;equalList(seen);
 w.childNodes=[x,null];const original=w.childNodes;seen=[];const result=w.walkTree((n,d)=>{seen.push(n.kind);d.v++;if(n===x){original.push(late);w.childNodes=[];}},data);;equalList(seen);expect(data.v);expect(result===data);
 w.childNodes=null;;let visits=0;throws(()=>w.walkTree(()=>visits++));expect(visits);end();
}
{
 const f=begin('copy_live_mutation_rebinding_and_spread_alias');f.setNodeClass('c',C);const p=f.create('a'),c=f.create('c'),late=f.create('e'),value={v:7};p.setProperty('alias',value);p.setChildren([c]);const original=p.childNodes;c.onCopy=()=>{original.push(late);p.childNodes=[];};;const copied=p.copy();expect(copied.childNodes.length);expect(p.childNodes.length);expect(copied.getProperty('alias')===value);;expect(copied.childNodes[1].kind);expect(copied.childNodes[1].parent===copied);;
 f.setNodeClass('a',D);;expect(p.copy().kind);expect(p.copy(true).kind);end();
}
{
 const f=begin('live_setChildren_and_join_evaluation');f.setNodeClass('s',S);f.setNodeClass('t',T);f.setNodeClass('k',K);const p=f.create('s'),a=f.create('a'),d=f.create('d'),input=[a];p.onAppend=n=>{if(n===a)input.push(d);};p.setChildren(input);;expect(p.childNodes.length);expect(d.parent===p);
 const root=f.create('a'),t=f.create('t'),e=f.create('e'),late=f.create('a');root.childNodes=[t,e];let captured=root.childNodes;t.onString=()=>{captured.push(late);root.childNodes=[];};;expect(root.toString());
 root.childNodes=[t,e,e];captured=root.childNodes;t.onString=()=>{captured.length=1;};;expect(root.toString());
 const k=f.create('k');k.childNodes=null;k.onKind=()=>{k.childNodes=[];};;expect(k.toString());end();
}
{
 const f=begin('copy_assignment_prototype_and_wrapper_empty_array');const p=f.create('a'),proto={inherited:7};Object.defineProperty(p.properties,'__proto__',{value:proto,enumerable:true});const c=p.copy();;expect(Object.hasOwn(c.properties,'__proto__'));expect(c.getProperty('inherited'));expect(Object.getPrototypeOf(c.properties)===proto);
 const wf=new WF({a:W}),w=wf.wrap(p),data={n:0};w.childNodes=[];const result=w.walkTree((n,d)=>d.n++,data);;expect(data.n);expect(result===data);end();
}
{
 const f=begin('join_recursive_array_suppression_and_recovery');const p=f.create('a'),d=f.create('d');p.childNodes=[p];;expect(p.toString());const shared=[d];p.childNodes=shared;d.childNodes=shared;;expect(p.toString());d.childNodes=[];;expect(p.toString());end();
}
{
 const f=begin('join_abrupt_conversion_unwinds_active_array');f.setNodeClass('t',T);const p=f.create('a'),t=f.create('t');p.childNodes=[t];t.onString=()=>{throw new TypeError('conversion');};;throws(()=>p.toString());t.onString=null;;expect(p.toString());end();
}
{
 const f=begin('default_kind_data_and_abrupt_virtual_initialization');f.setNodeClass('unknown',Plain);f.setNodeClass('f',F);const p=f.create('unknown',{id:'retained'}),a=f.create('a');;expect(p.kind);expect(p.toString());expect(p.copy().getProperty('id'));expect(p.copy(true).getProperty('id'));let absent=false;const data=p.walkTree((n,d)=>{absent=d===undefined;});;expect(absent);expect(data===undefined);throws(()=>f.create('f',{early:1,stop:2,late:3},[a]));expect(F.leaked!==undefined);;equalList(F.leaked.getPropertyNames());expect(F.leaked.childNodes.length);expect(a.parent===null);;end();
}

const report=({cases});const gold=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/tree-source.json'),'utf8'));assert.ok(process.argv.includes('--check'),'Source owner is check-only');assert.deepEqual(report,gold);console.log(JSON.stringify({profile:'tree-source.json',cases:report.cases?.length??report.rows?.length,mode:'check'}));
