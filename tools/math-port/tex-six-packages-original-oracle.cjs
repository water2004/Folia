'use strict';
// Actual pinned CJS observations for the six Tex packages owned by this port.
// Validate the complete locked profile before requiring any Source module.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),bundle=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax'),src=path.join(bundle,'src');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scopeBytes=fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'));
const scope=JSON.parse(scopeBytes.toString('utf8'));
assert.equal(sha(path.join(root,scope.profile.file)),scope.profile.sha256);
for(const lock of scope.files){const reference=path.join(root,'.reference/MathJax-src',lock.source);assert.equal(sha(reference),lock.source_sha256,lock.source);}
for(const lock of scope.files){const installed=path.join(bundle,'src',lock.source);if(fs.existsSync(installed))assert.equal(fs.readFileSync(installed,'utf8').replaceAll('\r\n','\n'),fs.readFileSync(path.join(root,'.reference/MathJax-src',lock.source),'utf8').replaceAll('\r\n','\n'),lock.source);}
const load=name=>require(path.join(src,'cjs',name));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const {MapHandler}=load('input/tex/MapHandler.js');
for(const name of ['action','bbox','braket','cancel','enclose','color'])load(`input/tex/${name}/${name[0].toUpperCase()+name.slice(1)}Configuration.js`);
function plain(v,seen=new WeakSet()){
 if(v===undefined)return{$:'undefined'};if(v===null||typeof v==='string'||typeof v==='boolean')return v;
 if(typeof v==='number')return Number.isNaN(v)?{$:'NaN'}:Object.is(v,-0)?{$:'-0'}:Number.isFinite(v)?v:{$:v<0?'-Infinity':'Infinity'};
 if(typeof v==='function')return{$:'function',name:v.name||'',length:v.length};if(typeof v!=='object')return String(v);
 if(seen.has(v))return{$:'cycle'};seen.add(v);if(Array.isArray(v))return Array.from({length:v.length},(_,i)=>Object.hasOwn(v,i)?plain(v[i],seen):{$:'hole'});
 if(v instanceof Map)return{$:'map',entries:[...v].map(([k,x])=>[plain(k,seen),plain(x,seen)])};
 const o={};for(const k of Reflect.ownKeys(v))o[typeof k==='symbol'?`symbol:${k.description||''}`:k]=plain(v[k],seen);return o;
}
function tree(n){return n==null?null:{kind:n.kind,attrs:plain(n.attributes?.getAllAttributes?.()??{}),properties:plain(n.getAllProperties?.()??{}),...(typeof n.getText==='function'?{text:n.getText()}:{}),children:(n.childNodes||[]).map(tree)};}
function capture(fn){try{return{result:fn(),error:null};}catch(e){return{result:null,error:{name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)}};}}
const inputs=[
 ['action-toggle',String.raw`\toggle{x}{y}\endtoggle`,['base','action']],
 ['action-toggle-over',String.raw`\toggle{a\over b}{c}\endtoggle`,['base','action']],
 ['action-mathtip',String.raw`\mathtip{x}{tip}`,['base','action']],
 ['action-texttip',String.raw`\texttip{x}{tip}`,['base','action']],
 ['bbox-empty',String.raw`\bbox{x}`,['base','bbox']],
 ['bbox-padding',String.raw`\bbox[2px]{x}`,['base','bbox']],
 ['bbox-color-style',String.raw`\bbox[red,border:1px solid blue]{x}`,['base','bbox']],
 ['bbox-invalid',String.raw`\bbox[nope]{x}`,['base','bbox']],
 ['bbox-duplicate',String.raw`\bbox[2px,3px]{x}`,['base','bbox']],
 ['braket-basic',String.raw`\bra{a}\ket{b}\braket{a|b}`,['base','braket']],
 ['braket-stretchy',String.raw`\Braket{a|b}`,['base','braket']],
 ['braket-over',String.raw`\Braket{a\over b|c}`,['base','braket']],
 ['braket-nested-set',String.raw`\Braket{\psi|A|\phi}+\Set{x|x>0}`,['base','braket']],
 ['cancel-family',String.raw`\cancel{x}+\bcancel{y}+\xcancel{z}`,['base','cancel']],
 ['cancel-to',String.raw`\cancelto{0}{x}`,['base','cancel']],
 ['cancel-options',String.raw`\cancel[color=red,data-padding=2px]{x}`,['base','cancel']],
 ['enclose-options',String.raw`\enclose{updiagonalstrike,circle}[mathcolor=red]{x}`,['base','enclose']],
 ['enclose-invalid-option',String.raw`\enclose{box}[unknown=1]{x}`,['base','enclose']],
 ['color-named',String.raw`\color{Red}{x}`,['base','color']],
 ['color-rgb',String.raw`\textcolor[rgb]{0.5,0,1}{x}`,['base','color']],
 ['color-rgb-invalid',String.raw`\textcolor[rgb]{2,0,1}{x}`,['base','color']],
 ['color-define',String.raw`\definecolor{custom}{RGB}{32,64,128}\color{custom}{x}`,['base','color']],
 ['colorbox',String.raw`\colorbox{red}{x}`,['base','color']],
 ['fcolorbox',String.raw`\fcolorbox{blue}{yellow}{x}`,['base','color']],
];
const observations=[];
for(const [name,input,packages] of inputs){let jax=null,result=null,error=null;try{jax=new TeX({packages});jax.setMmlFactory(new MmlFactory());const item=new AbstractMathItem(input,jax,true);result=tree(jax.compile(item,{}));}catch(e){error={name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)};}
 const maps={};for(const mapName of ['action-macros','bbox','Braket-macros','Braket-characters','cancel','enclose','color']){const map=MapHandler.getMap(mapName);if(map)maps[mapName]=plain(map.map);}
 observations.push({name,input,packages,result,error,packageData:jax?plain(jax.parseOptions.packageData):null,maps});}
const output={schema:1,scopeSha256:sha(path.join(root,'tools/math-port/profiles/scope.json')),cases:observations};
const dest=path.join(root,'build/math-port/tex-six-packages-original.json');fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest,JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify({cases:observations.length,sourceLocks:scope.files.length,output:path.relative(root,dest)}));
