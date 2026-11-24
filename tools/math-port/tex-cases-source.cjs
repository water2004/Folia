'use strict';
// Real pinned CasesConfiguration, native-independent TeX/MML expectations.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [file,sha]of [
 ['.reference/MathJax-src/ts/input/tex/cases/CasesConfiguration.ts','6bb2eab1a09eb5773d28dca0c4dcb2b7fc12048807952e6ab32ae6ea5658c484'],
 ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/cases/CasesConfiguration.js','8490555aaf017a0047b6f8edef9167310261b766d921b373e58fbf4075d1b61c']
])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,file))).digest('hex'),sha,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=f=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',f));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const {CasesConfiguration,CasesMethods,CasesTags,CasesBeginItem}=load('input/tex/cases/CasesConfiguration.js');
const TexParser=load('input/tex/TexParser.js').default;
let cases=0;function fixture(options={},tagformat=false){let error='';const jax=new TeX({packages:['base','ams','cases',...(tagformat?['tagformat']:[])],tags:'cases',maxMacros:1000,...options,formatError(j,e){error=e.id;return j.formatError(e)}});jax.setMmlFactory(new MmlFactory());return {jax,compile(source){++cases;error='';const tree=jax.compile(new AbstractMathItem(source,jax,true),reference.document);return {tree,error}}};}
function nodes(root,kind){const result=[];root.walkTree(n=>{if(n.kind===kind)result.push(n)});return result;}
function text(root){let result='';root.walkTree(n=>{if(n.isToken)result+=n.getText()});return result;}
function ok(f,source){const r=f.compile(source);assert.equal(r.error,'',source);assert.equal(f.jax.parseOptions.parsers.length,0);return r.tree;}
function snapshot(root){let s='';const add=v=>{v=String(v);s+=v.length+':'+v};function node(n){if(!n){add('null');return}add(n.kind);const attrs=Object.entries(n.attributes.getAllAttributes()).sort(([a],[b])=>a<b?-1:a>b?1:0);add(attrs.length);for(const [key,v]of attrs){add(key);add(v===null||v===undefined?'null':typeof v);add(v===null||v===undefined?'':String(v))}add(n.isToken?n.getText():'');const children=n.isToken?[]:n.childNodes;add(children.length);for(const c of children)node(c)}node(root);return s;}
function tags(root){return nodes(root,'mlabeledtr').map(row=>text(row.childNodes[0]));}
assert.equal(CasesConfiguration.priority,5);assert.deepEqual(CasesConfiguration.handler.environment,['cases-env']);assert.deepEqual(CasesConfiguration.handler.character,['cases-macros']);assert.deepEqual(Object.keys(CasesMethods),['NumCases','Entry','environment']);
const captures=[],errors=[],formats=[];
for(const mode of ['cases','ams','none','all'])for(const env of ['numcases','subnumcases'])for(const tagSide of ['left','right']){
 const source='\\begin{'+env+'}{f(x)=}a & first\\\\b & second\\end{'+env+'}',f=fixture({tags:mode,tagSide});captures.push({source,mode,tagSide,snapshot:snapshot(ok(f,source)),tagformat:false});
}
const extras=[
 '\\begin{numcases}{f=}\\end{numcases}',
 '\\begin{subnumcases}{}a\\end{subnumcases}',
 '\\begin{numcases}{f=}a & before $x+1$ after\\\\b & \\(y\\) end\\end{numcases}',
 '\\begin{subnumcases}{f=}a & {\\{braces\\} \\& \\$ \\&amp; Ω😀}\\end{subnumcases}',
 '\\begin{numcases}{f=}a & \u00a0\u2003 first  \\end{numcases}',
 '\\begin{numcases}{f=}a & {nested {braces}} end\\end{numcases}',
 '\\begin{numcases}{f=}a & one\\cr b & two\\end{numcases}',
 '\\begin{numcases}{f=}a & one\\label{a}\\\\b & two\\label{b}\\end{numcases}',
 '\\begin{subnumcases}{f=}a & one\\tag{X}\\\\b & two\\notag\\\\c & three\\end{subnumcases}',
 '\\begin{numcases}{f\\label{left}=}a & one\\label{row}\\\\b & two\\end{numcases}',
 '\\begin{array}{cc}a&b\\\\c&d\\end{array}',
 '\\begin{align}a&=b\\\\c&=d\\end{align}',
 '\\begin{cases}a&b\\\\c&d\\end{cases}',
 '\\begin{numcases}{f=}a & tail \\end{numcases}',
 '\\begin{subnumcases}{f=}a & {literal & inside}\\end{subnumcases}'
];
for(const source of extras){const f=fixture();const tree=ok(f,source);captures.push({source,mode:'cases',tagSide:'right',snapshot:snapshot(tree),tagformat:false})}
{
 const source='\\begin{subnumcases}{f=}a & first\\\\b & second\\end{subnumcases}',f=fixture({tagformat:{number:n=>'N'+n}},true),tree=ok(f,source);
 assert.deepEqual(nodes(tree,'mtable')[0].childNodes.filter(n=>n.kind==='mlabeledtr').map(n=>text(n.childNodes[0])),['(N1)','(N1)']);captures.push({source,mode:'cases',tagSide:'right',snapshot:snapshot(tree),tagformat:true});
}
{
 const f=fixture(),p=new TexParser('x',{},f.jax.parseOptions),tag=new CasesTags(f.jax.parseOptions);tag.configuration=f.jax.parseOptions;
 for(const sub of [null,0,1,26,27,-97,65440,65536]){const expected=sub===null?tag.formatNumber(7):tag.formatNumber(7,sub);formats.push({sub,expected})}
 p.mml();
}
const extraErrors=['\\begin{numcases*}{f=}a & text\\end{numcases*}','\\begin{subnumcases*}{f=}a & text\\end{subnumcases*}',
 '\\begin{numcases}{f=}a & one & two\\end{numcases}', '\\begin{numcases}{f=}a & &amp;\\end{numcases}', '\\begin{numcases}{f=}a & \\& escaped\\end{numcases}',
 '\\begin{numcases}', '\\begin{numcases}{f=}a & text', '\\begin{numcases}{f=}a & text}\\end{numcases}',
 '\\begin{numcases}{f=}a & {text\\end{numcases}', '\\begin{numcases}{\\unknown}a & text\\end{numcases}',
 '\\begin{numcases}{f=}\\unknown & text\\end{numcases}', '\\begin{numcases}{f=}a & text\\end{align}'];
