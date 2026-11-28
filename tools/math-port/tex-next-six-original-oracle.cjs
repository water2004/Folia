'use strict';
// Pinned Source observations for the next six TeX package ports.
// Verify the complete source lock before loading any package modules.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),bundle=path.join(root,'.reference/MathJax-src'),src=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
assert.equal(sha(path.join(root,scope.profile.file)),scope.profile.sha256);
for(const lock of scope.files)assert.equal(sha(path.join(bundle,lock.source)),lock.source_sha256,lock.source);
const load=name=>require(path.join(src,'cjs',name));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js'),{MapHandler}=load('input/tex/MapHandler.js');
for(const [pkg,file] of [['amscd','AmsCd'],['cases','Cases'],['centernot','Centernot'],['html','Html'],['textcomp','Textcomp'],['units','Units']])load(`input/tex/${pkg}/${file}Configuration.js`);
load('input/tex/empheq/EmpheqConfiguration.js');
load('input/tex/textmacros/TextMacrosConfiguration.js');
function plain(v,seen=new WeakSet()){
 if(v===undefined)return{$:'undefined'};if(v===null||typeof v==='string'||typeof v==='boolean')return v;
 if(typeof v==='number')return Number.isNaN(v)?{$:'NaN'}:Object.is(v,-0)?{$:'-0'}:Number.isFinite(v)?v:{$:v<0?'-Infinity':'Infinity'};
 if(typeof v==='function')return{$:'function',name:v.name||'',length:v.length};if(typeof v!=='object')return String(v);
 if(seen.has(v))return{$:'cycle'};seen.add(v);if(Array.isArray(v))return Array.from({length:v.length},(_,i)=>Object.hasOwn(v,i)?plain(v[i],seen):{$:'hole'});
 if(v instanceof Map)return{$:'map',entries:[...v].map(([k,x])=>[plain(k,seen),plain(x,seen)])};
 const out={};for(const key of Reflect.ownKeys(v))out[typeof key==='symbol'?`symbol:${key.description||''}`:key]=plain(v[key],seen);return out;
}
function tree(n){return n==null?null:{kind:n.kind,attrs:plain(n.attributes?.getAllAttributes?.()??{}),properties:plain(n.getAllProperties?.()??{}),...(typeof n.getText==='function'?{text:n.getText()}:{}),children:(n.childNodes||[]).map(tree)};}
const inputs=[
 ['amscd-horizontal',String.raw`\begin{CD}A @>>> B\\ @VfVV @VVgV\\ C @= D\end{CD}`,['base','ams','amscd']],
 ['amscd-labelled',String.raw`\begin{CD}A @>f>> B\\ C @VgVV D\end{CD}`,['base','ams','amscd']],
 ['amscd-options',String.raw`\begin{CD}A @>>> B\\ C @VVV D\end{CD}`,['base','ams','amscd']],
 ['cases-num',String.raw`\begin{numcases}{x=}a&first\\b&second\end{numcases}`,['base','ams','empheq','cases']],
 ['cases-subnum',String.raw`\begin{subnumcases}{x=}a&first\\b&second\end{subnumcases}`,['base','ams','empheq','cases']],
 ['cases-extra-tab',String.raw`\begin{numcases}{x=}a&first&extra\end{numcases}`,['base','ams','empheq','cases']],
 ['centernot-relation',String.raw`a\centernot\subseteq b`,['base','centernot']],
 ['centernot-command',String.raw`\centerOver{A}{B}`,['base','centernot']],
 ['centernot-nested',String.raw`x+\centernot\in y`,['base','centernot']],
 ['html-data',String.raw`\data{foo=bar,lang=en}{x}`,['base','html']],
 ['html-href-class',String.raw`\href{https://example.test}{\class{one two}{x}}`,['base','html']],
 ['html-style-id',String.raw`\style{color:red}{\cssId{eq1}{x}}`,['base','html']],
 ['html-invalid-attr',String.raw`\data{bad\ name=x}{x}`,['base','html']],
 ['textcomp-basic',String.raw`\text{\textcopyright\textregistered\texttrademark}`,['base','textmacros','textcomp']],
 ['textcomp-symbols',String.raw`\text{\textdegree\textmu\texteuro\textquotesingle}`,['base','textmacros','textcomp']],
 ['textcomp-outside-text',String.raw`\textcopyright+\textdegree`,['base','textcomp']],
 ['units-basic',String.raw`\units{m}+\units[5]{kg}`,['base','units']],
 ['units-fraction',String.raw`\unitfrac{m}{s}+\nicefrac{a}{b}`,['base','units']],
 ['units-options-default',String.raw`\units[10]{cm}`,['base','units']],
 ['units-malformed',String.raw`\unitfrac{m}`,['base','units']],
];
const records=[];
for(const [name,input,packages] of inputs){let jax=null,result=null,error=null;try{jax=new TeX({packages});jax.setMmlFactory(new MmlFactory());result=tree(jax.compile(new AbstractMathItem(input,jax,true),{}));}catch(e){error={name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)};}
 const maps={};for(const key of ['amscd_special','amscd_macros','amscd_environment','empheq-env','empheq-macros','cases-env','cases-macros','centernot','html_macros','textcomp-macros','units']){const map=MapHandler.getMap(key);if(map)maps[key]=plain(map.map);}
 records.push({name,input,packages,result,error,packageData:jax?plain(jax.parseOptions.packageData):null,maps});}
const output={schema:1,scopeSha256:sha(path.join(root,'tools/math-port/profiles/scope.json')),cases:records};
const dest=path.join(root,'build/math-port/tex-next-six-original.json');fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify({cases:records.length,sourceLocks:scope.files.length,output:path.relative(root,dest)}));
