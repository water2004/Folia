'use strict';
// Independent whole-package Mathtools witness and primitive physical-tree fixtures.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [owner,ts,cjs]of [
 ['MathtoolsConfiguration','7b14b1d6e4324397ab315fa012ceada7999df6cbf3b4c9f09fc3742f8b5a30ae','863415dfbc384852e5b3b39724d50cc453e1bb6e54f8f6a1073b9e5e43b8734d'],
 ['MathtoolsItems','61cadb6dcd19a00d7a6e3bbd7b6eb61a9e9fb72edc8982c83890f08f37a013bb','89bc6d7f2597a48d231743d06d031c7042f9963798cb514a84dabc52de4e023d'],
 ['MathtoolsMethods','ad6562e23ba65cafdb480afc33387982c7f7b14a2886ae7844dfbe64ec44e918','ec67f90855dec0ab60f4933beffe081799247a2ac01572429b696e4e02ed8a5a'],
 ['MathtoolsMappings','57522ac1c99b3154cbe19057c61d365e027a41f3946efa9093d3f1f05f28b1df','2bc22670070f957d770adb74bdb0e6795c6373560adadfc62fbe82fec42fc03b'],
 ['MathtoolsTags','2437a2beea0f0c04fac8697ab407a09672b2395d483b1942aefa51e9df057e47','bcdd1c9f8592d5640fa2b8bb8cade19c29e2d8f732e19a2dfbcb5415135e3e2d'],
 ['MathtoolsUtil','e8f02143609f1201b5c9c39638d0eb3f14a3d5adcb93fc5a6b4c1e888b3903e0','c132df85a45b50812466c8643f9dabd156f5205e5699d514f01292e02469e2d5']
])for(const [file,sha]of [[`.reference/MathJax-src/ts/input/tex/mathtools/${owner}.ts`,ts],[`tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/mathtools/${owner}.js`,cjs]])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=f=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',f));
const {TeX}=load('input/tex.js'),{AbstractMathItem}=load('core/MathItem.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js');
const {MathtoolsMethods,LEGACYCONFIG,LEGACYPRIORITY}=load('input/tex/mathtools/MathtoolsMethods.js');
const {MathtoolsConfiguration}=load('input/tex/mathtools/MathtoolsConfiguration.js');
const NewcommandMethods=load('input/tex/newcommand/NewcommandMethods.js').default;
const {AmsMethods}=load('input/tex/ams/AmsMethods.js'),BaseMethods=load('input/tex/base/BaseMethods.js').default;
let compiles=0;
function fixture(packages=['base','ams','newcommand','mathtools','boldsymbol']){let error='';const jax=new TeX({packages,tags:'ams',maxMacros:1000,formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());return {jax,compile(source){++compiles;error='';const tree=jax.compile(new AbstractMathItem(source,jax,true),reference.document);return {tree,error}}};}
function snapshot(root){let s='';const add=v=>{v=String(v);s+=v.length+':'+v};function visit(n){if(!n){add('null');return}add(n.kind);const attrs=Object.entries(n.attributes.getAllAttributes()).sort(([a],[b])=>a<b?-1:a>b?1:0);add(attrs.length);for(const [key,v]of attrs){add(key);add(v===null||v===undefined?'null':typeof v);add(v===null||v===undefined?'':String(v))}add(n.isToken?n.getText():'');const children=n.isToken?[]:n.childNodes;add(children.length);for(const c of children)visit(c)}visit(root);return s;}
function ok(f,source){const r=f.compile(source);assert.equal(r.error,'',source);assert.equal(f.jax.parseOptions.parsers.length,0);return r.tree;}
function nodes(root,kind){const result=[];root.walkTree(n=>{if(n.kind===kind)result.push(n)});return result;}
assert.equal(LEGACYPRIORITY,4);assert.deepEqual(LEGACYCONFIG,{macro:['mathtools-legacycolonsymbols']});assert.equal(MathtoolsConfiguration.configMethod[1],5);
for(const [name,fn]of [['Array',BaseMethods.Array],['Macro',BaseMethods.Macro],['xArrow',AmsMethods.xArrow],['HandleRef',AmsMethods.HandleRef],['AmsEqnArray',AmsMethods.AmsEqnArray],['MacroWithTemplate',NewcommandMethods.MacroWithTemplate]])assert.equal(MathtoolsMethods[name]===fn,true);
assert.equal(Object.keys(MathtoolsMethods).length,38);
const trees=[],errors=[],steps=[],order=[];
function tree(source){const f=fixture();const r=f.compile(source);assert.equal(r.error,'',source);assert.equal(f.jax.parseOptions.parsers.length,0);trees.push({source,snapshot:snapshot(r.tree)});return r.tree;}
function bad(source){const f=fixture(),r=f.compile(source);assert.notEqual(r.error,'',source);errors.push({source,id:r.error});ok(f,'x');}
for(const env of ['matrix*','pmatrix*','bmatrix*','Bmatrix*','vmatrix*','Vmatrix*','smallmatrix*','psmallmatrix','psmallmatrix*','bsmallmatrix','bsmallmatrix*','Bsmallmatrix','Bsmallmatrix*','vsmallmatrix','vsmallmatrix*','Vsmallmatrix','Vsmallmatrix*']){
 tree(`\\begin{${env}}a&b\\\\c&d\\end{${env}}`);
 if(env.endsWith('*'))for(const alignment of ['[l]','[r]','[]{lr}'])tree(`\\begin{${env}}${alignment}a&b\\\\c&d\\end{${env}}`);
}
for(const env of ['dcases','rcases','drcases','dcases*','rcases*','drcases*','cases*'])tree(`\\begin{${env}}x&first $a$\\\\y&second\\end{${env}}`);
for(const env of ['lgathered','rgathered'])tree(`\\begin{${env}}a+b\\\\c+d\\end{${env}}`);
for(const source of [
 '\\begin{crampedsubarray}{c}a\\\\b\\end{crampedsubarray}', '\\crampedsubstack{a\\\\b}',
 '\\begin{multlined}a+b\\\\c+d\\end{multlined}',
 '\\begin{multlined}[t][3em]a\\\\b\\end{multlined}',
 '\\begin{multlined}[b]a\\\\b\\end{multlined}',
 '\\begin{multlined}[4em]a\\\\b\\end{multlined}',
 '\\begin{multlined} [t]a\\\\b\\end{multlined}',
 '\\begin{multlined}[c] [4em]a\\\\b\\end{multlined}',
 '\\begin{multlined}[]a\\\\b\\end{multlined}',
 '\\begin{multlined}\\shoveleft[1em]{a}\\\\\\shoveright[2em]{b}\\end{multlined}',
 '\\begin{multline}\\shoveleft[]{a}\\\\\\shoveright{b}\\end{multline}',
 '\\begin{spreadlines}{2pt}\\begin{align}a&=b\\\\c&=d\\end{align}\\end{spreadlines}',
 '\\begin{spreadlines}{-20pt}\\begin{gather}a\\\\b\\end{gather}\\end{spreadlines}',
 '\\begin{spreadlines}{1em}x+\\begin{gathered}a\\\\b\\end{gathered}\\end{spreadlines}',
 '\\mathllap{x}+\\mathrlap{x}+\\mathclap{x}',
 '\\textllap{abc}+\\textrlap{def}+\\textclap{ghi}+\\clap{jk}',
 '\\cramped{x}+\\crampedllap{x}+\\crampedrlap{x}+\\crampedclap{x}',
 '\\mathclap[\\displaystyle]{x}+\\cramped[\\scriptstyle]{y}+\\cramped[\\scriptscriptstyle]{z}',
 '\\mathrlap[\\textstyle]{x}+\\cramped[unknown]{y}',
 '\\mathmakebox[2em][c]{x}+\\mathmakebox[][R]{y}+\\mathmakebox[0em][]{z}+\\mathmbox{x+y}',
 '\\mathmakebox[2em][İ]{x}+\\mathmakebox[][Σ]{y}',
 '\\overbracket{x+y}+\\underbracket{x+y}',
 '\\overbracket[][1em]{x}+\\underbracket[bad][2pt]{y}',
 '\\overbracket[2][.5em]{x}+\\underbracket[1px]{y}',
 '\\overbracket[\u00a01px\u00a0]{x}+\\underbracket[\u0130]{y}',
 '\\begin{align}a&=b\\\\\\Aboxed{c&=d}e\\end{align}',
 '\\begin{align}a&\\Aboxed{x&=y}\\end{align}',
 '\\MakeAboxedCommand*\\B\\boxed\\begin{align}\\B{x&=y}\\end{align}',
 '\\MakeAboxedCommand\\B\\fbox\\begin{align}\\B{x&=y}\\end{align}',
 '\\begin{align}\\MoveEqLeft x&=y\\\\\\MoveEqLeft[3]z&=w\\end{align}',
 '\\begin{align}a&=b\\\\\\ArrowBetweenLines c&=d\\end{align}',
 '\\begin{align}a&=b\\\\\\ArrowBetweenLines*[\\Downarrow]c&=d\\end{align}',
 '\\vdotswithin{abc}+\\vdotswithin{}',
 '\\begin{align}a&=b\\\\\\shortvdotswithin{=}\\\\c&=d\\end{align}',
 '\\begin{align}a&=b\\\\\\shortvdotswithin*{=}\\\\c&=d\\end{align}',
 '\\begin{align}a&=b\\\\\\MTFlushSpaceAbove c&=d\\MTFlushSpaceBelow e&=f\\end{align}',
 '\\nuparrow+\\ndownarrow+\\bigtimes',
 '\\splitfrac{a+b}{c+d}+\\splitdfrac{x}{y}',
 '\\xmathstrut{.2}+\\xmathstrut[.3]{-.4}+\\xmathstrut[]{+01.00}',
 '\\prescript{a}{b}{X}+\\prescript{}{}{Y}+\\prescript{a}{}{Z}',
 '\\prescript{}{b}{X}_c^d+\\prescript{a}{b}{X}_c+\\prescript{a}{b}{X}^d',
 '\\prescript{ {} }{ {} }{X}',
 '\\adjustlimits\\lim_{n}\\max_{k}',
 '\\lparen x\\rparen+\\left\\lparen x\\right\\rparen'
])tree(source);
const arrows=['xleftrightarrow','xLeftarrow','xRightarrow','xLeftrightarrow','xhookleftarrow','xhookrightarrow','xmapsto','xrightharpoondown','xleftharpoondown','xrightleftharpoons','xrightharpoonup','xleftharpoonup','xleftrightharpoons','xlongrightarrow','xlongleftarrow','xLongrightarrow','xLongleftarrow'];
for(const command of arrows)tree(`\\${command}[d]{u}+\\${command}{}`);
const relations=['coloneqq','Coloneqq','coloneq','Coloneq','eqqcolon','Eqqcolon','eqcolon','Eqcolon','colonapprox','Colonapprox','colonsim','Colonsim','dblcolon','approxcolon','Approxcolon','simcolon','Simcolon','colondash','Colondash','dashcolon','Dashcolon'];
for(const command of relations){tree(`\\${command}`);tree(`\\mathtoolsset{use-unicode=true}\\${command}`)}
for(const source of [
 '\\ordinarycolon+\\vcentercolon+\\MTThinColon+a:b',
 '\\mathtoolsset{centercolon=true,centercolon-offset=.1em,thincolon-dx=-.2em,thincolon-dw=-.3em}a:b+\\MTThinColon',
 '\\DeclarePairedDelimiter{\\abs}{\\lvert}{\\rvert}\\abs{x}+\\abs*{x}+\\abs[\\Big]{x}',
 '\\DeclarePairedDelimiterX{\\pair}[2]{(}{)}{#1,\\delimsize|#2}\\pair{x}{y}+\\pair*{x}{y}+\\pair[\\big]{x}{y}',
 '\\DeclarePairedDelimiterXPP{\\pair}[2]{A}{[}{]}{Z}{#1+\\delimsize|#2}\\pair{x}{y}',
 '\\DeclarePairedDelimiters{\\abs}{|}{|}\\abs{x}',
 '\\DeclarePairedDelimitersX{\\p}[1]{(}{)}{#1}\\p{x}',
 '\\DeclarePairedDelimitersXPP{\\p}[1]{A}{(}{)}{Z}{#1}\\p{x}',
 '\\DeclarePairedDelimiterX{\\p}{(}{)}{X}\\p y',
 '\\DeclarePairedDelimiterX{\\p}[]{(}{)}{X}\\p y',
 '\\DeclarePairedDelimiterX{\\p}[0]{(}{)}{X}\\p y',
 '\\DeclarePairedDelimiter{\\abs}{|}{|}\\abs{}',
 '\\DeclarePairedDelimiterXPP{\\p}[1]{\\sum\\limits}{(}{)}{_n}{#1}\\p{x}',
 '\\mathtoolsset{prescript-sup-format=\\mathbf,prescript-sub-format=\\mathit,prescript-arg-format=\\mathsf}\\prescript{a}{b}{X}',
 '\\mathtoolsset{smallmatrix-align=r}\\begin{smallmatrix*}a&b\\end{smallmatrix*}',
 '\\mathtoolsset{multlined-gap=2em,multlined-pos=t,multlined-width=4em,firstline-afterskip=1em,lastline-preskip=.5em}\\begin{multlined}a\\\\b\\end{multlined}',
 '\\mathtoolsset{shortvdotsadjustabove=.4em,shortvdotsadjustbelow=.3em}\\begin{align}a&=b\\\\\\shortvdotswithin{=}\\\\c&=d\\end{align}',
 '\\newtagform{box}{[}{]}\\usetagform{box}\\begin{equation}x\\label{x}\\end{equation}',
 '\\renewtagform{new}[\\mathbf]{<}{>}\\usetagform{new}\\begin{equation}x\\end{equation}',
 '\\newtagform{box}{[}{]}\\usetagform{box}\\renewtagform{box}{(}{)}\\begin{equation}x\\end{equation}',
 '\\usetagform{}\\begin{equation}x\\tag{Q}\\end{equation}'
])tree(source);
for(const source of [
 '\\begin{matrix*}[]x\\end{matrix*}', '\\begin{smallmatrix*}[]x\\end{smallmatrix*}',
 '\\begin{matrix*}[q]x\\end{matrix*}', '\\begin{multlined}[bad]x\\end{multlined}',
 '\\shoveleft{x}', '\\begin{multlined}a\\shoveleft{x}\\end{multlined}',
 '\\mathclap', '\\cramped', '\\mathmakebox', '\\mathmbox', '\\overbracket', '\\underbracket[.1em]',
 '\\Aboxed{x&y}', '\\MakeAboxedCommand*\\sin\\boxed', '\\MakeAboxedCommand x',
 '\\ArrowBetweenLines', '\\begin{align}a\\ArrowBetweenLines\\end{align}',
 '\\vdotswithin', '\\shortvdotswithin', '\\MTFlushSpaceAbove', '\\MTFlushSpaceBelow',
 '\\begin{array}{c}\\shortvdotswithin{x}\\end{array}',
 '\\DeclarePairedDelimiter{\\sin}{(}{)}', '\\DeclarePairedDelimiter{\\p}{(}',
 '\\DeclarePairedDelimiterX{\\p}[bad]{(}{)}{#1}',
 '\\DeclarePairedDelimiterX{\\p}[0]{(}{)}{#1}\\p',
 '\\DeclarePairedDelimiterX{\\p}[0]{(}{)}{##}\\p',
 '\\DeclarePairedDelimiterX{\\p}{(}{)}{#1}\\p',
 '\\DeclarePairedDelimiterX{\\p}[]{(}{)}{#1}\\p',
 '\\DeclarePairedDelimiterX{\\p}[2]{(}{)}{#1+#2}\\p{x}',
 '\\DeclarePairedDelimiterX{\\p}[0]{(}{)}{\\p}\\p',
 '\\DeclarePairedDelimiterX{\\p}[1]{(}{)}{\\delimsize#1}\\p[\\delimsize]{x}',
 '\\DeclarePairedDelimiter{\\p}{(}{)}\\DeclarePairedDelimiter{\\p}{[}{]}',
 '\\DeclarePairedDelimiterXPP{\\p}[1]{A}{(}{)}',
 '\\splitfrac{x}', '\\xmathstrut{1e2}', '\\xmathstrut[bad]{.2}', '\\prescript{a}{b}',
 '\\xlongrightarrow', '\\xmapsto[]',
 '\\newtagform{}{[}{]}', '\\newtagform{box}{[}{]}\\newtagform{box}{(}{)}',
 '\\newtagform{box}{[}{]}\\newtagform{box}{(}', '\\usetagform{absent}',
 '\\mathtoolsset{unknown=1}', '\\mathtoolsset{pariedDelimiters=x}', '\\mathtoolsset{tagforms=x}',
 '\\mathtoolsset{allow-mathtoolsset=false}', '\\mathtoolsset{centercolon=true,unknown=1}',
 '\\mathtoolsset{centercolon={bad}', '\\begin{spreadlines}{2pt}\\begin{equation}x\\end{spreadlines}'
])bad(source);
function sequence(group,sources){const f=fixture();for(const source of sources){const r=f.compile(source);steps.push({group,source,snapshot:r.error?'':snapshot(r.tree),error:r.error});if(!r.error)assert.equal(f.jax.parseOptions.parsers.length,0)}return f;}
sequence(1,['\\DeclarePairedDelimiter{\\abs}{|}{|}','\\abs{x}+\\abs*{x}','\\let\\saved\\abs\\def\\abs#1{Z}','\\saved{x}+\\abs{x}','\\DeclarePairedDelimiter{\\abs}{[}{]}','\\saved{y}']);
sequence(2,['\\mathtoolsset{centercolon=true,use-unicode=true}a:b+\\coloneqq','a:b+\\coloneqq','{\\mathtoolsset{centercolon=false,use-unicode=false}}','a:b+\\coloneqq','\\mathtoolsset{centercolon=true,unknown=1}','a:b+\\coloneqq']);
sequence(3,['\\mathtoolsset{legacycolonsymbols=true}\\coloneq','\\coloneq','\\mathtoolsset{legacycolonsymbols=false}\\coloneq','\\mathtoolsset{legacycolonsymbols=one}\\coloneq','\\mathtoolsset{legacycolonsymbols=two}\\coloneq','\\mathtoolsset{legacycolonsymbols=two}\\coloneq','\\mathtoolsset{legacycolonsymbols=true}\\coloneq','\\mathtoolsset{legacycolonsymbols=false}\\coloneq']);
sequence(4,['\\newtagform{box}{[}{]}\\usetagform{box}','\\begin{equation}x\\label{first}\\end{equation}','\\renewtagform{box}{(}{)}','\\begin{equation}y\\label{old}\\end{equation}','\\usetagform{box}','\\begin{equation}z\\label{new}\\end{equation}','\\usetagform{}','\\eqref{first}+\\eqref{old}+\\eqref{new}+\\ref{first}','\\usetagform{absent}','\\begin{equation}q\\label{recover}\\end{equation}']);
sequence(5,['\\MakeAboxedCommand*\\B\\boxed','\\begin{align}\\B{x&=y}\\end{align}','\\let\\savedB\\B','\\begin{align}\\savedB{a&=b}\\end{align}']);
sequence(6,['\\mathtoolsset{centercolon=true,centercolon=false}a:b','\\mathtoolsset{thincolon-dx=-.1em,thincolon-dw=-.2em,centercolon-offset=.2em}\\MTThinColon','\\MTThinColon']);
// Keep all 42 mathematical inputs from the earlier independent Source oracle.
// Read its literal case table without executing its obsolete reflection/dump runner.
const historical=fs.readFileSync(path.join(root,'tools/math-port/mathtools-original-oracle.cjs'),'utf8');
const historicalCases=[...historical.matchAll(/\['(mt-[^']+)', String\.raw`([^`]*)`\]/g)];
assert.equal(historicalCases.length,42);
let historicalErrors=0;
for(const [,name,source]of historicalCases){const f=fixture(),r=f.compile(source);if(r.error){errors.push({source,id:r.error});ok(f,'x');++historicalErrors;}else{assert.equal(f.jax.parseOptions.parsers.length,0,name);trees.push({source,snapshot:snapshot(r.tree)});}}
// The separate order fixture adds extpfeil before/after mathtools, with all
// other options unchanged from the ordinary tree fixture.
let orderCompiles=0;
for(const reversed of [false,true]){
 const packages=['base','ams','newcommand',...(reversed?['mathtools','extpfeil']:['extpfeil','mathtools']),'boldsymbol'];
 const f=fixture(packages),handler=f.jax.parseOptions.handlers.get('macro');
 const selected=handler.applicable('xmapsto').lookup('xmapsto');
 assert.deepEqual(selected.args,reversed?[0x21a6,6,7]:[0x21a6,10,10]);
 for(const source of ['\\xmapsto{f}','\\xmapsto[below]{above}']){
  const actual=snapshot(ok(f,source));++orderCompiles;
  order.push({reverse:reversed,source,snapshot:actual});
  if(!reversed)assert.equal(actual,snapshot(ok(fixture(),source)));
  else assert.notEqual(actual,snapshot(ok(fixture(),source)));
 }
}
// Source accepts container/scalar mutation; it is excluded from native tree fixtures.
{const f=fixture();ok(f,'\\mathtoolsset{pairedDelimiters=value}');assert.equal(f.jax.parseOptions.options.mathtools.pairedDelimiters,'value');f.jax.parseOptions.options.mathtools['allow-mathtoolsset']=false;assert.equal(f.compile('\\mathtoolsset').error,'ForbiddenMathtoolsSet');}
// Source's unconstrained scalar type changes can fail at string consumers.
// Native string fields reject such assignments; these are not success fixtures.
const typeExceptions=[];
for(const source of ['\\mathtoolsset{multlined-pos=true}\\begin{multlined}x\\end{multlined}',
 '\\mathtoolsset{smallmatrix-align=true}\\begin{smallmatrix*}x\\end{smallmatrix*}']){
 const f=fixture();let caught;
 try{f.compile(source);}catch(e){caught=e;}
 assert.equal(caught instanceof TypeError,true,source);
 typeExceptions.push({source,name:caught.name,message:caught.message});
}
const {utf16Field, byteField, writeTables} = require('./fixture-data.cjs');
writeTables(path.join(root, 'tests/math/fixtures/packages/mathtools'), {
  'trees': {columns: ["source_utf16", "snapshot_utf16"],
    rows: trees.map(c => [utf16Field(c.source), utf16Field(c.snapshot)])},
  'errors': {columns: ["source_utf16", "id_bytes"],
    rows: errors.map(c => [utf16Field(c.source), byteField(c.id)])},
  'steps': {columns: ["group_integer", "source_utf16", "snapshot_utf16", "error_bytes"],
    rows: steps.map(c => [c.group, utf16Field(c.source), utf16Field(c.snapshot), byteField(c.error)])},
  'order': {columns: ["reverse_boolean", "source_utf16", "snapshot_utf16"],
    rows: order.map(c => [+!!c.reverse, utf16Field(c.source), utf16Field(c.snapshot)])},
});
console.log(JSON.stringify({compiles,trees:trees.length,errors:errors.length,steps:steps.length,groups:6,historicalCases:historicalCases.length,historicalErrors,order:order.length,orderCompiles,containerBoundary:1,typeExceptions,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
