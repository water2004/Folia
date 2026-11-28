'use strict';
// Whole Bussproofs Source witness: real pinned NewCM output measurements and TeX.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),lock=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/locks/bussproofs-source-lock.json'),'utf8'));
for(const f of lock.files)for(const [file,sha]of [[path.join(root,'.reference/MathJax-src',f.ts),f.tsSha256],[path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',f.cjs),f.cjsSha256]])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=f=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',f));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const Methods=load('input/tex/bussproofs/BussproofsMethods.js').default;
const {BussproofsConfiguration}=load('input/tex/bussproofs/BussproofsConfiguration.js');
const Util=load('input/tex/bussproofs/BussproofsUtil.js');
let compiles=0,measurements=0;
const originalBBox=reference.output.getBBox;reference.output.getBBox=function(...args){++measurements;return originalBBox.apply(this,args)};
reference.output.pxPerEm=8/reference.output.font.params.x_height;
function fixture(packages=['base','newcommand','bussproofs']){let error='';const jax=new TeX({packages,maxMacros:1000,formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());return {jax,compile(source,document=reference.document){++compiles;error='';try{const tree=jax.compile(new AbstractMathItem(source,jax,true),document);return {tree,error,runtime:'',message:''}}catch(e){return {tree:null,error,runtime:e.name,message:String(e.message)}}}};}
function snapshot(root){let s='';const add=v=>{v=String(v);s+=v.length+':'+v};function visit(n){if(!n){add('null');return}add(n.kind);const attrs=Object.entries(n.attributes.getAllAttributes()).sort(([a],[b])=>a<b?-1:a>b?1:0);add(attrs.length);for(const [key,v]of attrs){add(key);add(v===null||v===undefined?'null':typeof v);add(v===null||v===undefined?'':String(v))}add(n.isToken?n.getText():'');const children=n.isToken?[]:n.childNodes;add(children.length);children.forEach(visit)}visit(root);return s;}
function ok(f,source){const r=f.compile(source);assert.equal(r.error,'',source);assert.equal(r.runtime,'',source+': '+r.message);assert.equal(f.jax.parseOptions.parsers.length,0);return r.tree;}
const trees=[],errors=[],runtime=[],steps=[];
function capture(source,name=''){const f=fixture(),r=f.compile(source);if(r.runtime){runtime.push({name,source,kind:r.runtime,message:r.message});ok(f,'x')}else if(r.error){errors.push({name,source,id:r.error});ok(f,'x')}else{assert.equal(f.jax.parseOptions.parsers.length,0);trees.push({name,source,snapshot:snapshot(r.tree)})}}
// Keep every original oracle formula; do not run its obsolete reflection capture.
const old=fs.readFileSync(path.join(root,'tools/math-port/bussproofs-original-oracle.cjs'),'utf8');
const historical=[...old.matchAll(/\['(proof-[^']+)', String\.raw`([^`]+)`\]/g)].map(m=>[m[1],m[2]]);assert.equal(historical.length,31);
for(const [name,source]of historical)capture(source,name);
// Mathematical cases retained from the retired expanded C++ observer.
const expanded=[
  ['empty-axiom-content', String.raw`\begin{prooftree}\AxiomC{}\end{prooftree}`],
  ['space-only-axiom-content', String.raw`\begin{prooftree}\AxiomC{   }\end{prooftree}`],
  ['empty-unary-conclusion', String.raw`\begin{prooftree}\AxiomC{A}\UnaryInfC{}\end{prooftree}`],
  ['space-only-unary-conclusion', String.raw`\begin{prooftree}\AxiomC{A}\UnaryInfC{   }\end{prooftree}`],
  ['empty-labels', String.raw`\begin{prooftree}\AxiomC{A}\LeftLabel{}\RightLabel{}\UnaryInfC{B}\end{prooftree}`],
  ['empty-label-left-only', String.raw`\begin{prooftree}\AxiomC{A}\LeftLabel{}\UnaryInfC{B}\end{prooftree}`],
  ['solid-line-command', String.raw`\begin{prooftree}\AxiomC{A}\solidLine\UnaryInfC{B}\end{prooftree}`],
  ['always-no-line', String.raw`\begin{prooftree}\alwaysNoLine\AxiomC{A}\UnaryInfC{B}\UnaryInfC{C}\end{prooftree}`],
  ['always-single-line', String.raw`\begin{prooftree}\alwaysSingleLine\AxiomC{A}\UnaryInfC{B}\UnaryInfC{C}\end{prooftree}`],
  ['always-solid-line', String.raw`\begin{prooftree}\alwaysSolidLine\AxiomC{A}\UnaryInfC{B}\UnaryInfC{C}\end{prooftree}`],
  ['always-root-bottom', String.raw`\begin{prooftree}\alwaysRootAtBottom\AxiomC{A}\UnaryInfC{B}\UnaryInfC{C}\end{prooftree}`],
  ['quaternary-sequent', String.raw`\begin{prooftree}\Axiom$A\fCenter B$\Axiom$C\fCenter D$\Axiom$E\fCenter F$\Axiom$G\fCenter H$\QuaternaryInf$I\fCenter J$\end{prooftree}`],
  ['quinary-sequent', String.raw`\begin{prooftree}\Axiom$A\fCenter B$\Axiom$C\fCenter D$\Axiom$E\fCenter F$\Axiom$G\fCenter H$\Axiom$I\fCenter J$\QuinaryInf$K\fCenter L$\end{prooftree}`],
  ['unary-insufficient-premises', String.raw`\begin{prooftree}\UnaryInfC{B}\end{prooftree}`],
  ['ternary-insufficient-premises', String.raw`\begin{prooftree}\AxiomC{A}\TrinaryInfC{B}\end{prooftree}`],
  ['quaternary-insufficient-premises', String.raw`\begin{prooftree}\AxiomC{A}\AxiomC{B}\QuaternaryInfC{C}\end{prooftree}`],
  ['quinary-insufficient-premises', String.raw`\begin{prooftree}\AxiomC{A}\AxiomC{B}\AxiomC{C}\AxiomC{D}\QuinaryInfC{E}\end{prooftree}`],
  ['left-label-outside-proof', String.raw`\LeftLabel{x}`],
  ['inference-outside-proof', String.raw`\UnaryInfC{B}`],
  ['root-command-outside-proof', String.raw`\rootAtBottom`],
  ['multiple-sequent-adjustments', String.raw`\begin{prooftree}\Axiom$A\fCenter B$\UnaryInf$B\fCenter C$\UnaryInf$C\fCenter D$\UnaryInf$D\fCenter E$\end{prooftree}`],
];
assert.equal(expanded.length,21);
for(const [name,source]of expanded)capture(source,name);
const proof=body=>'\\begin{prooftree}'+body+'\\end{prooftree}';
for(const source of [
 proof(''),proof('\\AxiomC{}\\UnaryInfC{B}'),proof('\\AxiomC{   }\\UnaryInfC{}'),
 proof('\\AxiomC{premise $x^2$ and \\(y\\)}\\UnaryInfC{conclusion $z$}'),
 proof('\\AxiomC{A}\\solidLine\\UnaryInfC{B}'),
 proof('\\AxiomC{A}\\noLine\\UnaryInfC{B}\\UnaryInfC{C}'),
 proof('\\alwaysNoLine\\AxiomC{A}\\singleLine\\UnaryInfC{B}\\UnaryInfC{C}'),
 proof('\\alwaysSingleLine\\AxiomC{A}\\dashedLine\\UnaryInfC{B}\\UnaryInfC{C}'),
 proof('\\alwaysSolidLine\\AxiomC{A}\\UnaryInfC{B}'),
 proof('\\AxiomC{A}\\rootAtTop\\UnaryInfC{B}\\alwaysRootAtBottom\\UnaryInfC{C}'),
 proof('\\AxiomC{A}\\LeftLabel{}\\UnaryInfC{B}'),
 proof('\\AxiomC{A}\\LeftLabel{$\\alpha$}\\UnaryInfC{B}\\UnaryInfC{C}'),
 proof('\\AxiomC{A}\\RightLabel{r}\\LeftLabel{l}\\UnaryInfC{B}\\AxiomC{C}\\BinaryInfC{D}'),
 proof('x\\AxiomC{A}\\UnaryInfC{B}'),proof('{x+y}\\AxiomC{A}\\UnaryInfC{B}'),
 proof('\\AxiomC{'+proof('\\AxiomC{A}\\UnaryInfC{B}')+'}\\UnaryInfC{C}'),
 proof('\\AxiomC{$\\frac{a}{b}$}\\UnaryInfC{$\\sqrt{x}$}'),
 proof('\\Axiom$A\\fCenter B$\\UnaryInf$CC\\fCenter D$\\UnaryInf$E\\fCenter FFF$'),
 '\\newcommand{\\fCenter}{\\vdash}'+proof('\\Axiom$AA\\fCenter B$\\UnaryInf$C\\fCenter DDD$'),
 '\\renewcommand{\\fCenter}{\\vdash}'+proof('\\Axiom$A\\fCenter B\\fCenter ignored$\\UnaryInf$C\\fCenter D$'),
 '\\fCenter x',
 ...['\\UnaryInfC{B}','\\LeftLabel{x}','\\RightLabel{x}','\\noLine','\\rootAtTop','\\Axiom$A\\fCenter B$','\\UnaryInf$A\\fCenter B$'],
 proof('\\AxiomC'),proof('\\AxiomC{A}\\UnaryInfC'),proof('\\AxiomC{A}\\LeftLabel'),
 proof('\\Axiom$A\\fCenter B'),proof('\\AxiomC{A}\\UnaryInf A'),proof('\\AxiomC{A}\\UnaryInf$A=B$'),
 proof('\\AxiomC{A}\\BinaryInf$A\\fCenter B$'),proof('\\AxiomC{\\unknown}'),
 '\\begin{prooftree}\\AxiomC{A}\\end{array}',
])capture(source);
for(const [arity,command]of [[1,'UnaryInf'],[2,'BinaryInf'],[3,'TrinaryInf'],[4,'QuaternaryInf'],[5,'QuinaryInf']])capture(proof('\\Axiom$A\\fCenter B$'.repeat(arity)+'\\'+command+'$CC\\fCenter D$'));
// Stateful real command definitions and captured aliases across expressions.
for(const [group,sources]of [
 [0,['\\renewcommand{\\fCenter}{\\vdash}',proof('\\Axiom$A\\fCenter BBB$\\UnaryInf$CCCC\\fCenter D$'),proof('\\AxiomC{A}\\UnaryInfC{B}')]],
 [1,['\\let\\savedAXC\\AXC\\let\\savedUIC\\UIC',proof('\\savedAXC{A}\\savedUIC{B}'),'\\renewcommand{\\AXC}[1]{changed}',proof('\\savedAXC{C}\\savedUIC{D}')]],
 [2,[proof('\\alwaysDashedLine\\AxiomC{A}\\UnaryInfC{B}'),proof('\\AxiomC{A}\\UnaryInfC{B}')]],
 [3,[proof('\\Axiom$A\\fCenter B$'),proof('\\AxiomC{A}\\UnaryInfC{B}')]],
 [4,['\\AxiomC{A}',proof('\\AxiomC{A}\\UnaryInfC{B}')]]
]){const f=fixture();for(const source of sources){const r=f.compile(source);steps.push({group,source,snapshot:r.tree?snapshot(r.tree):'',error:r.error,runtime:r.runtime});if(!r.runtime&&!r.error)assert.equal(f.jax.parseOptions.parsers.length,0)}}
{
 const f=fixture(),source='\\renewcommand{\\fCenter}{\\vdash}',first=f.compile(source);assert.equal(first.error,'');assert.equal(first.runtime,'');steps.push({group:5,source,snapshot:snapshot(first.tree),error:'',runtime:'',reset:false});
 f.jax.reset();const next=proof('\\Axiom$A\\fCenter BBB$\\UnaryInf$CC\\fCenter D$');steps.push({group:5,source:next,snapshot:snapshot(ok(f,next)),error:'',runtime:'',reset:true});
}
assert.deepEqual(Object.keys(Methods),['Prooftree','Axiom','Inference','Label','SetLine','RootAtTop','AxiomF','FCenter','InferenceF']);
assert.deepEqual(BussproofsConfiguration.handler,{character:[],delimiter:[],macro:['Bussproofs-macros'],environment:['Bussproofs-environments']});
assert.deepEqual(BussproofsConfiguration.preprocessors.map(([fn,p])=>[fn===Util.saveDocument,p]),[[true,1]]);
assert.deepEqual(BussproofsConfiguration.postprocessors.map(([fn,p])=>[fn===Util.clearDocument?'clear':fn===Util.makeBsprAttributes?'attrs':fn===Util.balanceRules?'balance':'?',p]),[['clear',3],['attrs',2],['balance',1]]);
const f=fixture(),map=f.jax.parseOptions.handlers.retrieve('Bussproofs-macros'),env=f.jax.parseOptions.handlers.retrieve('Bussproofs-environments');assert.equal([...map.map.keys()].length,33);assert.deepEqual(env.lookup('prooftree').args,[null,false]);assert.equal(env.lookup('prooftree').func===Methods.Prooftree,true);
assert.equal(map.lookup('AXC').func===Methods.Axiom,true);assert.equal(map.lookup('UIC').func===Methods.Inference,true);assert.deepEqual(map.lookup('QuinaryInf').args,[5]);assert.deepEqual(map.lookup('alwaysDashedLine').args,['dashed',true]);
// Missing output is an actual Source Error before parsing; no fake output methods.
const {AbstractOutputJax}=load('core/OutputJax.js');const noBBoxDocument=reference.mathjax.document('',{InputJax:f.jax,OutputJax:new AbstractOutputJax()});const missing=f.compile('x',noBBoxDocument);assert.equal(missing.runtime,'Error');assert.equal(missing.message,'The bussproofs extension requires an output jax with a getBBox() method');ok(f,'x');
// Package can run independently of newcommand.
const alone=fixture(['base','bussproofs']);ok(alone,proof('\\AxiomC{A}\\UnaryInfC{B}'));
// The genuine configured creator sees original child borrows and definitions.
{
 const f=fixture(),{NodeFactory}=load('input/tex/NodeFactory.js'),requests=[];
 f.jax.parseOptions.nodeFactory.setCreators({node(factory,kind,children=[],def={},text){const saved=[...children];const result=NodeFactory.createNode(factory,kind,children,def,text);requests.push({kind,children:saved,def:{...def},result});return result}});
 const tree=ok(f,proof('\\AxiomC{A}\\LeftLabel{l}\\RightLabel{r}\\UnaryInfC{B}'));
 const rule=requests.find(r=>r.kind==='mtable'&&r.def.rowlines==='solid');assert.ok(rule);assert.deepEqual(rule.def,{align:'top 2',rowlines:'solid',framespacing:'0 0'});
 assert.equal(rule.children[0]===rule.result.childNodes[0],true);assert.equal(rule.children[1]===rule.result.childNodes[1],true);
 assert.equal(requests.filter(r=>r.kind==='mspace'&&r.def.width==='.5ex').length,4);
 const labels=requests.filter(r=>r.kind==='mpadded');assert.equal(labels.length,2);assert.equal(labels[0].def.height,'.25em');assert.equal(labels[1].def.height,'-.25em');assert.equal(labels[1].def.lspace,'.5ex');
 assert.equal(tree.findNodes('mtable').some(n=>String(n.attributes.get('semantics')).includes('bspr_inferenceRule:down')),true);
}
const direct=[];
function semanticCase(name,action){const f=fixture(),node=f.jax.parseOptions.nodeFactory.create('node','mrow');action(node);f.jax.parseOptions.root=node;const math=new AbstractMathItem('',f.jax,true);math.root=node;Util.makeBsprAttributes({math,document:reference.document,data:f.jax.parseOptions});const semantics=node.attributes.getExplicit('semantics');direct.push({name,present:semantics!==undefined,semantics:semantics||''});}
semanticCase('plain-inference',n=>{Util.setProperty(n,'inferenceRule','down');Util.setProperty(n,'inference',0);Util.setProperty(n,'proof',true);n.setProperty('proof-left',.25);n.setProperty('proof-right',.5)});
semanticCase('all-finite-reinsert',n=>{for(const [key,value]of [['sequent',true],['axiom',false],['proof',true],['inference',0],['inferenceRule','up'],['labelledRule','both'],['prooflabel','left'],['sequentAdjust_right',0],['sequentAdjust_left',.5],['sequentProcessed',true]])Util.setProperty(n,key,value);Util.setProperty(n,'axiom',true);Util.removeProperty(n,'sequentProcessed');Util.setProperty(n,'sequentProcessed',false)});
semanticCase('delete-reinsert',n=>{Util.setProperty(n,'proof',true);Util.setProperty(n,'inference',1);Util.removeProperty(n,'proof');Util.setProperty(n,'proof',false)});
semanticCase('preserve-existing',n=>n.attributes.set('semantics','existing'));
semanticCase('margins-only',n=>{n.setProperty('proof-left',.25);n.setProperty('proof-right',.5)});
assert.equal(direct[0].semantics,'bspr_inferenceRule:down;bspr_inference:0;bspr_proof:true');
assert.equal(direct[2].semantics,'bspr_inference:1;bspr_proof:false');assert.equal(direct[3].semantics,'existing');assert.equal(direct[4].present,false);
const {utf16Field, byteField, primitiveFields, writeTables} = require('./fixture-data.cjs');
const methodNames = Object.keys(Methods);
const bindings = [...map.map].map(([name, macro]) => ({name,
  method: methodNames.indexOf(methodNames.find(key => Methods[key] === macro.func)), args: macro.args || []}));
writeTables(path.join(root, 'tests/math/fixtures/packages/bussproofs'), {
  "trees": {columns: ["name_utf16", "source_utf16", "snapshot_utf16"],
    rows: trees.map(r => [utf16Field(r.name), utf16Field(r.source), utf16Field(r.snapshot)])},
  "errors": {columns: ["source_utf16", "id_bytes"],
    rows: errors.map(r => [utf16Field(r.source), byteField(r.id)])},
  "runtime": {columns: ["source_utf16", "kind_bytes", "message_utf16"],
    rows: runtime.map(r => [utf16Field(r.source), byteField(r.kind), utf16Field(r.message)])},
  "steps": {columns: ["group_integer", "source_utf16", "snapshot_utf16", "error_bytes", "runtime_bytes", "reset_boolean"],
    rows: steps.map(r => [r.group, utf16Field(r.source), utf16Field(r.snapshot), byteField(r.error), byteField(r.runtime), +!!r.reset])},
  "direct": {columns: ["name_utf16", "present_boolean", "semantics_utf16"],
    rows: direct.map(r => [utf16Field(r.name), +!!r.present, utf16Field(r.semantics)])},
  bindings: {columns: ['name_utf16', 'method_integer', 'argument_count'],
    rows: bindings.map(r => [utf16Field(r.name), r.method, r.args.length])},
  arguments: {columns: ['binding_index', 'argument_index', 'type', 'value'],
    rows: bindings.flatMap((r, index) => r.args.map((value, argument) =>
      [index, argument, ...primitiveFields(value)]))}
});
console.log(JSON.stringify({compiles,measurements,historical:historical.length,expanded:expanded.length,trees:trees.length,errors:errors.length,runtime,steps:steps.length,direct:direct.length,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
