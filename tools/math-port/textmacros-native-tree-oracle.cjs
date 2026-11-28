'use strict';
// Full-tree Source reference for the independently compiled native frontend
// dumper. This runs the pinned MathJax CJS classes and real package graph.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),packageRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const scopeBytes=fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'));
const scope=JSON.parse(scopeBytes.toString('utf8'));
const scopeHash=crypto.createHash('sha256').update(scopeBytes).digest('hex');
assert.equal(scopeHash,'9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1','scope lock');
const original=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/evidence/textmacros-package-original-source.json'),'utf8'));
const cjsLocks={
  'input/tex.js':'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
  'input/tex/TexParser.js':'106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
  'input/tex/textmacros/TextParser.js':'ec2fa0145f5e90ef231b023a38ed7d9c1064a8cc58a46bb683d46439bd8fb400',
  'input/tex/textmacros/TextMacrosConfiguration.js':'5803611b187a6256cae6289cfc2f61fafa7f93395432d3a7b365f8089334b5cf',
  'input/tex/textmacros/TextMacrosMappings.js':'d331e33a2c3327eeeb2bc1fd5e7e98b0ef17fd7f317aaf91c19f9d665a1a5260',
  'input/tex/textmacros/TextMacrosMethods.js':'59e0abbeffeb9c859b2150d705abb1dc0a59a7ec71fb830ae6379d887c499817',
  'input/tex/base/BaseMethods.js':'b8d5c74e01f85ae391ec8d16ffcd216a4ca4267bd8ffa091c5bfe1462ad7b491',
  'input/tex/ParseUtil.js':'adafa18cefad39e8408bcc0e4eb7103dceb6d16c9e7ea2eb013f23602c3f8941',
  'input/tex/Configuration.js':'86f62a6f6e44fec20156546b7e8d424cac493c7465c2c8ec621c604332a276f3',
  'input/tex/ParseOptions.js':'ff19d70539cb76911c5f1d7673ddfd908183dcf5849c91cb068b19e58da8a9ec',
  'input/tex/Stack.js':'78743f9dfcf1ebf9beb890629d908111be5c1e47f49c16d5ffa0877c5935c385',
  'input/tex/MapHandler.js':'d82e3034776d2392689aad86500b5e60dbe0aa5540853b48f308841879f3cf67',
  'input/tex/StackItem.js':'53baa91d7c27475a91a9b6993235071ab32b04a29736c5920bdda2836ab5a91b',
  'core/MmlTree/MmlFactory.js':'d0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'core/MmlTree/MmlNode.js':'86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
  'core/Tree/Factory.js':'739db2e5760c29d7bb4f1bc7baa08ff8d727f4d117522aff5ede017f812cffef',
};
for(const [file,expected] of Object.entries(cjsLocks)){
  const bytes=fs.readFileSync(path.join(packageRoot,'cjs',file));
  assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),expected,`${file} CJS lock`);
}
for(const module of original.modules){
  if(module.source){
    const lock=scope.files.find(item=>item.source===module.source);assert.ok(lock,`${module.source} absent from scope`);
    const source=fs.readFileSync(path.join(root,'.reference/MathJax-src',module.source));
    assert.equal(crypto.createHash('sha256').update(source).digest('hex'),lock.source_sha256,`${module.source} source lock`);
    const installed=fs.readFileSync(path.join(packageRoot,module.source));
    assert.equal(installed.toString('utf8').replaceAll('\r\n','\n'),source.toString('utf8').replaceAll('\r\n','\n'),`${module.source} installed TS`);
  }
  if(module.cjs){
    const bytes=fs.readFileSync(path.join(packageRoot,'cjs',module.cjs));
    assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),module.cjs_sha256,`${module.cjs} CJS lock`);
  }
}
const load=file=>require(path.join(packageRoot,'cjs',file));
const {TeX}=load('input/tex.js'),TexParser=load('input/tex/TexParser.js').default;
const {TextParser}=load('input/tex/textmacros/TextParser.js');
const {MmlFactory}=load('core/MmlTree/MmlFactory.js');
load('input/tex/textmacros/TextMacrosConfiguration.js');
const cases={};
function plain(value,seen=new WeakSet()){
  if(value===undefined)return{$:'undefined'};if(value===null||typeof value==='string'||typeof value==='boolean')return value;
  if(typeof value==='number')return Number.isNaN(value)?{$:'NaN'}:Object.is(value,-0)?{$:'-0'}:value;
  if(typeof value==='function')return{$:'function',name:value.name||'',length:value.length};
  if(typeof value!=='object')return String(value);if(seen.has(value))return{$:'cycle'};seen.add(value);
  if(Array.isArray(value))return Array.from({length:value.length},(_,i)=>Object.hasOwn(value,i)?plain(value[i],seen):{$:'hole'});
  const out={};for(const key of Reflect.ownKeys(value))out[typeof key==='symbol'?`symbol:${key.description||''}`:key]=plain(value[key],seen);return out;
}
function tree(n){
  if(!n)return null;
  const out={kind:n.kind,inferred:!!n.isInferred,token:!!n.isToken};
  if(n.isToken)out.text=n.getText();
  out.attrs=plain(n.attributes?.getAllAttributes?.()||{});out.properties=plain(n.getAllProperties?.()||{});
  out.children=(n.childNodes||[]).map(tree);return out;
}
function jax(){const value=new TeX({packages:['base','textmacros']});value.setMmlFactory(new MmlFactory());return value;}
function parseTex(source){const session=jax(),parser=new TexParser(source,{},session.parseOptions);return tree(parser.mml());}
function parseText(source,level){
  const session=jax(),main=new TexParser('',{},session.parseOptions);
  const data=session.parseOptions.packageData.get('textmacros');data.texParser=main;
  data.parseOptions.nodeFactory.setMmlFactory(session.mmlFactory);
  return tree(new TextParser(source,{},data.parseOptions,level).mml());
}
const texCases=[['empty',''],['x','x'],['x-plus-one','x+1'],['fraction','\\frac{1}{2}'],['astral','😀'],['superscript','x^2']];
for(const [key,source] of texCases)cases[`tex/${key}`]=parseTex(source);
const macroCases=[
  ['percent','\\%'],['dagger','\\dagger'],['accent',"\\'{e}"],['frak','\\frak{x}'],
  ['quad','\\quad'],['hspace','\\hspace{1em}'],['rule','\\rule{1em}{2pt}'],
  ['reference','\\ref{missing}'],['underline','\\underline{x}'],['llap','\\llap{x}'],
  ['phantom','\\phantom{x}'],['smash','\\smash{x}'],['mml-token','\\mmlToken{mi}{x}'],
  ['emph','\\emph emphatic'],['textbf','\\textbf{bold}'],['textit','\\textit{italic}'],['texttt','\\texttt{mono}'],
];
for(const [key,source] of macroCases)cases[`macro/${key}`]=parseText(source);
const textCases=[
  ['bf','\\bf bold'],['it','\\it italic'],['tiny','\\tiny small'],['emph','\\emph emphatic'],
  ['textbf','\\textbf{bold}'],['textit','\\textit{italic}'],['texttt','\\texttt{mono}'],
  ['spaces','plain  many   spaces'],['nbsp','a\u00a0b'],['quotes',"`open'"],['double-quotes',"``pair''"],
  ['group','{group}'],['math-delimiter','$x$'],
];
for(const [key,source] of textCases)cases[`text/${key}`]=parseText(source);
cases['text/level-2']=parseText('abc',2);
const output={schema:1,scope_sha256:scopeHash,source_modules:original.modules,cases};
const destination=path.join(root,'tools/math-port/profiles/evidence/textmacros-native-tree-source.json');
const serialized=JSON.stringify(output,null,2)+'\n';
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(destination,'utf8').replaceAll('\r\n','\n'),serialized);
else fs.writeFileSync(destination,serialized);
console.log(JSON.stringify({cases:Object.keys(cases).length,destination:path.relative(root,destination)}));
