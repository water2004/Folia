'use strict';
// Pinned Source oracle for BBM, BBoldx and Dsfont. Validate the locked source
// profile before loading package modules or observing any Source behavior.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),bundle=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax'),src=path.join(bundle,'src');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
assert.equal(sha(path.join(root,scope.profile.file)),scope.profile.sha256);
for(const lock of scope.files){const f=path.join(root,'.reference/MathJax-src',lock.source);assert.equal(sha(f),lock.source_sha256,lock.source);}
for(const lock of scope.files){const f=path.join(bundle,'src',lock.source);if(fs.existsSync(f))assert.equal(fs.readFileSync(f,'utf8').replaceAll('\r\n','\n'),fs.readFileSync(path.join(root,'.reference/MathJax-src',lock.source),'utf8').replaceAll('\r\n','\n'),lock.source);}
const load=name=>require(path.join(src,'cjs',name));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js'),{MapHandler}=load('input/tex/MapHandler.js');
const BaseMethods=load('input/tex/base/BaseMethods.js').default;
for(const name of ['bbm','bboldx','dsfont'])load(`input/tex/${name}/${name[0].toUpperCase()+name.slice(1)}Configuration.js`);
load('input/tex/textmacros/TextMacrosConfiguration.js');
function plain(v,seen=new WeakSet()){
 if(v===undefined)return{$:'undefined'};if(v===null||typeof v==='string'||typeof v==='boolean')return v;
 if(typeof v==='number')return Number.isNaN(v)?{$:'NaN'}:Object.is(v,-0)?{$:'-0'}:Number.isFinite(v)?v:{$:v<0?'-Infinity':'Infinity'};
 if(typeof v==='function')return{$:'function',name:v.name||'',length:v.length};if(typeof v!=='object')return String(v);
 if(seen.has(v))return{$:'cycle'};seen.add(v);if(Array.isArray(v))return Array.from({length:v.length},(_,i)=>Object.hasOwn(v,i)?plain(v[i],seen):{$:'hole'});
 if(v instanceof Map)return{$:'map',entries:[...v].map(([k,x])=>[plain(k,seen),plain(x,seen)])};const o={};for(const k of Reflect.ownKeys(v))o[typeof k==='symbol'?`symbol:${k.description||''}`:k]=plain(v[k],seen);return o;
}
function tree(n){return n==null?null:{kind:n.kind,attrs:plain(n.attributes?.getAllAttributes?.()??{}),properties:plain(n.getAllProperties?.()??{}),...(typeof n.getText==='function'?{text:n.getText()}:{}),children:(n.childNodes||[]).map(tree)};}
const cases=[
 ['bbm-default',String.raw`\mathbbm{ABC} + \mathbbmss{xyz} + \mathbbmtt{12}`,['base','bbm']],
 ['bbm-bold-switch',String.raw`\mathbbm{A}\mathversion{bold}\mathbbm{B}\mathversion{regular}\mathbbm{C}`,['base','bbm']],
 ['bbm-missing-argument',String.raw`\mathbbm`,['base','bbm']],
 ['bboldx-normal',String.raw`\mathbb{R}+\bbGamma+\bbalpha+\bbLparen`,['base','bboldx']],
 ['bboldx-bold-option',String.raw`\mathbb{R}+\mathbfbb{C}+\bfbbGamma`,['base','bboldx'],{bboldx:{bfbb:true}}],
 ['bboldx-light-option',String.raw`\mathbb{R}+\bbGamma`,['base','bboldx'],{bboldx:{light:true}}],
 ['bboldx-dotless',String.raw`\imathbb+\jmathbb+\imathbfbb+\jmathbfbb`,['base','bboldx']],
 ['bboldx-text',String.raw`\text{\textbb{A} \textbfbb{B} \txtbbalpha \txtbfbbalpha}`,['base','textmacros','bboldx']],
 ['bboldx-missing-argument',String.raw`\mathbb`,['base','bboldx']],
 ['dsfont-default',String.raw`\mathds{1}+\mathds{A}`,['base','dsfont']],
 ['dsfont-sans-option',String.raw`\mathds{1}+\mathds{A}`,['base','dsfont'],{dsfont:{sans:true}}],
 ['dsfont-missing-argument',String.raw`\mathds`,['base','dsfont']],
 ['bbm-bold-option',String.raw`\mathbbm{A}+\mathbbmss{B}`,['base','bbm'],{bbm:{bold:true}}],
 ['bboldx-option-precedence',String.raw`\mathbb{A}+\bbGamma+\bfbbGamma`,['base','bboldx'],{bboldx:{bfbb:true,light:true}}],
 ['bboldx-text-dotless-aliases',String.raw`\text{\itextbb\jtextbb\itextbfbb\jtextbfbb}`,['base','textmacros','bboldx']],
 ['bboldx-text-light-option',String.raw`\text{\textbb{A} \textbfbb{B} \txtbbalpha \txtbfbbalpha}`,['base','textmacros','bboldx'],{bboldx:{light:true}}],
];
const maps=['bbm','bboldx-mathchar0miNormal','bboldx-delimiterNormal','bboldx-mathchar0miBold','bboldx-delimiterBold','bboldx','text-bboldx-mathchar0miNormal','text-bboldx-delimiterNormal','text-bboldx-mathchar0miBold','text-bboldx-delimiterBold','text-bboldx','dsfont'];
const lookup=(name,key)=>MapHandler.getMap(name)?.lookup(key);
const macro=(name,key)=>lookup(name,key)?.func;
const aliases={
 bbmChooseFontShared:macro('bbm','mathbbm')===macro('bbm','mathbbmss'),
 bbmBaseMathFontShared:macro('bbm','mathbbmtt')===BaseMethods.MathFont,
 bboldxNormalCharacterShared:MapHandler.getMap('bboldx-mathchar0miNormal')?.parser===MapHandler.getMap('text-bboldx-mathchar0miNormal')?.parser,
 bboldxNormalDelimiterShared:MapHandler.getMap('bboldx-delimiterNormal')?.parser===MapHandler.getMap('text-bboldx-delimiterNormal')?.parser,
 bboldxBoldCharacterShared:MapHandler.getMap('bboldx-mathchar0miBold')?.parser===MapHandler.getMap('text-bboldx-mathchar0miBold')?.parser,
 bboldxBoldDelimiterShared:MapHandler.getMap('bboldx-delimiterBold')?.parser===MapHandler.getMap('text-bboldx-delimiterBold')?.parser,
 bboldxChooseFontShared:macro('bboldx','mathbb')===macro('bboldx','mathbfbb'),
 bboldxTextFontShared:macro('text-bboldx','textbb')===macro('text-bboldx','textbfbb'),
 bboldxMacroAlias:macro('bboldx','imathbb')===BaseMethods.Macro,
 dsfontChooseFont:!!macro('dsfont','mathds')
};
const records=[];
for(const [name,input,packages,options={}] of cases){let jax=null,result=null,error=null;try{jax=new TeX({...options,packages});jax.setMmlFactory(new MmlFactory());result=tree(jax.compile(new AbstractMathItem(input,jax,true),{}));}catch(e){error={name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)};}
 const mapValues={};for(const key of maps){const map=MapHandler.getMap(key);if(map)mapValues[key]={name:plain(map.name),map:plain(map.map),functionName:map.parser?.name||'',functionLength:map.parser?.length};}
 records.push({name,input,packages,options,result,error,packageData:jax?plain(jax.parseOptions.packageData):null,maps:mapValues,aliases});}
const output={schema:1,scopeSha256:sha(path.join(root,'tools/math-port/profiles/scope.json')),cases:records};const dest=path.join(root,'build/math-port/tex-bboldx-font-packages-original.json');fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,JSON.stringify(output,null,2)+'\n');console.log(JSON.stringify({cases:records.length,sourceLocks:scope.files.length,output:path.relative(root,dest)}));
