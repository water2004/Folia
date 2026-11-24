'use strict';
// Real pinned ColortblConfiguration and MML; no production JavaScript runtime.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [file,sha]of [
 ['.reference/MathJax-src/ts/input/tex/colortbl/ColortblConfiguration.ts','0a10559b100eb9576ac5d1bcd5db4e5e288144a9de753a866bca7c87f819464c'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/colortbl/ColortblConfiguration.js','b2da866632dcecb74b05559fa13c0fdd3d79c9d2b4a927bb22a587ef6c76227e']
])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=f=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',f));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const {ColortblConfiguration,ColorArrayItem}=load('input/tex/colortbl/ColortblConfiguration.js'),{ColorConfiguration}=load('input/tex/color/ColorConfiguration.js');
const TexParser=load('input/tex/TexParser.js').default;
let cases=0;function fixture(color=false,colortbl=true,reversed=false){let error='';const packages=['base','ams',...(color?['color']:[]),...(colortbl?['colortbl']:[])];if(reversed){packages.splice(packages.indexOf('colortbl'),1);packages.unshift('colortbl')}const jax=new TeX({packages,maxMacros:1000,formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());return {jax,compile(source){++cases;error='';const tree=jax.compile(new AbstractMathItem(source,jax,true),reference.document);return {tree,error}}};}
function nodes(root,kind){const a=[];root.walkTree(n=>{if(n.kind===kind)a.push(n)});return a;}
function ok(f,source){const r=f.compile(source);assert.equal(r.error,'',source);assert.equal(f.jax.parseOptions.parsers.length,0);return r.tree;}
function snapshot(root){let s='';const add=v=>{v=String(v);s+=v.length+':'+v};function visit(n){if(!n){add('null');return}add(n.kind);const attrs=Object.entries(n.attributes.getAllAttributes()).sort(([a],[b])=>a<b?-1:a>b?1:0);add(attrs.length);for(const [key,v]of attrs){add(key);add(v===null||v===undefined?'null':typeof v);add(v===null||v===undefined?'':String(v))}add(n.isToken?n.getText():'');const children=n.isToken?[]:n.childNodes;add(children.length);for(const c of children)visit(c)}visit(root);return s;}
assert.equal(ColortblConfiguration.priority,10);assert.equal(ColortblConfiguration.configMethod[1],10);assert.deepEqual(ColortblConfiguration.handler.macro,['colortbl']);
const captures=[],errors=[],direct=[];
const sources=[
 '\\begin{array}{cc}a&b\\\\c&d\\end{array}',
 '\\begin{array}{cc}\\columncolor{yellow}a&\\columncolor{green}b\\\\c&d\\end{array}',
 '\\begin{array}{cc}\\rowcolor{red}\\columncolor{yellow}\\cellcolor{blue}a&b\\\\c&d\\end{array}',
 '\\begin{array}{ccc}\\columncolor{yellow}a&b&\\columncolor{green}c\\\\d&e&f\\end{array}',
 '\\begin{array}{cc}\\cellcolor{red}a&b\\\\\\rowcolor{blue}c&d\\\\e&f\\end{array}',
 '\\begin{array}{cc}a&b\\\\\\rowcolor{green}c&d\\end{array}',
 '\\begin{array}{cc}\\columncolor{red}a&b\\\\\\columncolor{red}c&d\\end{array}',
 '\\begin{array}{>{\\columncolor{cyan}}c>{\\columncolor{yellow}}c}a&b\\\\c&d\\end{array}',
 '\\begin{array}{c}\\columncolor{red}[1pt][2pt]a\\end{array}',
 '\\begin{array}{c}\\columncolor{red}[][2pt]a\\end{array}',
 '\\begin{array}{c}\\columncolor{red}[1pt]a\\end{array}',
 '\\begin{array}{ccc}\\cellcolor[rgb]{0,0.5,1}a&\\cellcolor[RGB]{12,128,255}b&\\cellcolor[gray]{0.5}c\\end{array}',
 '\\begin{pmatrix}\\rowcolor{red}a&b\\end{pmatrix}',
 '\\begin{smallmatrix}\\cellcolor{blue}a&b\\end{smallmatrix}',
 '\\begin{array}{|c|}\\hline\\cellcolor{red}a\\\\\\hline\\end{array}',
 '\\begin{array}{|c}\\cellcolor{red}a\\end{array}',
 '\\begin{array}{:c|}\\cellcolor{red}a\\end{array}',
 '\\begin{array}{c}\\cellcolor{}a\\end{array}',
 '\\begin{array}{cc}\\columncolor{}a&b\\\\\\columncolor{}c&d\\end{array}',
 '\\begin{array}{c}\\columncolor{red}\\columncolor{blue}a\\\\\\columncolor{blue}b\\end{array}',
 '\\begin{array}{c}\\cellcolor{red}[1pt]a\\end{array}',
 '\\begin{array}{c}\\rowcolor{red}[1pt]a\\end{array}',
 '\\begin{array}{c}\\cellcolor{transparent}a\\end{array}',
 '\\begin{array}{c}\\cellcolor{red}\\end{array}',
 '\\begin{array}{cc}\\rowcolor{red}\\begin{array}{c}\\cellcolor{blue}x\\end{array}&y\\end{array}'
];
for(const source of sources){const f=fixture();const tree=ok(f,source);captures.push({source,color:false,reversed:false,snapshot:snapshot(tree)})}
{
 const f=fixture(),map=f.jax.parseOptions.handlers.retrieve('colortbl');assert.equal(f.jax.parseOptions.itemFactory.create('array') instanceof ColorArrayItem,true);
 assert.equal(f.jax.parseOptions.packageData.has('color'),true);for(const [name,arg]of [['cellcolor','cell'],['rowcolor','row'],['columncolor','col']])assert.deepEqual(map.lookup(name).args,[arg]);
 for(const command of ['color','definecolor','textcolor','colorbox'])assert.equal(f.jax.parseOptions.handlers.get('macro').contains(command),false);
 assert.equal(map.lookup('cellcolor').func===map.lookup('rowcolor').func,true);assert.equal(map.lookup('rowcolor').func===map.lookup('columncolor').func,true);
}
const badSources=['\\cellcolor{red}','\\cellcolor','\\cellcolor[invalid]{red}','\\cellcolor[rgb]{2,0,0}',
 '\\begin{array}{c}x\\rowcolor{red}\\end{array}','\\begin{array}{cc}a&\\rowcolor{red}b\\end{array}',
 '\\begin{array}{c}a\\\\\\columncolor{red}b\\end{array}',
 '\\begin{array}{c}\\columncolor{red}a\\\\\\columncolor{blue}b\\end{array}',
 '\\begin{array}{c}a\\\\\\columncolor{}b\\end{array}',
 '\\begin{align}\\cellcolor{red}a&=b\\end{align}',
 '\\begin{array}{c}{\\cellcolor{red}a}\\end{array}',
 '\\begin{array}{c}\\mathbf{\\cellcolor{red}a}\\end{array}',
 '\\begin{array}{c}\\cellcolor[rgb]{1,2}a\\end{array}',
 '\\begin{array}{c}\\cellcolor{red}\\unknown\\end{array}'];
for(const source of badSources){const f=fixture(),r=f.compile(source);assert.notEqual(r.error,'',source);errors.push({source,id:r.error});ok(f,'\\begin{array}{c}\\cellcolor{blue}x\\end{array}')}
{
 const original=ColorConfiguration.configMethod[0];let calls=0;
 ColorConfiguration.configMethod[0]=(config,jax)=>{++calls;return original(config,jax)};
 try{const f=fixture();assert.equal(calls,1);const model=f.jax.parseOptions.packageData.get('color').model;model.defineColor('rgb','Stored','0,0.5,1');ColortblConfiguration.config(f.jax.configuration,f.jax);assert.equal(calls,1);assert.equal(f.jax.parseOptions.packageData.get('color').model===model,true);assert.equal(model.getColor('named','Stored'),'#007fff');const selected=fixture(true);assert.equal(calls,2);
 const missing=fixture(false,false);assert.equal(calls,2);assert.equal(missing.jax.parseOptions.packageData.has('color'),false);assert.equal(missing.jax.parseOptions.itemFactory.create('array') instanceof ColorArrayItem,false);ColortblConfiguration.config(missing.jax.configuration,missing.jax);assert.equal(calls,3);assert.equal(missing.jax.parseOptions.packageData.has('color'),true);}
 finally{ColorConfiguration.configMethod[0]=original}
}
const sequence=[];
{
 const f=fixture(true),definition='\\definecolor{Stored}{rgb}{0,0.5,1}';sequence.push({source:definition,snapshot:snapshot(ok(f,definition))});
 for(const source of ['\\begin{array}{c}\\cellcolor{Stored}a\\end{array}', '\\definecolor{Stored}{RGB}{255,12,0}', '\\begin{array}{c}\\columncolor{Stored}b\\end{array}'])sequence.push({source,snapshot:snapshot(ok(f,source))});
 f.jax.reset();assert.equal(f.jax.parseOptions.packageData.get('color').model.getColor('named','Stored'),'#ff0c00');
 const independent=fixture();assert.equal(independent.jax.parseOptions.packageData.get('color').model.getColor('named','Stored'),'Stored');
}
for(const reversed of [false,true]){const source='\\begin{array}{c}\\cellcolor{red}x\\end{array}',f=fixture(false,true,reversed);captures.push({source,color:false,reversed,snapshot:snapshot(ok(f,source))})}

function directArray(action){const f=fixture(),p=new TexParser('',{},f.jax.parseOptions),factory=f.jax.parseOptions.itemFactory,stop=p.stack.Pop(),array=factory.create('array');p.stack.Push(array);try{action(f,p,array)}finally{p.stack.Pop();p.stack.Push(stop);p.mml();assert.equal(f.jax.parseOptions.parsers.length,0)}}
function directToken(p,t){return p.create('token','mi',{},t)}
directArray((f,p,array)=>{
 array.color.col[0]='green';array.color.row='blue';array.color.cell='red';array.Push(directToken(p,'a'));array.EndEntry();assert.equal(array.row[0].attributes.get('mathbackground'),'red');assert.equal(array.color.cell,'');
 array.Push(directToken(p,'b'));array.EndEntry();assert.equal(array.row[1].attributes.get('mathbackground'),'blue');array.EndRow();assert.equal(array.color.row,'');assert.equal(array.color.col[0],'green');
 array.Push(directToken(p,'c'));array.EndEntry();assert.equal(array.row[0].attributes.get('mathbackground'),'green');array.EndRow();assert.equal(array.hasColor,true);direct.push({name:'precedence',snapshot:snapshot(array.createMml())});
});
directArray((f,p,array)=>{
 array.color.col[2]='yellow';array.color.col[3]='';assert.equal(array.color.col.length,4);assert.equal(Object.hasOwn(array.color.col,0),false);assert.equal(Object.hasOwn(array.color.col,3),true);
 for(const t of ['a','b','c']){array.Push(directToken(p,t));array.EndEntry()}array.EndRow();direct.push({name:'sparse',snapshot:snapshot(array.createMml())});
});
directArray((f,p,array)=>{
 const map=f.jax.parseOptions.handlers.retrieve('colortbl');array.Push(directToken(p,'x'));p.string='{blue}';p.i=0;
 assert.throws(()=>map.lookup('rowcolor').func(p,'\\rowcolor','row'),e=>e.id==='RowColorNotFirst');assert.equal(array.color.row,'blue');assert.equal(p.i,6);array.EndEntry();array.EndRow();direct.push({name:'partialRow',snapshot:snapshot(array.createMml())});
});
directArray((f,p,array)=>{
 const func=f.jax.parseOptions.handlers.retrieve('colortbl').lookup('columncolor').func;
 array.color.col[0]='';array.Push(directToken(p,'a'));array.EndEntry();array.EndRow();p.string='{}';p.i=0;func(p,'\\columncolor','col');assert.equal(array.color.col[0],'');assert.equal(array.hasColor,false);
 delete array.color.col[0];p.string='{}';p.i=0;assert.throws(()=>func(p,'\\columncolor','col'),e=>e.id==='ColumnColorNotTop');assert.equal(Object.hasOwn(array.color.col,0),false);
});
for(const [kind,input,rest]of [['col','{red}[left][right]z','z'],['col','{red}[][right]z','[right]z'],['cell','{red}[left]z','[left]z'],['row','{red}[left]z','[left]z']])directArray((f,p,array)=>{
 const map=f.jax.parseOptions.handlers.retrieve('colortbl');p.string=input;p.i=0;map.lookup('columncolor').func(p,'\\color',kind);assert.equal(p.string.slice(p.i),rest);
});
// Direct assembly exercises each Source wrapper independently of command parsing.
for(const mode of ['plain','fenced','style','menclose','fullframe','mixed'])directArray((f,p,array)=>{
 array.color.cell='red';array.Push(directToken(p,'x'));array.EndEntry();array.EndRow();
 if(mode==='fenced')array.setProperties({open:'(',close:')'});
 if(mode==='style')array.arraydef.scriptlevel=1;
 if(mode==='menclose')array.frame=[['left','solid']];
 if(mode==='fullframe')array.frame=[['top','solid'],['right','solid'],['bottom','solid'],['left','solid']];
 if(mode==='mixed')array.frame=[['left','solid'],['right','dashed']];
 direct.push({name:mode,snapshot:snapshot(array.createMml())});
});
const {utf16Field, byteField, writeTables} = require('./fixture-data.cjs');
writeTables(path.join(root, 'tests/math/fixtures/packages/colortbl'), {
  'trees': {columns: ["source_utf16", "color_boolean", "reversed_boolean", "snapshot_utf16"],
    rows: captures.map(c => [utf16Field(c.source), +!!c.color, +!!c.reversed, utf16Field(c.snapshot)])},
  'errors': {columns: ["source_utf16", "id_bytes"],
    rows: errors.map(c => [utf16Field(c.source), byteField(c.id)])},
  'sequence': {columns: ["source_utf16", "snapshot_utf16"],
    rows: sequence.map(c => [utf16Field(c.source), utf16Field(c.snapshot)])},
  'direct': {columns: ["name_utf16", "snapshot_utf16"],
    rows: direct.map(c => [utf16Field(c.name), utf16Field(c.snapshot)])},
});
console.log(JSON.stringify({cases,snapshots:captures.length,errors:errors.length,sequence:sequence.length,direct:direct.length,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