for(const source of extraErrors){const f=fixture(),r=f.compile(source);assert.notEqual(r.error,'',source);const entry={source,id:r.error};errors.push(entry);ok(f,'x');const recovered=ok(f,'\\begin{numcases}{g=}z & recovered\\label{after}\\end{numcases}');entry.recoveryTag=f.jax.parseOptions.tags.allLabels.after.tag;assert.notEqual(entry.recoveryTag,'');}



const sequence=[];
{
 const f=fixture();
 for(const source of ['\\begin{numcases}{f\\label{left}=}a & one\\label{row}\\\\b & two\\end{numcases}',
  '\\ref{left}+\\eqref{row}', '\\begin{equation}z\\label{next}\\end{equation}',
  '\\begin{subnumcases}{g=}a & first\\label{suba}\\\\b & second\\label{subb}\\end{subnumcases}', '\\ref{suba}+\\eqref{subb}'])
  sequence.push({source,snapshot:snapshot(ok(f,source))});
 assert.deepEqual(Object.fromEntries(Object.entries(f.jax.parseOptions.tags.allLabels).map(([k,v])=>[k,v.tag])),{left:'2',row:'1',next:'3',suba:'4a',subb:'4b'});
}
{
 const f=fixture(),p=new TexParser('x',{},f.jax.parseOptions),factory=f.jax.parseOptions.itemFactory;
 const env=f.jax.parseOptions.handlers.retrieve('cases-env'),map=f.jax.parseOptions.handlers.retrieve('cases-macros');
 assert.equal(env.parser===CasesMethods.environment,true);assert.equal(map.lookup('&').func===CasesMethods.Entry,true);
 for(const name of ['numcases','subnumcases']){const m=env.lookup(name);assert.equal(m.func===CasesMethods.NumCases,true);assert.deepEqual(m.args,['cases'])}
 let forwarded;CasesMethods.environment(p,'probe',(parser,item,...args)=>{forwarded=args;assert.equal(item instanceof CasesBeginItem,true);assert.equal(item.getName(),'probe');assert.equal(item.getProperty('end'),true);return null},['cases',2,false]);assert.deepEqual(forwarded,['cases',2,false]);
 const begin=factory.create('cases-begin').setProperties({name:'numcases',end:true}),end=factory.create('end').setProperty('name','numcases');
 const first=begin.checkItem(end);assert.deepEqual(first,[[],true]);assert.equal(begin.getProperty('end'),false);
 const second=begin.checkItem(end);assert.equal(second[0].length,1);assert.equal(second[0][0].kind,'mml');assert.equal(second[1],true);
 const other=factory.create('end').setProperty('name','other');assert.throws(()=>begin.checkItem(other),e=>e.id==='EnvBadEnd');assert.throws(()=>begin.checkItem(factory.create('stop')),e=>e.id==='EnvMissingEnd');
 const Stack=load('input/tex/Stack.js').default,stack=new Stack(factory,{},true),shared=factory.create('cases-begin').setProperties({name:'numcases',end:true});
 stack.Push(shared);stack.Push(shared);assert.equal(stack.stack.length,3);assert.equal(stack.Top()===stack.Top(2),true);
 stack.Push(end);assert.equal(stack.stack.length,2);assert.equal(stack.Top()===shared,true);assert.equal(shared.getProperty('end'),false);
 stack.Push(end);assert.equal(stack.stack.length,1);assert.equal(stack.Top().kind,'start');p.mml();
}
{
 const f=fixture(),tag=f.jax.parseOptions.tags;
 tag.start('subnumcases',true,true);tag.autoTag();assert.equal(tag.currentTag.tag,'1a');tag.clearTag();tag.autoTag();assert.equal(tag.currentTag.tag,'1b');tag.end();
 tag.start('numcases-left',true,true);tag.autoTag();assert.equal(tag.currentTag.tag,'1');tag.end();
 tag.start('subnumcases',true,true);tag.autoTag();assert.equal(tag.currentTag.tag,'2a');tag.end();
}
const {utf16Field, byteField, writeTables} = require('./fixture-data.cjs');
writeTables(path.join(root, 'tests/math/fixtures/packages/cases'), {
  'trees': {columns: ["source_utf16", "mode_utf16", "tagSide_utf16", "snapshot_utf16", "tagformat_boolean"],
    rows: captures.map(c => [utf16Field(c.source), utf16Field(c.mode), utf16Field(c.tagSide), utf16Field(c.snapshot), +!!c.tagformat])},
  'errors': {columns: ["source_utf16", "id_bytes", "recoveryTag_utf16"],
    rows: errors.map(c => [utf16Field(c.source), byteField(c.id), utf16Field(c.recoveryTag)])},
  'formats': {columns: ["sub_optional_integer", "expected_utf16"],
    rows: formats.map(c => [(c.sub === null ? "null" : c.sub), utf16Field(c.expected)])},
  'sequence': {columns: ["source_utf16", "snapshot_utf16"],
    rows: sequence.map(c => [utf16Field(c.source), utf16Field(c.snapshot)])},
});
console.log(JSON.stringify({cases,snapshots:captures.length,errors:errors.length,formats:formats.length,sequence:sequence.length,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
