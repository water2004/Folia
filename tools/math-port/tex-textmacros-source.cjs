'use strict';
// Pinned TextParser/TextMacros algorithms through actual TeX, factories and MML.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [name,ts,cjs]of [
 ['TextParser','538059c8482022c1a4bc6f8e763fdc21993fa6353f1956655ae11c9271788a30','ec2fa0145f5e90ef231b023a38ed7d9c1064a8cc58a46bb683d46439bd8fb400'],
 ['TextMacrosMethods','15b736370eed35d3e65af14d772bfe1b1ad3287844ed44f908f6a9b14004d9c3','59e0abbeffeb9c859b2150d705abb1dc0a59a7ec71fb830ae6379d887c499817'],
 ['TextMacrosMappings','9c0e47239e9b9c59e47d045fb56d67b8bfe3d025214641b2ba7f56ad60202cb2','d331e33a2c3327eeeb2bc1fd5e7e98b0ef17fd7f317aaf91c19f9d665a1a5260'],
 ['TextMacrosConfiguration','5d712a805bfe3ff3db425f24c1887eab300782a8570363abbbe77641222faefc','5803611b187a6256cae6289cfc2f61fafa7f93395432d3a7b365f8089334b5cf']
])for(const [file,hash]of [['.reference/MathJax-src/ts/input/tex/textmacros/'+name+'.ts',ts],['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/textmacros/'+name+'.js',cjs]])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),hash,file);
const reference=require('./reference-profile.cjs').createReference();
assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=file=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',file));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const TexParser=load('input/tex/TexParser.js').default,{TextParser}=load('input/tex/textmacros/TextParser.js');
const {NodeFactory}=load('input/tex/NodeFactory.js'),{TextMacrosMethods:methods}=load('input/tex/textmacros/TextMacrosMethods.js');
const base=load('input/tex/base/BaseMethods.js').default;
const aliases=['Macro','Spacer','Hskip','rule','Rule','HandleRef','UnderOver','Lap','Phantom','Smash','MmlToken'];
assert.equal(Object.keys(methods).length,29);for(const name of aliases)assert.equal(methods[name]===base[name],true);
let cases=0;
function fixture(packages=[],options={}){let error='',message='';const jax=new TeX({...options,packages:['base','textmacros',...packages],maxMacros:100,formatError:(j,e)=>{error=e.id;message=e.message;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());const data=jax.parseOptions.packageData.get('textmacros');data.parseOptions.nodeFactory.setMmlFactory(jax.mmlFactory);assert.equal(jax.parseOptions.packageData.has('autoload'),false);assert.equal(jax.parseOptions.packageData.has('require'),false);return {jax,data,compile(source){++cases;error='';message='';const tree=jax.compile(new AbstractMathItem(source,jax,false),reference.document);return {tree,error,message}}};}
function nodes(root,kind){const result=[];root.walkTree(n=>{if(n.kind===kind)result.push(n)});return result;}
function text(root){const result=[];root.walkTree(n=>{if(n.isToken)result.push(n.getText())});return result.join('');}
function ok(f,source){const r=f.compile(source);assert.equal(r.error,'',source);return r.tree;}
const plain=[['',''],['abc 日本😀','abc 日本😀'],[' a   b\t\nc ',' a b c '],['``double\'\' `single\'','“double” ‘single’'],['a%comment\nb','ab'],['a%tail','a'],['a~b','a b'],['\\$\\_\\%\\{\\}\\ \\&\\#','$_%{} &#'],['\\dagger\\ddagger\\S\\AA\\ldots\\vdots','†‡§Å…⋮']];
{
 const f=fixture();for(const [source,expected]of plain)assert.equal(text(ok(f,'\\text{'+source+'}')),expected,source);
 assert.equal(nodes(ok(f,'\\text{}'),'mtext').length,0);
 const chunks=nodes(ok(f,'\\text{a{\\bf b}c\\emph{d\\emph{e}}f\\frak{g}h}'),'mtext');
 assert.deepEqual(chunks.map(n=>[n.getText(),n.attributes.get('mathvariant')]),[['a','normal'],['b','bold'],['c','normal'],['d','-tex-mathit'],['e','normal'],['f','normal'],['g','fraktur'],['h','normal']]);
}
const sizes=[['Tiny',.5],['tiny',.6],['scriptsize',.7],['SMALL',.7],['Small',.8],['footnotesize',.8],['small',.9],['normalsize',1],['large',1.095],['Large',1.2],['LARGE',1.44],['huge',1.73],['Huge',2.07],['HUGE',2.49]];
{
 const f=fixture();for(const [command,size]of sizes){const n=nodes(ok(f,'\\text{\\'+command+' x}'),'mtext')[0];assert.equal(n.attributes.get('mathsize'),size,command);assert.equal(typeof n.attributes.get('mathsize'),'number');}
 for(const [command,font]of [['rm','normal'],['mit','italic'],['oldstyle','-tex-oldstyle'],['cal','-tex-calligraphic'],['it','-tex-mathit'],['bf','bold'],['sf','sans-serif'],['tt','monospace'],['Bbb','double-struck']]){const arg=command==='Bbb'?'{x}':' x';assert.equal(nodes(ok(f,'\\text{\\'+command+arg+'}'),'mtext')[0].attributes.get('mathvariant'),font,command);}
}
const accents=[["'",'´'],['’','´'],['`','`'],['‘','`'],['^','^'],['"','¨'],['~','~'],['=','¯'],['.','˙'],['u','˘'],['v','ˇ']];
// Keep Source map data separate from the native runner and production tables.
const {utf16Field: u, primitiveFields, writeTables} = require('./fixture-data.cjs');
const mapFixture=fixture(),mappingRows=[],mappingArguments=[];
for(const mapName of ['text-special','text-macros'])for(const [name,macro]of mapFixture.data.parseOptions.handlers.retrieve(mapName).map){
 const method=Object.entries(methods).find(([,fn])=>fn===macro.func)[0];
 for (const [index, value] of macro.args.entries())
   mappingArguments.push([mappingRows.length, index, ...primitiveFields(value)]);
 mappingRows.push([u(mapName),u(name),method,macro.args.length]);
}
writeTables(path.join(root,'tests/math/fixtures/packages/textmacros'), {
 mappings: {columns: ['map','name','method','argument_count'], rows: mappingRows},
 'mapping-arguments': {columns: ['binding','index','type','value'], rows: mappingArguments}
});

{
 const f=fixture();for(const [command,glyph]of accents){const n=nodes(ok(f,'\\text{\\'+command+'{a}}'),'mover');assert.equal(n.length,1);assert.equal(text(n[0].childNodes[0]),'a');assert.equal(text(n[0].childNodes[1]),glyph);}
 for(const source of ['\\text{a $x+1$ b}','\\text{a \\(x+1\\) b}','\\text{$x_{y}$}','\\text{$\\{x\\}$}','\\text{$\\$+x$}']){const tree=ok(f,source);assert.ok(nodes(tree,'TeXAtom').length>=1);assert.equal(nodes(tree,'mi').some(n=>n.getText()==='x'),true);}
}
const baseCases=[['\\,','mspace'],['\\hskip 1em','mspace'],['\\rule{1em}{2em}','mspace'],['\\Rule{1em}{2em}{0em}','mspace'],['\\phantom{x}','mphantom'],['\\smash{x}','mpadded'],['\\llap{x}','mpadded'],['\\underline{x}','munder'],['\\mmlToken{mi}[mathcolor=blue]{x}','mi'],['\\ref{missing}','mtext'],['\\eqref{missing}','mtext']];
{
 const f=fixture();for(const [source,kind]of baseCases)assert.ok(nodes(ok(f,'\\text{'+source+'}'),kind).length>=1,source);
}
{
 const f=fixture(['color','html']);const tree=ok(f,'\\text{a{\\color{red}b}c\\textcolor{blue}{d}\\href{target}{e}}');
 assert.deepEqual(nodes(tree,'mtext').map(n=>[n.getText(),n.attributes.get('mathcolor')]),[['a',''],['b','red'],['c',''],['d','blue'],['e','']]);
 assert.equal(nodes(tree,'mtext').find(n=>n.getText()==='e').attributes.get('href'),'target');
}
const errors=[['^','MathModeOnly'],['_','MathModeOnly'],['&','Misplaced'],['#','Misplaced'],['$x','MathNotTerminated'],['\\(x','MathNotTerminated'],['$x}','MathNotTerminated'],['\\unknown','UndefinedControlSequence'],['\\frac{a}{b}','MathMacro'],['\\href{x}{y}','UndefinedControlSequence'],['\\bf\\emph','MissingArgFor'],['\\u','MissingArgFor']];
for(const [source,id]of errors){const f=fixture();const r=f.compile('\\text{'+source+'}');assert.equal(r.error,id,source);ok(f,'\\text{ok}');}
{
 const f=fixture(['newcommand']);ok(f,'\\newcommand{\\word}[1]{Hello #1}\\let\\saved\\word');assert.equal(text(ok(f,'\\text{\\word{A} \\saved{B}}')),'Hello A Hello B');
}
{
 const f=fixture(['newcommand','configmacros'],{macros:{fixed:['Fixed #1',1]}});ok(f,'\\let\\captured\\fixed\\renewcommand{\\fixed}[1]{Changed #1}');assert.equal(text(ok(f,'\\text{\\captured{A} \\fixed{B}}')),'Fixed A Changed B');
}
{
 const f=fixture(),requests=[];f.data.parseOptions.nodeFactory.set('node',(factory,kind,children,def,t)=>{const result=NodeFactory.createNode(factory,kind,children,def,t);if(kind==='mtext'||kind==='mo')requests.push({kind,def:{...def},node:result});return result});
 const tree=ok(f,'\\text{\\bf\\large a\\u{b}$x$}');
 assert.equal(requests.some(r=>r.kind==='mtext'&&r.def.mathvariant==='bold'),true);
 const accent=requests.find(r=>r.kind==='mo');assert.ok(accent);assert.equal(nodes(tree,'mo').includes(accent.node),true);assert.equal(f.jax.parseOptions.nodeLists.mo.includes(accent.node),true);
 assert.equal(accent.node.attributes.get('mathsize'),1.095);assert.equal(accent.node.attributes.get('mathvariant'),'bold');
}
{
 const f=fixture(),primary=new TexParser('',{},f.jax.parseOptions);primary.mml();f.data.texParser=primary;
 const inner=f.data.parseOptions,levels=[];
 inner.nodeFactory.set('node',(factory,kind,children,def,t)=>{if(kind==='mtext')levels.push(factory.configuration.parser.level);return NodeFactory.createNode(factory,kind,children,def,t)});
 const p=new TextParser('a',{},inner,2),result=p.mml();assert.equal(result.kind,'mstyle');assert.equal(result.attributes.get('scriptlevel'),2);assert.deepEqual(levels,[undefined]);
 const q=new TextParser('',{mathsize:2,mathcolor:'red',mathvariant:'bold'},inner);
 const token=inner.nodeFactory.create('token','mi',{mathsize:0,mathcolor:'blue',mathvariant:'italic'},'x');q.addAttributes(token);assert.equal(token.attributes.get('mathsize'),0);assert.equal(token.attributes.get('mathcolor'),'blue');assert.equal(token.attributes.get('mathvariant'),'italic');q.mml();
 const r=new TextParser('',{},inner),a=inner.nodeFactory.create('token','mi',{},'a'),b=inner.nodeFactory.create('token','mi',{},'b'),inferred=inner.nodeFactory.create('node','inferredMrow',[a,b]);r.PushMath(inferred);const math=r.mml();assert.equal(math.kind,'TeXAtom');assert.equal(math.childNodes[0].childNodes[0].kind,'mrow');const real=math.childNodes[0].childNodes[0];assert.equal(real.childNodes[0]===a,true);assert.equal(real.childNodes[1]===b,true);
 for(const source of ['}','$x}']){inner.clear();assert.throws(()=>new TextParser(source,{},inner),e=>e.id==='ExtraCloseMissingOpen');}
}
{
 const {Configuration}=load('input/tex/Configuration.js'),ParseOptions=load('input/tex/ParseOptions.js').default;
 Configuration.create('text-style-test',{parser:'text',options:{mathStyle:'ISO'}});
 const f=fixture([],{mathStyle:'French',textmacros:{packages:['text-base','text-style-test']}});
 assert.equal(f.jax.parseOptions.options.mathStyle,'French');assert.equal(f.data.parseOptions.options===f.jax.parseOptions.options,true);
 assert.equal(f.data.parseOptions.mathStyle===ParseOptions.getVariant.get('ISO'),true);
}
console.log(JSON.stringify({methods:18,aliases:aliases.length,mapEntries:mappingRows.length,sizes:sizes.length,accents:accents.length,cases,errorCases:errors.length,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
