'use strict';
// Original Bboldx maps plus real TeX/MML font outcomes; independent C++ fixture.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [name,ts,cjs]of [
 ['BboldxMethods','2f61aa0f168cadabfbcd64541d2e4f79116197093df599c3aab75eb087e6f70f','36fcf862ced456fb8d3ea4b96b75fdd6af9a2c11407c475c67a4c1003c964519'],
 ['BboldxMappings','53a1530cc8f4ff2cd3ae722d0a82fb038dc599d4f342d8abaf2c0da5ffb5912d','0c4ee88f5ad810c075b85e5817b1f50d248bce818c5d857e2dede2d4d713ab6a'],
 ['BboldxConfiguration','3b19e38200f09906aaaef6fe37c5539975e41cad2be3f82fb82e1be9a0481494','ecf0f53871e8f5ac4705132855f09179f9b5c974524c258c5aa87581be1dc8b0']
])for(const [file,hash]of [['.reference/MathJax-src/ts/input/tex/bboldx/'+name+'.ts',ts],['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/bboldx/'+name+'.js',cjs]])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),hash,file);
const reference=require('./reference-profile.cjs').createReference();
assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=file=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',file));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const {MapHandler}=load('input/tex/MapHandler.js'),{NodeFactory}=load('input/tex/NodeFactory.js');
const {BboldxMethods:methods}=load('input/tex/bboldx/BboldxMethods.js');
assert.equal(Object.keys(methods).length,7);assert.equal(methods.Macro===load('input/tex/base/BaseMethods.js').default.Macro,true);
let cases=0;
function fixture(bfbb=false,light=false,text=true,aliases=false,reversed=false,selected=true){
 let error='',message='';const packages=reversed?[...(selected?['bboldx']:[]),'base',...(text?['textmacros']:[])]:['base',...(text?['textmacros']:[]),...(selected?['bboldx']:[])];if(aliases)packages.push('newcommand');
 const jax=new TeX({packages,...(selected?{bboldx:{bfbb,light}}:{}),formatError:(j,e)=>{error=e.id;message=e.message;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());
 return {jax,compile(source){++cases;error='';message='';const tree=jax.compile(new AbstractMathItem(source,jax,false),reference.document);return {tree,error,message}}};
}
function nodes(root,kind){const result=[];root.walkTree(n=>{if(n.kind===kind)result.push(n)});return result;}
function ok(f,source){const result=f.compile(source);assert.equal(result.error,'',source);return result.tree;}
function fontNode(root){const result=[];root.walkTree(n=>{if(n.isToken&&String(n.attributes.get('mathvariant')).startsWith('-bboldx'))result.push(n)});assert.equal(result.length,1);return result[0];}
const combinations=[[false,false],[false,true],[true,false],[true,true]],math=combinations.map(x=>fixture(...x,false)),text=combinations.map(x=>fixture(...x,true));
const methodName=fn=>{const name=Object.entries(methods).find(([,body])=>body===fn)?.[0];assert.ok(name);return name};
const charRows=[],commandRows=[];
for(const prefix of ['','text-']){
 for(const suffix of ['mathchar0miNormal','delimiterNormal','mathchar0miBold','delimiterBold']){
  const mapName=prefix+'bboldx-'+suffix,map=MapHandler.getMap(mapName);assert.ok(map);assert.equal(methodName(map.parser),suffix);
  for(const [name,token]of map.map){const row={map:mapName,name,glyph:token.char,method:suffix,text:!!prefix,fonts:[]};for(let i=0;i<4;++i){const n=fontNode(ok(prefix?text[i]:math[i],prefix?'\\text{\\'+name+'}':'\\'+name));assert.equal(n.getText(),token.char,name);assert.equal(n.kind,suffix.startsWith('delimiter')?'mo':'mi');if(n.kind==='mo')assert.equal(n.attributes.get('stretchy'),false);row.kind=n.kind;row.fonts.push(n.attributes.get('mathvariant'));}charRows.push(row);}
 }
 const mapName=prefix+'bboldx',map=MapHandler.getMap(mapName);assert.ok(map);
 for(const [name,macro]of map.map){const method=methodName(macro.func),row={map:mapName,name,method,args:macro.args,text:!!prefix,fonts:[]};for(let i=0;i<4;++i){const source='\\'+name+(method==='Macro'?'':'{x}'),n=fontNode(ok(prefix?text[i]:math[i],prefix?'\\text{'+source+'}':source));row.kind=n.kind;row.glyph=n.getText();row.fonts.push(n.attributes.get('mathvariant'));}commandRows.push(row);}
}
assert.equal(charRows.length,168);assert.equal(commandRows.length,12);
assert.equal(charRows.find(r=>r.name==='bfbblambda').glyph,'οBB');assert.equal(charRows.find(r=>r.name==='txtbfbblambda').glyph,'λ');assert.equal(commandRows.some(r=>r.name==='jmathbfbb'),true);
const {utf16Field: u, writeTables} = require('./fixture-data.cjs');
const fixtureDirectory = path.join(root, 'tests/math/fixtures/packages/bboldx');
const fixtureTables = {
 characters: {columns: ['map','name','glyph','method','kind','font00','font01','font10','font11','text'],
   rows: charRows.map(r => [u(r.map),u(r.name),u(r.glyph),r.method,r.kind,...r.fonts.map(u),+r.text])},
 commands: {columns: ['map','name','method','argument0','argument1','argument2','count','glyph','kind','font00','font01','font10','font11','text'],
   rows: commandRows.map(r => [u(r.map),u(r.name),r.method,...[...r.args,...Array(3-r.args.length).fill('')].map(u),r.args.length,u(r.glyph),r.kind,...r.fonts.map(u),+r.text])}
};

{
 const f=text[0];assert.deepEqual(f.jax.parseOptions.options.textmacros.packages,['text-base','text-bboldx']);assert.equal(f.jax.parseOptions.packageData.get('textmacros').parseOptions.options===f.jax.parseOptions.options,true);
 for(const reverse of [false,true])assert.equal(fontNode(ok(fixture(false,false,true,false,reverse),'\\mathbb{x}')).attributes.get('mathvariant'),'-bboldx');
 assert.equal(nodes(ok(fixture(false,false,true,false,false,false),'\\mathbb{x}'),'mi')[0].attributes.get('mathvariant'),'double-struck');
 assert.equal(fixture(false,false,true,false,false,false).compile('\\bbGamma').error,'UndefinedControlSequence');
 assert.equal(nodes(ok(math[0],'\\text{\\textbb{x}}'),'mtext')[0].getText(),'\\textbb{x}');ok(math[0],'\\mathbb{x}');
 const tree=ok(f,'\\mathbf{\\mathbb{x}y}z');assert.deepEqual(nodes(tree,'mi').map(n=>[n.getText(),n.attributes.get('mathvariant')]),[['x','-bboldx'],['y','bold'],['z','italic']]);
 f.jax.parseOptions.options.bboldx.bfbb=true;f.jax.parseOptions.options.bboldx.light=true;assert.equal(fontNode(ok(f,'\\text{\\textbb{x}}')).attributes.get('mathvariant'),'-bboldx-bold');f.jax.parseOptions.options.bboldx.bfbb=false;f.jax.parseOptions.options.bboldx.light=false;
 assert.deepEqual(f.jax.parseOptions.options.textmacros.packages,['text-base','text-bboldx']);
}
{
 const f=fixture(),requests=[];f.jax.parseOptions.nodeFactory.set('token',(factory,kind,def,text)=>{const n=NodeFactory.createToken(factory,kind,def,text);requests.push({kind,def:{...def},text,node:n});return n});
 const tree=ok(f,'\\bbGamma+\\bfbbLparen'),glyphs=nodes(tree,'mi').concat(nodes(tree,'mo').filter(n=>n.getText()==='('));
 const chosen=requests.filter(r=>r.text==='Γ'||r.text==='(');assert.deepEqual(chosen.map(r=>r.def),[{mathvariant:'-bboldx'},{stretchy:false,mathvariant:'-bboldx-bold'}]);for(const r of chosen)assert.equal(glyphs.includes(r.node),true);
}
const errors=[['\\mathbb','MissingArgFor'],['\\mathbfbb','MissingArgFor'],['\\text{\\textbb}','MissingArgFor'],['\\text{\\textbfbb}','MissingArgFor'],['\\mathbb{\\unknown}','UndefinedControlSequence']];
for(const [source,id]of errors){const f=fixture();assert.equal(f.compile(source).error,id,source);ok(f,'\\mathbb{x}');}
{
 const f=fixture(false,false,true,true);ok(f,'\\let\\savedFont\\mathbb\\let\\savedChar\\bbGamma\\let\\savedDelim\\bfbbLparen\\let\\savedMacro\\imathbb\\let\\savedBold\\bfbbGamma\\let\\savedNormalDelim\\bbLparen');for(const [command,glyph]of [['savedFont{x}','x'],['savedChar','Γ'],['savedDelim','('],['savedMacro','ı'],['savedBold','Γ'],['savedNormalDelim','(']])assert.equal(fontNode(ok(f,'\\'+command)).getText(),glyph);
 f.jax.parseOptions.options.bboldx.light=true;assert.equal(fontNode(ok(f,'\\savedChar')).attributes.get('mathvariant'),'-bboldx-light');assert.equal(fontNode(ok(f,'\\savedDelim')).attributes.get('mathvariant'),'-bboldx');assert.equal(fontNode(ok(f,'\\savedBold')).attributes.get('mathvariant'),'-bboldx');assert.equal(fontNode(ok(f,'\\savedNormalDelim')).attributes.get('mathvariant'),'-bboldx-light');assert.equal(f.compile('\\text{\\savedChar}').error,'MathMacro');ok(f,'\\text{\\txtbbGamma}');
}
writeTables(fixtureDirectory, fixtureTables);
console.log(JSON.stringify({maps:10,characters:charRows.length,commands:commandRows.length,combinations:4,cases,errorCases:errors.length,sourceFixture:path.relative(root,fixtureDirectory),compiled_source_sha256:reference.provenance.compiled_source_sha256}));
