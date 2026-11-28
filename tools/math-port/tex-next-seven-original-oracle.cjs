'use strict';
// Pinned CJS observations for the next seven TeX packages. Check the complete
// repository Source lock before requiring any MathJax module.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),bundle=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax'),src=path.join(bundle,'src');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
assert.equal(sha(path.join(root,scope.profile.file)),scope.profile.sha256);
for(const lock of scope.files){const reference=path.join(root,'.reference/MathJax-src',lock.source);assert.equal(sha(reference),lock.source_sha256,lock.source);}
for(const lock of scope.files){const installed=path.join(bundle,'src',lock.source);if(fs.existsSync(installed))assert.equal(fs.readFileSync(installed,'utf8').replaceAll('\r\n','\n'),fs.readFileSync(path.join(root,'.reference/MathJax-src',lock.source),'utf8').replaceAll('\r\n','\n'),lock.source);}
const load=name=>require(path.join(src,'cjs',name));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js'),{MapHandler}=load('input/tex/MapHandler.js');
for(const name of ['noundefined','verb','unicode','upgreek','gensymb','fontsizev3','extpfeil'])load(`input/tex/${name}/${name[0].toUpperCase()+name.slice(1)}Configuration.js`);
function plain(v,seen=new WeakSet()){
 if(v===undefined)return{$:'undefined'};if(v===null||typeof v==='string'||typeof v==='boolean')return v;
 if(typeof v==='number')return Number.isNaN(v)?{$:'NaN'}:Object.is(v,-0)?{$:'-0'}:Number.isFinite(v)?v:{$:v<0?'-Infinity':'Infinity'};
 if(typeof v==='function')return{$:'function',name:v.name||'',length:v.length};if(typeof v!=='object')return String(v);
 if(seen.has(v))return{$:'cycle'};seen.add(v);if(Array.isArray(v))return Array.from({length:v.length},(_,i)=>Object.hasOwn(v,i)?plain(v[i],seen):{$:'hole'});
 if(v instanceof Map)return{$:'map',entries:[...v].map(([k,x])=>[plain(k,seen),plain(x,seen)])};
 const out={};for(const key of Reflect.ownKeys(v))out[typeof key==='symbol'?`symbol:${key.description||''}`:key]=plain(v[key],seen);return out;
}
function tree(node){return node==null?null:{kind:node.kind,attrs:plain(node.attributes?.getAllAttributes?.()??{}),properties:plain(node.getAllProperties?.()??{}),...(typeof node.getText==='function'?{text:node.getText()}:{}),children:(node.childNodes||[]).map(tree)};}
const inputs=[
 ['undefined-macro',String.raw`x+\unknownControl+y`,['base','noundefined']],
 ['verb-delimited',String.raw`\verb|a b + c|`,['base','verb']],
 ['verb-missing-delimiter',String.raw`\verb`,['base','verb']],
 ['verb-unclosed',String.raw`\verb|abc`,['base','verb']],
 ['unicode-decimal',String.raw`\unicode{65}`,['base','unicode']],
 ['unicode-hex',String.raw`\unicode{0x1F600}`,['base','unicode']],
 ['unicode-height-font',String.raw`\unicode[1.2,0.3][Example Font]{65}`,['base','unicode']],
 ['unicode-bad-font',String.raw`\unicode[Bad;Font]{65}`,['base','unicode']],
 ['unicode-bad-value',String.raw`\unicode{xyz}`,['base','unicode']],
 ['raw-unicode',String.raw`\U{1F600}`,['base','unicode']],
 ['raw-unicode-invalid',String.raw`\U{110000}`,['base','unicode']],
 ['char-decimal',String.raw`\char65`,['base','unicode']],
 ['char-hex',String.raw`\char"41`,['base','unicode']],
 ['char-octal',String.raw`\char'101`,['base','unicode']],
 ['char-control-sequence',String.raw`\char`+"`\\alpha",['base','unicode']],
 ['upgreek',String.raw`\upalpha+\Uppi+\Upgamma`,['base','upgreek']],
 ['gensymb',String.raw`\ohm+\degree+\celsius+\perthousand+\micro`,['base','gensymb']],
 ['fontsize-v3',String.raw`\tiny x+\Large y+\Huge z`,['base','fontsizev3']],
 ['extpfeil-basic',String.raw`\xmapsto[a]{b}`,['base','newcommand','extpfeil']],
 ['extpfeil-new',String.raw`\Newextarrow{\foo}{7,8}{0x41}\foo{b}`,['base','newcommand','extpfeil']],
 ['extpfeil-invalid-name',String.raw`\Newextarrow{foo}{7,8}{65}`,['base','newcommand','extpfeil']],
 ['extpfeil-invalid-space',String.raw`\Newextarrow{\foo}{7x8}{65}`,['base','newcommand','extpfeil']],
 ['extpfeil-invalid-codepoint',String.raw`\Newextarrow{\foo}{7,8}{bad}`,['base','newcommand','extpfeil']],
];
const records=[];
for(const [name,input,packages] of inputs){let jax=null,result=null,error=null;try{jax=new TeX({packages});jax.setMmlFactory(new MmlFactory());result=tree(jax.compile(new AbstractMathItem(input,jax,true),{}));}catch(e){error={name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)};}
 const mapNames=['fontsizev3-macros','unicode','upgreek','gensymb-symbols','verb','extpfeil'];const maps={};for(const key of mapNames){const map=MapHandler.getMap(key);if(map)maps[key]=plain(map.map);}
 records.push({name,input,packages,result,error,packageData:jax?plain(jax.parseOptions.packageData):null,maps});}
const output={schema:1,scopeSha256:sha(path.join(root,'tools/math-port/profiles/scope.json')),cases:records};const dest=path.join(root,'build/math-port/tex-next-seven-original.json');fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,JSON.stringify(output,null,2)+'\n');console.log(JSON.stringify({cases:records.length,sourceLocks:scope.files.length,output:path.relative(root,dest)}));
