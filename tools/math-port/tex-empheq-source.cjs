'use strict';
// Pinned Empheq algorithms and real TeX/MML. This is development-only evidence.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [file,sha]of [
 ['.reference/MathJax-src/ts/input/tex/empheq/EmpheqUtil.ts','aa558e95f36ae14e83aa7a161818844a180868d85746cd4efb8dacce418c09b9'],
 ['.reference/MathJax-src/ts/input/tex/empheq/EmpheqConfiguration.ts','e733850c4d5817f005755658aef907a04562e78e1b121a322637fa7fae8452bc'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/empheq/EmpheqUtil.js','35140a72344dfc9e4de07138259fc0958700348af97ec1f0180be696dd88e714'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/empheq/EmpheqConfiguration.js','4f4401e56b9731108b19f4baed3b36c2e6c163b80dfadd323b565eb23a274134']
])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=f=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',f));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const {EmpheqUtil:util}=load('input/tex/empheq/EmpheqUtil.js'),{EmpheqMethods,EmpheqConfiguration}=load('input/tex/empheq/EmpheqConfiguration.js');
const TexParser=load('input/tex/TexParser.js').default,{NodeFactory}=load('input/tex/NodeFactory.js');
const {ParseUtil}=load('input/tex/ParseUtil.js');
const ParseMethods=load('input/tex/ParseMethods.js').default;
let cases=0;function fixture(options={}){let error='';const jax=new TeX({packages:['base','ams','empheq'],tags:'ams',maxMacros:1000,...options,formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());return {jax,compile(source){++cases;error='';const tree=jax.compile(new AbstractMathItem(source,jax,true),reference.document);return {tree,error}}};}
function nodes(root,kind){const result=[];root.walkTree(n=>{if(n.kind===kind)result.push(n)});return result;}
function text(root){let result='';root.walkTree(n=>{if(n.isToken)result+=n.getText()});return result;}
function ok(f,source){const r=f.compile(source);assert.equal(r.error,'',source);assert.equal(f.jax.parseOptions.parsers.length,0);return r.tree;}
function recoverNumber(f){const tree=ok(f,'\\begin{equation}z\\label{after}\\end{equation}');assert.equal(nodes(tree,'mlabeledtr').length,1);assert.equal(f.jax.parseOptions.tags.allLabels.after.tag,'1');}
function snapshot(root){let s='';const add=v=>{v=String(v);s+=v.length+':'+v};function node(n){if(!n){add('null');return}add(n.kind);const attrs=Object.entries(n.attributes.getAllAttributes()).sort(([a],[b])=>a<b?-1:a>b?1:0);add(attrs.length);for(const [key,v]of attrs){add(key);add(v===null||v===undefined?'null':typeof v);add(v===null||v===undefined?'':String(v))}add(n.isToken?n.getText():'');const children=n.isToken?[]:n.childNodes;add(children.length);for(const c of children)node(c)}node(root);return s;}
assert.deepEqual(Object.keys(EmpheqMethods),['Empheq','EmpheqMO','EmpheqDelim']);assert.equal(EmpheqConfiguration.priority,5);assert.deepEqual(EmpheqConfiguration.handler.macro,['empheq-macros']);assert.deepEqual(EmpheqConfiguration.handler.environment,['empheq-env']);
for(const env of ['equation','align','gather','flalign','alignat','multline'])for(const suffix of ['', '*'])assert.equal(util.checkEnv(env+suffix),true);
for(const env of ['aligned','split','array','cases','align**',' align','toString'])assert.equal(util.checkEnv(env),false);
const captures=[],symbols=[],errors=[],typeErrors=[];
for(const env of ['equation','align','gather','flalign','alignat','multline'])for(const star of ['', '*'])for(const sides of ['', 'left=L','right=R','left=L,right=R']){
 const name=env+star,argument=env==='alignat'?name+'=1':name,body=env==='equation'?'a=b':env==='gather'||env==='multline'?'a=b\\\\c=d':'a&=b\\\\c&=d';
 const source='\\begin{empheq}'+(sides?'['+sides+']':'')+'{'+argument+'}'+body+'\\end{empheq}';
 const f=fixture();try{const tree=ok(f,source);captures.push({source,tagSide:'right',snapshot:snapshot(tree)})}
 catch(e){assert.equal(source,'\\begin{empheq}[right=R]{equation*}a=b\\end{empheq}');assert.equal(e instanceof TypeError,true);assert.match(e.message,/reading 'split'/);typeErrors.push(source);ok(f,'x');recoverNumber(f);}
}
{
 const f=fixture(),map=f.jax.parseOptions.handlers.retrieve('empheq-macros');
 const environment=f.jax.parseOptions.handlers.retrieve('empheq-env');assert.equal(environment.parser===ParseMethods.environment,true);assert.equal(environment.lookup('empheq').func===EmpheqMethods.Empheq,true);assert.deepEqual(environment.lookup('empheq').args,['empheq']);
 for(const command of ['empheql','empheqr','empheqbigl','empheqbigr']){assert.equal(map.lookup(command).func===EmpheqMethods.EmpheqDelim,true);assert.equal(map.lookup(command).args.length,0)}
 for(const [name,macro]of map.map){if(macro.func===EmpheqMethods.EmpheqMO){const tree=ok(f,'\\'+name);const mo=nodes(tree,'mo');assert.equal(mo.length,1);assert.equal(mo[0].getText(),macro.args[0]);symbols.push({name,glyph:macro.args[0],snapshot:snapshot(tree)})}}
 assert.equal(symbols.length,32);
 for(const command of ['empheql','empheqr','empheqbigl','empheqbigr'])for(const delimiter of ['(','.']){const source='\\'+command+delimiter,tree=ok(f,source);assert.equal(nodes(tree,'mo')[0].attributes.get('stretchy'),true);assert.equal(nodes(tree,'mo')[0].attributes.get('symmetric'),true);captures.push({source,tagSide:'right',snapshot:snapshot(tree)})}
}
for(const source of ['\\begin{empheq}[left]{align}a&=b\\end{empheq}','\\begin{empheq}[right]{align}a&=b\\end{empheq}']){
 const f=fixture();assert.throws(()=>f.compile(source),e=>e instanceof TypeError&&e.message==='tex.trim is not a function');typeErrors.push(source);ok(f,'x');recoverNumber(f);
}
const utilityCaptures=[];
function table(f,shape='ragged'){
 const factory=f.jax.mmlFactory,token=t=>factory.create('mi',{},[factory.create('text').setText(t)]),cell=t=>factory.create('mtd',{},[token(t)]);
 const rows=shape==='empty'?[]:shape==='labeled'?[factory.create('mlabeledtr',{},['label','a'].map(cell)),factory.create('mtr',{},['b','c','d'].map(cell))]:[factory.create('mtr',{},['a'].map(cell)),factory.create('mtr',{},['b','c','d'].map(cell))];
 return factory.create('mtable',{columnalign:'center right left',columnspacing:'1em 2em'},rows);
}
for(const [name,shape]of [['cellBlock','ragged'],['topRowTable','labeled'],['rowspanCell','labeled'],['left','empty'],['left','labeled'],['right','empty'],['right','ragged'],['right','labeled']]){
 const f=fixture(),p=new TexParser('x',{},f.jax.parseOptions),original=table(f,shape);let result;
 const before=snapshot(original);
 if(name==='cellBlock'){result=util.cellBlock('a+b',original,p,'');assert.equal(result.childNodes[0].childNodes.length,4);assert.equal(nodes(result,'mtable')[0]===original,true)}
 if(name==='topRowTable'){
  result=util.topRowTable(original,p);const copy=nodes(result,'mtable')[0];assert.equal(copy===original,false);assert.equal(copy.childNodes.length,1);assert.equal(original.childNodes.length,2);assert.equal(copy.childNodes[0]===original.childNodes[0],false);assert.equal(copy.attributes.get('align'),'baseline 1');
  assert.equal(snapshot(original),before);copy.childNodes[0].attributes.set('data-copy','yes');assert.equal(original.childNodes[0].attributes.get('data-copy'),undefined);copy.childNodes[0].attributes.unset('data-copy');
 }
 if(name==='rowspanCell'){result=f.jax.mmlFactory.create('mtd');util.rowspanCell(result,'L',original,p,'');assert.equal(snapshot(original),before)}
 if(name==='left'||name==='right'){result=ParseUtil.copyNode(original,p);util[name](result,original,'L',p,'');assert.equal(snapshot(original),before)}
 utilityCaptures.push({name,shape,snapshot:snapshot(result)});p.mml();assert.equal(f.jax.parseOptions.parsers.length,0);
}
{
 const f=fixture(),p=new TexParser('x',{},f.jax.parseOptions);assert.equal(util.columnCount(table(f,'empty')),0);assert.equal(util.columnCount(table(f,'ragged')),3);assert.equal(util.columnCount(table(f,'labeled')),3);
 const labeled=table(f,'labeled');labeled.setChildren(labeled.childNodes.slice(0,1));assert.equal(util.columnCount(labeled),1);
 const original=table(f),requests=[];f.jax.parseOptions.nodeFactory.set('node',(factory,kind,children,def,txt)=>{requests.push(kind);return NodeFactory.createNode(factory,kind,children,def,txt)});
 util.topRowTable(original,p);assert.equal(requests.filter(k=>k==='mphantom').length,0);assert.equal(requests.filter(k=>k==='mpadded').length,1);
 requests.length=0;util.cellBlock('a+b',original,p,'');assert.equal(requests.filter(k=>k==='mphantom').length,1);p.mml();
}
{
 const f=fixture(),p=new TexParser('x',{},f.jax.parseOptions);f.jax.parseOptions.tags.label='side';
 util.cellBlock('L',table(f),p,'align');assert.equal(f.jax.parseOptions.tags.currentTag.env,'align');assert.equal(f.jax.parseOptions.tags.labels.side.tag,'1');assert.equal(f.jax.parseOptions.tags.labels.side.id,'mjx-eqn:side');p.mml();
}
const extraSources=[
 '\\begin{empheq}[left=false,right=false]{align}a&=b\\end{empheq}',
 '\\begin{empheq}[left=,right=]{gather*}a\\end{empheq}',
 '\\begin{empheq}[left={L},right={R}]{alignat=1=discarded}a&=b\\end{empheq}',
 '\\begin{empheq}[left={\\empheqlbrace},right={\\empheqrbrace}]{align}a&=b\\end{empheq}',
 '\\begin{empheq}[left=L,right=R]{align}a&=b\\tag{A}\\label{first}\\\\c&=d\\notag\\end{empheq}',
 '\\begin{empheq}[left=L]{align*}\\end{empheq}',
];
for(const source of extraSources)for(const tagSide of ['left','right']){const f=fixture({tagSide});captures.push({source,tagSide,snapshot:snapshot(ok(f,source))})}
{
 const source='\\boldsymbol{\\empheqlbrace \\empheql(}',f=fixture({packages:['base','ams','empheq','boldsymbol']}),tree=ok(f,source),mo=nodes(tree,'mo');
 assert.deepEqual(mo.map(n=>n.getText()),['{','(']);assert.deepEqual(mo.map(n=>n.attributes.get('mathvariant')),['bold','bold']);
 assert.deepEqual(mo.map(n=>n.attributes.get('data-latex')),['\\empheqlbrace',source]);
 captures.push({source,tagSide:'right',snapshot:snapshot(tree),bold:true});
}
for(const source of ['\\begin{empheq}{array}a\\end{empheq}','\\begin{empheq}[bad=x]{align}a&=b\\end{empheq}','\\begin{empheq}',
 '\\begin{empheq}{alignat}a&=b\\end{empheq}','\\begin{empheq}{align}a&=b',
 '\\begin{empheq}{align}\\begin{empheq}{align}a&=b\\end{empheq}\\end{empheq}',
 '\\begin{empheq}[left={\\unknown}]{align}a&=b\\end{empheq}','\\empheql','\\empheqr{bad}']){
 const f=fixture(),r=f.compile(source);assert.notEqual(r.error,'',source);errors.push({source,id:r.error});ok(f,'x');
}
{
 const f=fixture({tagSide:'left'});ok(f,'\\begin{empheq}[left=L,right=R]{align}a&=b\\tag{A}\\label{first}\\end{empheq}');assert.equal(f.jax.parseOptions.tags.allLabels.first.tag,'A');assert.equal(text(ok(f,'\\ref{first}+\\eqref{first}')),'A+(A)');
}
assert.deepEqual(util.splitOptions('left={a=b},right={c,d}',{left:1,right:1}),{left:'a=b',right:'c,d'});
assert.throws(()=>util.splitOptions('bad=x',{left:1}),e=>e.id==='InvalidOption');
assert.equal(typeErrors.length,3);assert.equal(utilityCaptures.length,8);assert.equal(captures.length,68);
const {utf16Field, byteField, writeTables} = require('./fixture-data.cjs');
writeTables(path.join(root, 'tests/math/fixtures/packages/empheq'), {
  'cases': {columns: ["source_utf16", "tagSide_utf16", "snapshot_utf16", "bold_boolean"],
    rows: captures.map(c => [utf16Field(c.source), utf16Field(c.tagSide), utf16Field(c.snapshot), +!!c.bold])},
  'symbols': {columns: ["name_utf16", "glyph_utf16", "snapshot_utf16"],
    rows: symbols.map(c => [utf16Field(c.name), utf16Field(c.glyph), utf16Field(c.snapshot)])},
  'utilities': {columns: ["name_utf16", "shape_utf16", "snapshot_utf16"],
    rows: utilityCaptures.map(c => [utf16Field(c.name), utf16Field(c.shape), utf16Field(c.snapshot)])},
  'errors': {columns: ["source_utf16", "id_bytes"],
    rows: errors.map(c => [utf16Field(c.source), byteField(c.id)])},
  'type-errors': {columns: ["value_utf16"],
    rows: typeErrors.map(c => [utf16Field(c)])},
});
console.log(JSON.stringify({cases,environments:6,matrix:48,treeCases:captures.length,symbols:symbols.length,utilityCases:utilityCaptures.length,texErrors:errors.length,typeErrors:typeErrors.length,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
