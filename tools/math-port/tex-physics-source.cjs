'use strict';
// Pinned whole Physics package, genuine TeX and primitive physical-tree data.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),lock=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/locks/physics-source-lock.json'),'utf8'));
for(const f of lock.files)for(const [file,sha]of [[path.join(root,'.reference/MathJax-src',f.ts),f.tsSha256],[path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',f.cjs),f.cjsSha256]])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=f=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',f));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const {MapHandler}=load('input/tex/MapHandler.js'),Methods=load('input/tex/physics/PhysicsMethods.js').default;
const {AutoOpen}=load('input/tex/physics/PhysicsItems.js'),{PhysicsConfiguration}=load('input/tex/physics/PhysicsConfiguration.js');
const TexParser=load('input/tex/TexParser.js').default,{NodeFactory}=load('input/tex/NodeFactory.js');
let compiles=0;
function fixture(italic=false,arrow=false,extra=false){let error='';const jax=new TeX({packages:['base',...(extra?['newcommand']:[]),'physics'],physics:{italicdiff:italic,arrowdel:arrow},maxMacros:1000,formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());return {jax,compile(source){++compiles;error='';try{const tree=jax.compile(new AbstractMathItem(source,jax,true),reference.document);return {tree,error,runtime:'',message:''}}catch(e){return {tree:null,error,runtime:e.name,message:String(e.message)}}}};}
function snapshot(root){let s='';const add=v=>{v=String(v);s+=v.length+':'+v};function visit(n){if(!n){add('null');return}add(n.kind);const attrs=Object.entries(n.attributes.getAllAttributes()).sort(([a],[b])=>a<b?-1:a>b?1:0);add(attrs.length);for(const [key,v]of attrs){add(key);add(v===null||v===undefined?'null':typeof v);add(v===null||v===undefined?'':String(v))}for(const key of ['open','close','texClass']){const v=n.getProperty(key);add(v===undefined?'':typeof v);add(v===undefined?'':String(v))}add(n.isToken?n.getText():'');const children=n.isToken?[]:n.childNodes;add(children.length);children.forEach(visit)}visit(root);return s;}
function ok(f,source){const r=f.compile(source);assert.equal(r.error,'',source);assert.equal(r.runtime,'',source+': '+r.message);assert.equal(f.jax.parseOptions.parsers.length,0);return r.tree;}
// Capture immutable registration inputs before Source mutates shared token attributes.
const mapNames=['Physics-automatic-bracing-macros','Physics-vector-mo','Physics-vector-mi','Physics-vector-macros','Physics-expressions-macros','Physics-quick-quad-macros','Physics-derivative-macros','Physics-bra-ket-macros','Physics-matrix-macros','Physics-aux-envs','Physics-characters'];
assert.equal(Object.keys(Methods).length,28);assert.equal(PhysicsConfiguration.priority,5);assert.deepEqual(PhysicsConfiguration.options.physics,{italicdiff:false,arrowdel:false});
const bindings=[];for(const name of mapNames){const map=MapHandler.getMap(name);for(const [token,e]of map.map)bindings.push(e.func?{map:name,token,method:Object.keys(Methods).find(k=>Methods[k]===e.func),args:e.args||[],char:null,attributes:{}}:{map:name,token,method:name.endsWith('-mo')?'mathchar0mo':'mathchar0mi',args:[],char:e.char,attributes:{...(e.attributes||{})}})}assert.equal(bindings.length,196);
for(const token of ['dotproduct','vdot','gradientnabla'])assert.deepEqual(bindings.find(b=>b.token===token).attributes,{mathvariant:'bold'});
const trees=[],errors=[],runtime=[],steps=[],direct=[];
function capture(source,italic=false,arrow=false,name=''){const f=fixture(italic,arrow),r=f.compile(source);if(r.runtime){runtime.push({source,italic,arrow,kind:r.runtime,message:r.message});ok(f,'x')}else if(r.error){errors.push({source,italic,arrow,id:r.error});ok(f,'x')}else{assert.equal(f.jax.parseOptions.parsers.length,0);trees.push({name,source,italic,arrow,snapshot:snapshot(r.tree)})}}
const old=fs.readFileSync(path.join(root,'tools/math-port/physics-original-oracle.cjs'),'utf8');
const historical=[...old.matchAll(/\['(physics-[^']+)', String\.raw`([^`]+)`, (.+)\],/g)];assert.equal(historical.length,63);
for(const m of historical)capture(m[2],m[3].includes('italicdiff: true'),m[3].includes('arrowdel: true'),m[1]);
for(const source of [
 '\\qty', '\\qty x', '\\qty|x|', '\\qty[x]', '\\qty{a\\over b}', '\\qty(a\\over b)', '\\qty(a+(b))', '\\qty((a))', '\\qty(a\\over\\qty[b])',
 '\\qty\\Bigg[a]', '\\qty\\Bigl(x)', '\\pqty*{x}', '\\pqty\\big{a}', '\\abs{x}', '\\norm{x}', '\\vqty{x}', '\\bqty{x}',
 '\\eval', '\\eval{x}_0^1', '\\eval[x|_0', '\\eval*(\\frac{a}{b}|_0', '\\eval(\\qty[x]|_0',
 '\\comm*{A}{B}', '\\comm\\big{A}{B}', '\\acomm{A}{B}', '\\pb{A}{B}',
 '\\vb{a\\vb*{b}c}d', '\\vb*{a\\vb{b}c}d', '\\vb{\\mathrm{x}y}z', '\\mathbf{\\vb{x}y}z',
 '\\vb*{\\alpha\\Gamma1+\\hat{x}}', '\\vb{a\\over b}c', '\\vb{a}b\\vb*{c}d', '\\va*{x}\\vu{y}',
 '\\grad{f}', '\\grad[f]', '\\grad(f)', '\\div{v}', '\\div[v]', '\\curl x', '\\Res{z}', '\\Re{x}', '\\Im(x)', '\\PV{x}', '\\pv[x]',
 '\\sin[2](x)', '\\sin[](x)', '\\sin x', '\\trace(A)', '\\Trace(A)', '\\Pr(A)', '\\qq{if $x$}', '\\qq{}', '\\qcomma x',
 '\\dd[]', '\\dd[2]x', '\\dd[2](x)', '\\var{x}', '\\flatfrac{a}{b}',
 '\\dv[2]{f}{x}', '\\dv[]{x}', '\\pdv[3]{f}{x}(z)', '\\pdv[9]{f}{x}{y}(ignored)', '\\pdv{f}{x}{}(z)',
 '\\fdv*{f}{x}', '\\dv{}{x}', '\\dv{x}{}',
 '\\bra*{a}\\ket{b}', '\\bra{a}\\ket*{b}', '\\bra{a}\\ket', '\\bra{a}x', '\\ket*{a}', '\\ip*{a}', '\\ip{a}{}',
 '\\dyad*{a}{b}', '\\dyad{a}{}', '\\ev*{A}{s}', '\\ev**{A}{s}', '\\ev{}{s}', '\\ev{A}{}', '\\mel*{a}{A}{b}', '\\mel**{a}{A}{b}',
 '\\mqty', '\\mqty[a&b\\\\c&d]', '\\mqty|a&b\\\\c&d|', '\\mqty{\\imat{3}}', '\\mqty{\\imat{03}}',
 '\\mqty{\\imat{-1}}', '\\mqty{\\imat{2suffix}}', '\\mqty{\\xmat{a}{2}{3}}', '\\mqty{\\xmat*{a}{1}{3}}', '\\mqty{\\xmat*{a}{3}{1}}', '\\mqty{\\xmat*{a}{1}{1}}',
 '\\mqty{\\xmat{a}{0}{-2}}', '\\mqty{\\zmat{2}{2}}', '\\mqty{\\dmat{{a,b},c}}', '\\mqty{\\admat{a,b,c}}', '\\dmat x', '\\mqty{\\dmat{}}',
 ...['0','1','2','3','x','y','z','q','', 'xprefix'].map(s=>'\\mqty{\\pmat{'+s+'}}'),
 ...['pmqty','Pmqty','bmqty','vmqty','spmqty','sPmqty','sbmqty','svmqty','mdet','smdet'].map(s=>'\\'+s+'{a&b\\\\c&d}'),
 ...['\\qty(x','\\qty[x)','\\pqty x','\\pqty*','\\comm','\\comm{a}','\\comm\\bad{a}{b}', '\\vb', '\\vb{\\unknown}', '\\va', '\\dd[', '\\dv', '\\pdv{', '\\bra', '\\ket', '\\ip', '\\mel{a}{b}', '\\mqty(a&b', '\\imat{bad}', '\\xmat{a}{02}{3}', '\\xmat{a}{2}{3x}', '\\xmat{a}{2.0}{3}', '\\pmat', '\\dmat{a,b'],
])capture(source);
for(const italic of [false,true])for(const arrow of [false,true])for(const source of ['\\diffd x+\\vnabla','\\dd{x}+\\dv{f}{x}+\\grad(f)+\\div(v)','\\vb{\\diffd x+\\vnabla}'])capture(source,italic,arrow);
for(const [group,sources]of [
 [0,['\\let\\savedket\\ket','\\bra{a}\\savedket{b}','\\let\\ket\\relax','\\bra*{a}\\savedket*{b}']],
 [1,['\\let\\savedbra\\bra','\\renewcommand{\\bra}[1]{q}','\\savedbra{a}\\ket{b}']],
 [2,['\\vb{\\unknown}','x','\\vb{a}b']],
 [3,['\\newcommand{\\saved}[1]{\\qty(#1)}','\\saved{x}','\\qty(a\\over b)']]
]){const f=fixture(false,false,true);for(const source of sources){const r=f.compile(source);steps.push({group,source,snapshot:r.tree?snapshot(r.tree):'',error:r.error,runtime:r.runtime});if(!r.error&&!r.runtime)assert.equal(f.jax.parseOptions.parsers.length,0)}}
capture('\\imat{'+ '9'.repeat(400)+'}');
capture('|)]');
// Direct AutoOpen actions preserve Source's precise flag, replay and fence state behavior.
function frame(name,action){const f=fixture(),p=new TexParser('',{},f.jax.parseOptions),factory=f.jax.parseOptions.itemFactory,item=factory.create('auto open');item.setProperties({open:'(',close:')',big:'',right:'',smash:false,ignore:false});try{const node=action(item,p,factory);if(node)direct.push({name,snapshot:snapshot(node)})}finally{p.mml()}}
const token=(p,t)=>p.create('token','mi',{},t);
frame('default', (a,p)=>{a.Push(token(p,'x'));const n=a.toMml();for(const k of ['open','close','texClass'])assert.equal(n.getProperty(k),undefined);return n});
frame('force-ignored',(a,p)=>{a.Push(token(p,'x'));return a.toMml(true,true)});
frame('false-force',(a,p)=>{const n=token(p,'x');n.setProperty('open','o');n.setProperty('close','c');n.setProperty('texClass',2);a.Push(n);const out=a.toMml(false,true);assert.equal(out.childNodes[0]===n,true);assert.equal(n.getProperty('open'),'o');return out});
frame('smash-right',(a,p)=>{a.setProperties({smash:true,right:'y'});a.Push(token(p,'x'));return a.toMml()});
frame('pre-close',(a,p,f)=>{a.Push(token(p,'x'));const out=a.checkItem(f.create('close').setProperty('pre-autoclose',true));assert.deepEqual(out,[null,false]);assert.equal(a.Size(),1);return a.toMml(false)});
frame('autoclose',(a,p,f)=>{a.Push(token(p,'x'));const result=a.checkItem(f.create('mml',token(p,')')).setProperty('autoclose',true));assert.equal(result[1],true);assert.equal(result[0].length,1);return result[0][0]});
frame('ignore',(a,p,f)=>{a.setProperty('ignore',true);a.Push(token(p,'x'));const result=a.checkItem(f.create('mml',token(p,')')).setProperty('autoclose',true));assert.deepEqual(result,[[],true]);assert.equal(a.Size(),0);return a.toMml(false)});
frame('nested-count',(a,p,f)=>{assert.equal(a.openCount,0);a.checkItem(f.create('mml',p.create('token','mo',{},'(')));assert.equal(a.openCount,1);assert.equal(a.closing(']'),false);assert.equal(a.openCount,1);assert.equal(a.closing(')'),false);assert.equal(a.openCount,0);assert.equal(a.closing(')'),true);assert.equal(a.openCount,-1);assert.equal(a.closing(')'),false);assert.equal(a.openCount,-2);return a.toMml(false)});
frame('stop',(a,p,f)=>{assert.throws(()=>a.checkItem(f.create('stop')),e=>e.id==='ExtraOrMissingDelims');return a.toMml(false)});
// Genuine configured creators: fenced definition before stripping; Source's
// VectorBold lacks exception cleanup, which the native RAII contract strengthens.
let sourceCreatorLeak=false;
{
 const f=fixture(),factory=f.jax.parseOptions.nodeFactory,requests=[];
 factory.setCreators({node(nf,kind,children=[],def={},text){const out=NodeFactory.createNode(nf,kind,children,def,text);requests.push({kind,def:{...def},node:out});return out}});
 ok(f,'\\qty(x)');const row=requests.find(r=>r.kind==='mrow'&&r.def.open==='('&&r.def.close===')');assert.ok(row);assert.equal(row.def.texClass,7);for(const key of ['open','close','texClass'])assert.equal(row.node.getProperty(key),undefined);
 const token=factory.get('token');factory.setCreators({node(nf,kind,children=[],def={},text){if(kind==='mi'&&text?.getText()==='q')throw Error('physics creator probe');return NodeFactory.createNode(nf,kind,children,def,text)}});
 const result=f.compile('\\vb{q}');assert.equal(result.runtime,'Error');assert.equal(result.message,'physics creator probe');sourceCreatorLeak=factory.get('token')!==token;assert.equal(sourceCreatorLeak,true);
 factory.setCreators({token,node:NodeFactory.createNode});ok(f,'x');
}
for(const binding of bindings)if(binding.char!==null)capture('\\'+binding.token);
const {utf16Field, byteField, primitiveFields, writeTables} = require('./fixture-data.cjs');
writeTables(path.join(root, 'tests/math/fixtures/packages/physics'), {
  "trees": {columns: ["name_utf16", "source_utf16", "italic_boolean", "arrow_boolean", "snapshot_utf16"],
    rows: trees.map(r => [utf16Field(r.name), utf16Field(r.source), +!!r.italic, +!!r.arrow, utf16Field(r.snapshot)])},
  "errors": {columns: ["source_utf16", "italic_boolean", "arrow_boolean", "id_bytes"],
    rows: errors.map(r => [utf16Field(r.source), +!!r.italic, +!!r.arrow, byteField(r.id)])},
  "runtime": {columns: ["source_utf16", "kind_bytes", "message_utf16"],
    rows: runtime.map(r => [utf16Field(r.source), byteField(r.kind), utf16Field(r.message)])},
  "steps": {columns: ["group_integer", "source_utf16", "snapshot_utf16", "error_bytes", "runtime_bytes"],
    rows: steps.map(r => [r.group, utf16Field(r.source), utf16Field(r.snapshot), byteField(r.error), byteField(r.runtime)])},
  "direct": {columns: ["name_utf16", "snapshot_utf16"],
    rows: direct.map(r => [utf16Field(r.name), utf16Field(r.snapshot)])},
  bindings: {columns: ['map_utf16', 'token_utf16', 'method_bytes', 'argument_count',
      'has_character', 'character_utf16', 'attribute_count'],
    rows: bindings.map(r => [utf16Field(r.map), utf16Field(r.token), byteField(r.method),
      r.args.length, +(r.char !== null), utf16Field(r.char ?? ''), Object.keys(r.attributes).length])},
  attributes: {columns: ['binding_index', 'attribute_index', 'key_utf16', 'type', 'value'],
    rows: bindings.flatMap((r, index) => Object.entries(r.attributes).map(([key, value], attribute) =>
      [index, attribute, utf16Field(key), ...primitiveFields(value)]))},
  arguments: {columns: ['binding_index', 'argument_index', 'type', 'value'],
    rows: bindings.flatMap((r, index) => r.args.map((value, argument) =>
      [index, argument, ...primitiveFields(value)]))}
});
console.log(JSON.stringify({compiles,historical:historical.length,trees:trees.length,errors:errors.length,runtime,steps:steps.length,direct:direct.length,maps:mapNames.length,bindings:bindings.length,sourceCreatorLeak,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
