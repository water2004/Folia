'use strict';
// Pinned Source observations for colortbl, tagformat, and setoptions.
// Validate the complete Source profile before loading any MathJax module.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),bundle=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax'),src=path.join(bundle,'src');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
assert.equal(sha(path.join(root,scope.profile.file)),scope.profile.sha256);
for(const lock of scope.files)assert.equal(sha(path.join(root,'.reference/MathJax-src',lock.source)),lock.source_sha256,lock.source);
for(const lock of scope.files){const installed=path.join(bundle,'src',lock.source);if(fs.existsSync(installed))assert.equal(fs.readFileSync(installed,'utf8').replaceAll('\r\n','\n'),fs.readFileSync(path.join(root,'.reference/MathJax-src',lock.source),'utf8').replaceAll('\r\n','\n'),lock.source);}
const load=name=>require(path.join(src,'cjs',name));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js'),{MapHandler}=load('input/tex/MapHandler.js');
for(const name of ['color','ams','colortbl','tagformat','setoptions'])load(`input/tex/${name}/${name[0].toUpperCase()+name.slice(1)}Configuration.js`);
function plain(v,seen=new WeakSet()){
 if(v===undefined)return{$:'undefined'};if(v===null||typeof v==='string'||typeof v==='boolean')return v;
 if(typeof v==='number')return Number.isNaN(v)?{$:'NaN'}:Object.is(v,-0)?{$:'-0'}:Number.isFinite(v)?v:{$:v<0?'-Infinity':'Infinity'};
 if(typeof v==='function')return{$:'function',name:v.name||'',length:v.length};if(typeof v!=='object')return String(v);
 if(seen.has(v))return{$:'cycle'};seen.add(v);if(Array.isArray(v))return Array.from({length:v.length},(_,i)=>Object.hasOwn(v,i)?plain(v[i],seen):{$:'hole'});
 if(v instanceof Map)return{$:'map',entries:[...v].map(([k,x])=>[plain(k,seen),plain(x,seen)])};
 const out={};for(const k of Reflect.ownKeys(v))out[typeof k==='symbol'?`symbol:${k.description||''}`:k]=plain(v[k],seen);return out;
}
function tree(n){return n==null?null:{kind:n.kind,attrs:plain(n.attributes?.getAllAttributes?.()??{}),properties:plain(n.getAllProperties?.()??{}),...(typeof n.getText==='function'?{text:n.getText()}:{}),children:(n.childNodes||[]).map(tree)};}
const inputs=[
 ['colortbl-cell',String.raw`\begin{array}{cc}\cellcolor{red}a&b\\c&d\end{array}`,['base','color','colortbl'],{}],
 ['colortbl-row',String.raw`\begin{array}{c}\rowcolor{blue}a\\b\end{array}`,['base','color','colortbl'],{}],
 ['colortbl-column',String.raw`\begin{array}{c}\columncolor{green}c\end{array}`,['base','color','colortbl'],{}],
 ['colortbl-outside',String.raw`\cellcolor{red}x`,['base','color','colortbl'],{}],
 ['colortbl-row-late',String.raw`\begin{array}{c}a\rowcolor{red}b\end{array}`,['base','color','colortbl'],{}],
 ['tagformat-base',String.raw`\begin{equation}x\label{eq:x}\end{equation}`,['base','ams','tagformat'],{}],
 ['tagformat-ref',String.raw`\begin{equation}x\label{eq:x}\end{equation}\ref{eq:x}`,['base','ams','tagformat'],{}],
 ['tagformat-custom',String.raw`\begin{equation}x\label{eq:x}\end{equation}\ref{eq:x}`,['base','ams','tagformat'],{tagformat:{number:n=>`N${n}`,tag:t=>['[',t,']'],ref:t=>['<',t,'>'],id:id=>`custom:${id}`,url:(id,base)=>`${base}?id=${encodeURIComponent(id)}`}}],
 ['setoptions-tex',String.raw`\setOptions{displaystyle=true}x`,['base','setoptions'],{}],
 ['setoptions-package',String.raw`\setOptions[ams]{numbered=false}\begin{equation}x\end{equation}`,['base','ams','setoptions'],{}],
 ['setoptions-invalid-package',String.raw`\setOptions[no-such-package]{x=1}x`,['base','setoptions'],{}],
 ['setoptions-invalid-key',String.raw`\setOptions{unknownKey=1}x`,['base','setoptions'],{}],
];
const cases=[];
for(const [name,input,packages,settings] of inputs){let jax=null,result=null,error=null;try{jax=new TeX({...settings,packages});jax.setMmlFactory(new MmlFactory());const item=new AbstractMathItem(input,jax,true);result=tree(jax.compile(item,{}));}catch(e){error={name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)};}
 const maps={};for(const mapName of ['colortbl','setoptions']){const map=MapHandler.getMap(mapName);if(map)maps[mapName]=plain(map.map);}
 cases.push({name,input,packages,settings:plain(settings),result,error,packageData:jax?plain(jax.parseOptions.packageData):null,tagClass:jax?.parseOptions.tags?.constructor?.name??null,tagOptions:jax?plain(jax.parseOptions.options.tagformat):null,maps});}
const output={schema:1,scopeSha256:sha(path.join(root,'tools/math-port/profiles/scope.json')),cases};
const dest=path.join(root,'build/math-port/tex-next-packages-original.json');fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify({cases:cases.length,sourceLocks:scope.files.length,output:path.relative(root,dest)}));
