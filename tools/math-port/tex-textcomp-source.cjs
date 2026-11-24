'use strict';
// Hash-locked original Textcomp maps/configuration through actual TeX/MML.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [name,ts,cjs]of [
 ['TextcompConfiguration','069b5df106bc4aaf060317b96ddbb8f8d5708ca34842f0c39e3290c53ec2492c','6a50dc60e8240222bc926771bf7b94a9c4e129ba32a25d5da1a92bf5e92883ac'],
 ['TextcompMappings','21e81e2467f13c9d3d52463dfdd4f80f002bf996da1d04ad5e011303ae68899b','802b1a41f0084b904abd4aab797d3cf3d5f66b80c0662a65038ec155a11b1c31']
])for(const [file,sha]of [['.reference/MathJax-src/ts/input/tex/textcomp/'+name+'.ts',ts],['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/textcomp/'+name+'.js',cjs]])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();
assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=file=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',file));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const {NodeFactory}=load('input/tex/NodeFactory.js');
let cases=0;
function fixture(text=true,aliases=false,reversed=false){
 const packages=['base',...(text?(reversed?['textcomp','textmacros']:['textmacros','textcomp']):['textcomp']),...(aliases?['newcommand']:[])];
 let error='';const jax=new TeX({packages,formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());
 return {jax,compile(s){++cases;error='';const tree=jax.compile(new AbstractMathItem(s,jax,false),reference.document);return {tree,error}}};
}
function nodes(root,kind){const result=[];root.walkTree(n=>{if(n.kind===kind)result.push(n)});return result;}
function ok(f,s){const result=f.compile(s);assert.equal(result.error,'',s);return result.tree;}
const f=fixture(),math=fixture(false),entries=[...f.jax.parseOptions.handlers.retrieve('textcomp-macros').map];
assert.equal(entries.length,117);
const {utf16Field, byteField, writeTables} = require('./fixture-data.cjs');
const fixturePath = path.join(root, 'tests/math/fixtures/packages/textcomp');
writeTables(fixturePath, {
  "commands": {columns: ["name_utf16", "glyph_utf16", "font_utf16"],
    rows: entries.map(([name, m]) => [name, m.args[0], m.args[1] || ''].map(utf16Field))},
});
for(const [name,m]of entries){
 const glyph=m.args[0],font=m.args[1];
 for(const [instance,source]of [[math,'\\'+name],[f,'\\text{\\'+name+'}']]){
  const texts=nodes(ok(instance,source),'mtext');assert.equal(texts.length,1,name);assert.equal(texts[0].getText(),glyph,name);
  if(font)assert.equal(texts[0].attributes.get('mathvariant'),font,name);
 }
 const texts=nodes(ok(f,'\\text{L\\'+name+' R}'),'mtext');
 assert.deepEqual(texts.map(n=>n.getText()),font?['L',glyph,'R']:['L'+glyph+'R'],name);
}
assert.deepEqual(f.jax.parseOptions.options.textmacros.packages,['text-base','text-textcomp']);
assert.equal(nodes(ok(fixture(true,false,true),'\\text{\\texteuro}'),'mtext')[0].getText(),'€');
{
 const requests=[];f.jax.parseOptions.nodeFactory.set('node',(factory,kind,children,def,txt)=>{if(kind==='mtext')requests.push({def:{...def},text:txt});return NodeFactory.createNode(factory,kind,children,def,txt)});
 const root=ok(f,'\\textoneoldstyle'),texts=nodes(root,'mtext');assert.equal(requests.length,1);assert.deepEqual(requests[0].def,{mathvariant:'-tex-oldstyle'});assert.equal(requests[0].text===texts[0].childNodes[0],true);
}
{
 const alias=fixture(true,true);ok(alias,'\\let\\saved\\texteuro\\newcommand{\\curr}{\\texteuro}');
 assert.equal(nodes(ok(alias,'\\saved'),'mtext')[0].getText(),'€');assert.equal(nodes(ok(alias,'\\text{\\curr}'),'mtext')[0].getText(),'€');
 assert.equal(alias.compile('\\text{\\saved}').error,'MathMacro');ok(alias,'\\text{\\texteuro}');
}
console.log(JSON.stringify({commands:entries.length,cases,sourceFixture:path.relative(root, fixturePath),compiled_source_sha256:reference.provenance.compiled_source_sha256}));
