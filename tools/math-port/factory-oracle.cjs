// Root-owned expectations from actual pinned JS, never native implementations.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),npmRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const sources={
 'Factory':'e61d2f5868287e2f382503cf690cd241698bf212957c1795d0cd35ce3bef049e',
 'NodeFactory':'b02c5ac2cabb61755785d35def6b57f8246322c626f548df600ca81af27b4599',
 'WrapperFactory':'8c59ed6a5e646e0736730070ba87161d7060f1f4a083e92afcc8012345537cea'
};
for(const [name,hash]of Object.entries(sources)){
 const source=fs.readFileSync(path.join(root,'.reference/MathJax-src/ts/core/Tree',name+'.ts'));
 assert.equal(crypto.createHash('sha256').update(source).digest('hex'),hash);
 assert.equal(fs.readFileSync(path.join(npmRoot,'ts/core/Tree',name+'.ts'),'utf8').replaceAll('\r\n','\n'),source.toString('utf8').replaceAll('\r\n','\n'));
}
const {AbstractFactory:F}=require(path.join(npmRoot,'cjs/core/Tree/Factory.js'));
const resolvedFactory=require.resolve('@mathjax/src/js/core/Tree/Factory.js',{paths:[path.join(root,'tools/mathjax-bundle')]});
assert.equal(fs.realpathSync(resolvedFactory),fs.realpathSync(path.join(npmRoot,'cjs/core/Tree/Factory.js')));
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(resolvedFactory)).digest('hex'),'739db2e5760c29d7bb4f1bc7baa08ff8d727f4d117522aff5ede017f812cffef');
const {AbstractNodeFactory:NF}=require(path.join(npmRoot,'cjs/core/Tree/NodeFactory.js'));
const {AbstractWrapperFactory:WF}=require(path.join(npmRoot,'cjs/core/Tree/WrapperFactory.js'));
class B{constructor(factory,...args){this.factory=factory;this.args=args;}get kind(){return 'reported-b';}}
class D extends B{get kind(){return 'reported-d';}}
class U{constructor(factory,...args){this.factory=factory;this.args=args;}get kind(){return 'reported-b';}}
const cases=[];let currentCase;function begin(name){currentCase={name,observations:[]};cases.push(currentCase);}function expect(v){currentCase.observations.push(['eq',v]);}function throws(fn){let e;try{fn();}catch(error){e=error;}assert.ok(e,'Missing source exception');currentCase.observations.push(['error',e.name],['caught',true]);}function kinds(f){currentCase.observations.push(['kinds',f.getKinds()]);}function end(){}
{
 begin('defaults');F.defaultNodes={first:B,unknown:U};;
 const f=new F();kinds(f);expect(f.create('absent').kind);
 const empty=new F({});;kinds(empty);throws(()=>empty.create('first'));
 class ChildF extends F{};ChildF.defaultNodes={own:D};const child=new ChildF();;kinds(child);expect(child.create('own').kind);
 F.defaultNodes={after:D};const later=new F();;kinds(later);kinds(f);F.defaultNodes={};end();
}
{
 begin('map_order_and_class_identity');const f=new F({'10':B,'2':D,alpha:U,'01':B,'4294967295':D});;kinds(f);
 f.setNodeClass('alpha',B);;kinds(f);f.deleteNodeClass('2');f.setNodeClass('2',D);;kinds(f);
 f.setNodeClass('alias',B);;expect(f.getNodeClass('alias')===f.getNodeClass('alpha'));
 const original=f.create('alpha'),derived=f.create('2'),unrelated=new U(f);;
 for(const node of [original,derived,unrelated])expect(f.nodeIsKind(node,'alpha'));
 f.setNodeClass('alpha',D);;expect(f.nodeIsKind(original,'alpha'));expect(f.nodeIsKind(derived,'alpha'));expect(f.nodeIsKind(original,'alias'));expect(f.create('alpha').kind);
 expect(f.nodeIsKind(null,'alias'));throws(()=>f.nodeIsKind(original,'missing'));expect(f.getNodeClass('missing')===undefined);
 f.setNodeClass('invalid',undefined);;kinds(f);throws(()=>f.create('invalid'));throws(()=>f.nodeIsKind(original,'invalid'));end();
}
{
 begin('capture_and_arguments');const f=new F({chosen:B,unknown:U});;const r=f.create('chosen',3,'arg',null);;expect(r.factory===f);expect(r.args.length);expect(r.args[0]);expect(r.args[1]);expect(r.args[2]===null);
 f.defaultKind='chosen';;expect(f.create('absent',4).kind);f.deleteNodeClass('chosen');;throws(()=>f.create('absent'));
 const metadata={v:7};B.defaults=metadata;;expect(B.defaults===metadata);
 const h=new F({chosen:B}),handle={v:7};const hr=h.create('chosen',handle,handle);;expect(hr.args[0]===handle);expect(hr.args[0]===hr.args[1]);end();
}
{
 begin('prototype_creator_retained');const f=new F({unknown:U});;throws(()=>f.create('__proto__'));f.setNodeClass('__proto__',B);;kinds(f);expect(f.create('__proto__').kind);f.deleteNodeClass('__proto__');;kinds(f);expect(f.create('__proto__').kind);expect(f.getNodeClass('__proto__')===undefined);throws(()=>f.nodeIsKind(f.create('__proto__'),'__proto__'));end();
}
{
 begin('node_factory_direct_and_defaults');const f=new NF({chosen:B,unknown:U});;throws(()=>f.create('absent'));
 const one=f.create('chosen'),two=f.create('chosen');;expect(one.args.length);expect(one.args[0]!==two.args[0]);expect(one.args[1]!==two.args[1]);
 const props={v:7},children=[one,null];const r=f.create('chosen',props,children);;expect(r.args[0]===props);expect(r.args[1]===children);
 const raw=f.create('chosen',undefined,null,'discarded');;expect(raw.args.length);expect(raw.args[1]===null);
 const all=f.create('chosen',null,null);;expect(all.args[0]===null);end();
}
{
 begin('wrapper_factory_forwarding');const f=new WF({chosen:B,unknown:U});;let reads=0;const node={get kind(){reads++;return 'chosen';}};const r=f.wrap(node,7,'extra');;expect(reads);expect(r.factory===f);expect(r.args.length);expect(r.args[0]===node);expect(r.args[1]);expect(r.args[2]);
 const missing={kind:'absent'};;expect(f.wrap(missing).kind);throws(()=>f.wrap(null));end();
}
{
 begin('independent_live_creator_object');const f=new F({chosen:B,unknown:U});const view=f.node,saved=view.chosen;;
 expect(view===f.node);f.nodeMap.set('chosen',D);;expect(f.getNodeClass('chosen')===D);expect(f.create('chosen').kind);expect(f.nodeIsKind(f.create('chosen'),'chosen'));
 f.setNodeClass('chosen',D);;expect(view===f.node);expect(saved().kind);expect(view.chosen().kind);
 view.alias=saved;;expect(f.create('alias').kind);expect(f.getNodeClass('alias')===undefined);throws(()=>f.nodeIsKind(f.create('alias'),'alias'));
 view.chosen=undefined;;expect(f.create('chosen').kind);expect(f.getNodeClass('chosen')===D);f.deleteNodeClass('chosen');;expect(saved().kind);expect(Object.hasOwn(view,'chosen'));end();
}
{
 begin('own_proto_and_falsy_direct');const f=new F({base:B,derived:D,unknown:U});;
 f.setNodeClass('__proto__',B);;const retained=Object.getPrototypeOf(f.node);;Object.defineProperty(f.node,'__proto__',{value:f.node.derived,writable:true,configurable:true});;expect(f.create('__proto__').kind);
 f.setNodeClass('__proto__',D);;expect(Object.getPrototypeOf(f.node)===retained);f.deleteNodeClass('__proto__');;expect(f.create('__proto__').kind);
 f.node.toString=undefined;;expect(f.create('toString').kind);
 const direct=new NF({base:B,unknown:U});direct.node.base=undefined;;throws(()=>direct.create('base'));direct.setNodeClass('__proto__',B);direct.deleteNodeClass('__proto__');;expect(direct.create('__proto__').kind);
 for(const key of ['call','apply','bind','arguments','caller','prototype'])throws(()=>f.create(key));
 for(const key of ['length','name'])expect(f.create(key).kind);
 f.node.call=undefined;;expect(f.create('call').kind);
 f.node.bad={};;throws(()=>f.create('bad'));end();
}
{
 begin('predicate_is_not_construction');class Throwing extends B{constructor(...args){super(...args);throw new TypeError('body');}}const f=new F({chosen:Throwing,unknown:U});const r=Object.create(Throwing.prototype);;expect(f.nodeIsKind(r,'chosen'));throws(()=>f.create('chosen'));
 const duplicate={a:B,'10':B,'2':B};duplicate.a=D;;const other=new F(duplicate);kinds(other);expect(other.create('a').kind);end();
}
assert.ok(process.argv.includes('--check'));assert.deepEqual({cases},JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p58-factory-source.json'),'utf8')));console.log(JSON.stringify({cases:cases.length,assertions:cases.reduce((n,c)=>n+c.observations.length,0),mode:'check'}));
