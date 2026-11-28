'use strict';
// Whole-record Source observations for the locked TagFormat and SetOptions files.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../../..'),bundle=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax'),src=path.join(bundle,'src');
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const lock=JSON.parse(fs.readFileSync(path.join(__dirname,'source-lock.json'),'utf8'));
const scopePath=path.join(root,'tools/math-port/profiles/scope.json');
assert.equal(digest(scopePath),lock.scopeSha256,'scope.json changed; refresh the reviewed Source lock first');
for(const source of lock.sources){
 const file=path.join(root,'.reference/MathJax-src',source.file);
 assert.equal(digest(file),source.sha256,`${source.file} differs from the locked MathJax source`);
 if(source.cjsFile)assert.equal(digest(path.join(root,source.cjsFile)),source.cjsSha256,`${source.cjsFile} differs from its executable Source lock`);
}
const load=name=>require(path.join(src,'cjs',name));
const {TeX}=load('input/tex.js'),{MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js'),{MapHandler}=load('input/tex/MapHandler.js');
const {tagformatConfig}=load('input/tex/tagformat/TagFormatConfiguration.js');
const {TagsFactory}=load('input/tex/Tags.js');
const configurationFiles={base:'BaseConfiguration',ams:'AmsConfiguration',cases:'CasesConfiguration',mathtools:'MathtoolsConfiguration',tagformat:'TagFormatConfiguration',setoptions:'SetOptionsConfiguration'};
for(const name of Object.keys(configurationFiles))load(`input/tex/${name}/${configurationFiles[name]}.js`);
function plain(v,seen=new WeakSet()){
 if(v===undefined)return{$:'undefined'};if(v===null||typeof v==='string'||typeof v==='boolean')return v;
 if(typeof v==='number')return Number.isNaN(v)?{$:'NaN'}:Object.is(v,-0)?{$:'-0'}:Number.isFinite(v)?v:{$:v<0?'-Infinity':'Infinity'};
 if(typeof v==='function')return{$:'function',name:v.name||'',length:v.length};if(typeof v!=='object')return String(v);
 if(seen.has(v))return{$:'cycle'};seen.add(v);if(Array.isArray(v))return Array.from({length:v.length},(_,i)=>Object.hasOwn(v,i)?plain(v[i],seen):{$:'hole'});
 if(v instanceof Map)return{$:'map',entries:[...v].map(([k,x])=>[plain(k,seen),plain(x,seen)])};
 const out={};for(const k of Reflect.ownKeys(v))out[typeof k==='symbol'?`symbol:${k.description||''}`:k]=plain(v[k],seen);return out;
}
function tree(n){return n==null?null:{kind:n.kind,attrs:plain(n.attributes?.getAllAttributes?.()??{}),properties:plain(n.getAllProperties?.()??{}),...(typeof n.getText==='function'?{text:n.getText()}:{}),children:(n.childNodes||[]).map(tree)};}
const cases=[
 ['tagformat-default',String.raw`\begin{equation}x\label{eq:alpha beta}\end{equation}`,['base','ams','tagformat'],{}],
 ['tagformat-base-parent',String.raw`\begin{equation}x\label{eq:base}\end{equation}`,['base','tagformat'],{tags:'base'}],
 ['tagformat-ams-parent',String.raw`\begin{equation}x\label{eq:ams}\end{equation}`,['base','ams','tagformat'],{tags:'ams'}],
 ['tagformat-none-parent',String.raw`\begin{equation}x\label{eq:none}\end{equation}`,['base','tagformat'],{}],
 ['tagformat-all-parent',String.raw`\begin{equation}x\label{eq:all}\end{equation}`,['base','ams','tagformat'],{tags:'all'}],
 ['tagformat-cases-parent',String.raw`\begin{numcases}{x=}a&first\\b&second\end{numcases}`,['base','ams','cases','tagformat'],{tags:'cases'}],
 ['tagformat-mathtools-parent',String.raw`\begin{equation}x\label{eq:mathtools}\end{equation}`,['base','ams','mathtools','tagformat'],{}],
 ['tagformat-ref-fallback',String.raw`\begin{equation}x\label{eq:ref}\end{equation}\ref{eq:ref}`,['base','ams','tagformat'],{}],
 ['tagformat-raw-callbacks',String.raw`x`,['base','ams','tagformat'],{}],
 ['tagformat-config-tags-registration',String.raw`x`,['base','ams','tagformat'],{}],
 ['tagformat-all-custom',String.raw`\begin{equation}x\label{eq:custom}\end{equation}\ref{eq:custom}`,['base','ams','tagformat'],{tagformat:{number:function number(n){return this.number===number&&arguments.length===1?`N${n}`:'bad-number';},tag:function tag(t){return this.tag===tag&&arguments.length===1?['[',t,']']:['bad-tag'];},ref:function ref(t){return this.ref===ref&&arguments.length===1?['<',t,'>']:['bad-ref'];},id:function id(value){return this.id===id&&arguments.length===1?`custom:${value}`:'bad-id';},url:function url(value,base){return this.url===url&&arguments.length===2?`${base}?id=${encodeURIComponent(value)}`:'bad-url';}}}],
 ['tagformat-custom-no-ref',String.raw`\begin{equation}x\label{eq:no-ref}\end{equation}\ref{eq:no-ref}`,['base','ams','tagformat'],{tagformat:{number:function number(n){return this.number===number&&arguments.length===1?`Q${n}`:'bad-number';},tag:function tag(t){return this.tag===tag&&arguments.length===1?['{',t,'}']:['bad-tag'];},id:function id(value){return this.id===id&&arguments.length===1?`no-ref:${value}`:'bad-id';},url:function url(value,base){return this.url===url&&arguments.length===2?`${base}#${encodeURIComponent(value)}`:'bad-url';}}}],
 ['setoptions-tex',String.raw`\setOptions{displaystyle=true}x`,['base','setoptions'],{}],
 ['setoptions-package-valid',String.raw`\setOptions[ams]{multlineWidth=60%}\begin{multline}x\\y\end{multline}`,['base','ams','setoptions'],{}],
 ['setoptions-invalid-package',String.raw`\setOptions[no-such-package]{x=1}x`,['base','setoptions'],{}],
 ['setoptions-parse-before-package-filter',String.raw`\setOptions[no-such-package]{=}`,['base','setoptions'],{}],
 ['setoptions-invalid-key',String.raw`\setOptions{unknownKey=1}x`,['base','setoptions'],{}],
 ['setoptions-package-blocked',String.raw`\setOptions[setoptions]{allowOptionsDefault=false}x`,['base','setoptions'],{}],
 ['setoptions-package-default-disabled',String.raw`\setOptions[ams]{multlineWidth=60%}x`,['base','ams','setoptions'],{setoptions:{allowPackageDefault:false}}],
 ['setoptions-option-default-disabled',String.raw`\setOptions{displaystyle=true}x`,['base','setoptions'],{setoptions:{allowOptionsDefault:false}}],
 ['setoptions-filter-value',String.raw`\setOptions{FindTeX=/^x$/}x`,['base','setoptions'],{setoptions:{allowOptions:{tex:{FindTeX:true}}}}],
 ['setoptions-live-source-change',String.raw`\setOptions{displaystyle=true}x`,['base','setoptions'],{setoptions:{filterPackage:function filterPackage(parser,extension){if(this.filterPackage!==filterPackage||arguments.length!==2)return false;parser.options.setoptions.allowOptions.tex.displaystyle=false;return true;}}}],
 ['setoptions-package-data-preserved',String.raw`\setOptions{displaystyle=false}x`,['base','ams','setoptions'],{}]
];
function data(jax){return jax?plain(jax.parseOptions.packageData):null;}
function tagParent(jax){
 if(!jax?.parseOptions.tags)return null;
 const tagFormatPrototype=Object.getPrototypeOf(jax.parseOptions.tags);
 const parentPrototype=tagFormatPrototype&&Object.getPrototypeOf(tagFormatPrototype);
 return parentPrototype?.constructor?.name??null;
}
function parentName(instance){const prototype=Object.getPrototypeOf(instance),parent=prototype&&Object.getPrototypeOf(prototype);return parent?.constructor?.name??null;}
const output=[];
let callbackTarget=null;
for(const [name,input,packages,settings] of cases){
 const callbackEvents=[];
 if(name==='tagformat-raw-callbacks'){
  const format={};
  const callback=(callbackName,length,body)=>{const fn=function(...args){callbackEvents.push({name:callbackName,receiver:this===undefined?'undefined':this===callbackTarget?'tagformat':'other',args:plain(args)});return body(...args);};Object.defineProperty(fn,'name',{value:callbackName});Object.defineProperty(fn,'length',{value:length});return fn;};
  format.number=callback('number',1,()=>({kind:'raw-number'}));
  format.tag=callback('tag',1,()=>null);
  format.ref=callback('ref',1,()=>23);
  format.id=callback('id',1,()=>undefined);
  format.url=callback('url',2,()=>({kind:'raw-url'}));
  settings.tagformat=format;
 }
 let jax=null,result=null,error=null;
 try{jax=new TeX({...settings,packages});jax.setMmlFactory(new MmlFactory());const item=new AbstractMathItem(input,jax,true);result=tree(jax.compile(item,{}));}
 catch(e){error={name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)};}
 const setoptions=MapHandler.getMap('setoptions');
 let callbackProbe=null;
 if(name==='tagformat-raw-callbacks'&&jax){
  callbackTarget=jax.parseOptions.options.tagformat;
  const tags=jax.parseOptions.tags;
  callbackProbe={values:[tags.formatNumber(8),tags.formatTag('tag'),tags.formatRef('ref'),tags.formatId('id'),tags.formatUrl('id','base')].map(value=>plain(value)),events:callbackEvents};
 }
 let configTagsProbe=null;
 if(name==='tagformat-config-tags-registration'&&jax){
  const observed=[];
  const selected=jax.parseOptions.tags.constructor;
  function CustomTags(){return selected.apply(this,arguments);}
  CustomTags.prototype=Object.create(selected.prototype);
  Object.defineProperty(CustomTags.prototype,'constructor',{value:CustomTags,writable:true,configurable:true});
  jax.parseOptions.options.tags='custom';
  tagformatConfig({tags:{custom:CustomTags}},jax);
  const custom=TagsFactory.create(jax.parseOptions.options.tags);
  observed.push({kind:'custom',className:custom.constructor.name,parentName:parentName(custom)});
  jax.parseOptions.options.tags='missing';
  tagformatConfig({tags:{}},jax);
  const missing=TagsFactory.create(jax.parseOptions.options.tags);
  observed.push({kind:'missing',className:missing.constructor.name,parentName:parentName(missing)});
  configTagsProbe=observed;
 }
 output.push({name,input,packages,settings:plain(settings),result,error,options:jax?plain(jax.parseOptions.options):null,packageData:data(jax),tagClass:jax?.parseOptions.tags?.constructor?.name??null,tagParent:tagParent(jax),tagOptions:jax?plain(jax.parseOptions.options.tagformat):null,callbackProbe,configTagsProbe,setoptionsMap:setoptions?plain(setoptions.map):null});
}
const record={schema:1,scopeSha256:digest(scopePath),sourceLockSha256:digest(path.join(__dirname,'source-lock.json')),cases:output};
const destination=path.join(root,'build/math-port/tag-setoptions-original.json');fs.mkdirSync(path.dirname(destination),{recursive:true});fs.writeFileSync(destination,JSON.stringify(record,null,2)+'\n');
console.log(JSON.stringify({cases:output.length,sourceFiles:lock.sources.length,sourceLockSha256:record.sourceLockSha256,output:path.relative(root,destination)}));
