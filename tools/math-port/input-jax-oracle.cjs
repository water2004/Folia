// Root original core/InputJax observations; stimuli do not implement a jax.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=(process.env.FOLIA_MATH_ROOT||path.resolve(__dirname,'../..')),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p46-parser-options-source.json'))),hashes=[];
for(const source of ['ts/core/InputJax.ts','ts/util/Options.ts','ts/util/FunctionList.ts','ts/util/PrioritizedList.ts']){
 const entry=lock.hashes.find(x=>x.source===source);assert.ok(entry,source);
 const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);
 assert.equal(fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));
 assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(base,source.replace(/^ts\//,'cjs/').replace(/\.ts$/,'.js')))).digest('hex'),entry.cjs_sha256);hashes.push(entry);
}
const {AbstractInputJax}=require(path.join(base,'cjs/core/InputJax.js')),{FunctionList}=require(path.join(base,'cjs/util/FunctionList.js'));
const val=v=>v===undefined?'U':v===null?'N':typeof v==='object'?'object':String(v),rows=[];
function fixture(){
 const trace=[],math={},otherMath={},document={},data={value:7},factory={},adaptor={};let first=null;
 class Probe extends AbstractInputJax{compile(){return factory;}filter(list,m,d,v){return this.executeFilters(list,m,d,v);}}
 // Source subclass has distinct metadata but inherits actual original defaults.
 Probe.NAME='generic';Probe.OPTIONS={preFilters:[],postFilters:[]};
 const e={trace,math,otherMath,document,data,factory,adaptor,Class:Probe,captured:null,first:null,hook:null,jax:null,
  make(options){return this.jax=new Probe(options);},
  capture(){Object.defineProperty(Probe.prototype,'constructor',{configurable:true,get(){e.captured=this;return Probe;}});},
  run(list,value=data){return this.jax.filter(list,math,document,value);}};
 function callback(label){const fn=function(args){if(label==='f'){first=args;e.first=args;}trace.push(label+':'+String(this.item===fn)+':'+String(args.math===math)+':'+String(args.document===document)+':'+String(first===args));if(e.hook)return e.hook(label,args);};return fn;}
 e.f=callback('f');e.g=callback('g');return e;
}
function add(label,run){const e=fixture();let error='ok',result='THREW';try{result=run(e);}catch(x){error=x.name||'raw';}rows.push({label,expected:error+'|'+result+'|'+e.trace.join('/')});}
add('initial-fields',e=>{const j=e.make();return[j.name,j.processStrings,j.adaptor===null,j.mmlFactory===null,Array.from(j.preFilters).length,Array.from(j.postFilters).length].join(':');});
add('two-instances-options-and-lists',e=>{const a=e.make(),b=e.make();return[a.options!==b.options,a.options.preFilters!==b.options.preFilters,a.preFilters!==b.preFilters,a.preFilters!==a.postFilters].join(':');});
add('inherited-original-class-metadata',e=>{delete e.Class.NAME;delete e.Class.OPTIONS;const j=e.make();return j.name+':'+Array.from(j.preFilters).length+':'+String(j.options.preFilters!==AbstractInputJax.OPTIONS.preFilters)+':'+String(j.constructor===e.Class);});
add('live-class-name',e=>{const j=e.make(),first=j.name;e.Class.NAME='changed';return first+':'+j.name;});
add('live-instance-constructor',e=>{const j=e.make();j.constructor={NAME:'other'};return j.name;});
add('constructor-CLASS-capture',e=>{const other={NAME:'other',OPTIONS:{marker:2}};e.Class.OPTIONS.marker=1;Object.defineProperty(e.Class.prototype,'constructor',{get(){e.trace.push('constructor');Object.defineProperty(this,'constructor',{value:other});return e.Class;}});const j=e.make();return j.name+':'+Array.from(j.preFilters).length+':'+j.options.marker;});
add('constructor-initial-field-phase',e=>{Object.defineProperty(e.Class.prototype,'constructor',{get(){e.trace.push('phase:'+val(this.options)+':'+String(this.preFilters===undefined)+':'+String(this.postFilters===undefined)+':'+String(this.adaptor===null));return e.Class;}});e.make();return'ok';});
add('constructor-options-getter-order',e=>{e.capture();const defs=e.Class.OPTIONS;Object.defineProperty(e.Class,'OPTIONS',{get(){e.trace.push('OPTIONS:'+val(e.captured.options));return defs;}});return e.make().name;});
add('defaults-user-merge-and-alias',e=>{const nested={x:1,fn:e.f};e.Class.OPTIONS.nested=nested;const j=e.make({nested:{x:9}});return[j.options.nested.x,j.options.nested!==nested,j.options.nested.fn===e.f,nested.x].join(':');});
add('actual-filter-option-construction',e=>{const j=e.make({preFilters:[[e.f,8],[e.g,1]],postFilters:[e.f]});j.preFilters.execute({math:e.math,document:e.document,data:e.data});j.postFilters.execute({math:e.math,document:e.document,data:e.data});return'ok';});
for(const[tag,v]of [['undefined',undefined],['null',null],['false',false],['zero',0]])add('constructor-options:'+tag,e=>{const j=e.make(v);return Array.from(j.preFilters).length+':'+Array.from(j.postFilters).length;});
add('array-getter-during-merge-before-options-assignment',e=>{e.capture();const pre=[];Object.defineProperty(pre,0,{enumerable:true,get(){e.trace.push('pre:get:'+val(e.captured.options));return e.f;}});e.Class.OPTIONS.preFilters=pre;const j=e.make();return Array.from(j.preFilters).length+':'+Array.from(j.postFilters).length;});
add('constructor-pre-merge-error-keeps-fields-absent',e=>{e.capture();const pre=[];Object.defineProperty(pre,0,{enumerable:true,get(){throw Error('pre');}});e.Class.OPTIONS.preFilters=pre;try{e.make();}catch(x){return val(e.captured.options)+':'+String(e.captured.preFilters===undefined)+':'+String(e.captured.postFilters===undefined);}return'bad';});
add('constructor-post-merge-error-keeps-fields-absent',e=>{e.capture();const post=[];Object.defineProperty(post,0,{enumerable:true,get(){throw Error('post');}});e.Class.OPTIONS.preFilters=[e.f];e.Class.OPTIONS.postFilters=post;try{e.make();}catch(x){return val(e.captured.options)+':'+String(e.captured.preFilters===undefined)+':'+String(e.captured.postFilters===undefined);}return'bad';});
add('setters-retain-real-identities',e=>{const j=e.make();j.setAdaptor(e.adaptor);j.setMmlFactory(e.factory);return String(j.adaptor===e.adaptor)+':'+String(j.mmlFactory===e.factory);});
add('setters-nullish',e=>{const j=e.make();j.setAdaptor(undefined);j.setMmlFactory(null);return String(j.adaptor===undefined)+':'+String(j.mmlFactory===null);});
add('initialize-reset-no-op',e=>{const j=e.make(),options=j.options,pre=j.preFilters;j.setAdaptor(e.adaptor);j.initialize();j.reset(3,e.adaptor);return[options===j.options,pre===j.preFilters,j.adaptor===e.adaptor].join(':');});
add('findMath-fresh-empty-arrays',e=>{const j=e.make(),a=j.findMath('x'),b=j.findMath(e.adaptor,null);a.push('sentinel');return String(a!==b)+':'+b.length;});
add('filter-record-and-data-identity',e=>{e.make();const list=new FunctionList([e.f,e.g]);return String(e.run(list)===e.data);});
add('filter-data-mutation-and-replacement',e=>{e.make();e.hook=(label,a)=>{if(label==='f'){a.data.value=9;a.data=17;}};return val(e.run(new FunctionList([e.f,e.g])))+':'+e.data.value;});
add('filter-false-stops-but-returns-data',e=>{e.make();e.hook=(label,a)=>{a.data=23;return false;};return val(e.run(new FunctionList([e.f,e.g])));});
add('filter-record-math-document-live',e=>{e.make();e.hook=(label,a)=>{if(label==='f'){a.math=e.otherMath;a.document=null;}};e.run(new FunctionList([e.f,e.g]));return'ok';});
add('filter-throw-retains-prior-effects',e=>{e.make();e.hook=(label,a)=>{a.data=31;throw Error('filter');};try{e.run(new FunctionList([e.f,e.g]));}catch(x){return val(e.first.data);}return'bad';});
add('filter-fresh-record-per-invocation',e=>{e.make();const list=new FunctionList([e.f]);e.run(list);const a=e.first;e.run(list);return String(a!==e.first);});
add('filter-heterogeneous-native-data',e=>{e.make();return String(e.run(new FunctionList([e.f]),e.adaptor)===e.adaptor);});
add('filter-empty-data-undefined',e=>{e.make();return String(e.jax.filter(new FunctionList(),e.math,e.document,undefined)===undefined);});
add('filter-raw-throw-identity',e=>{e.make();const raw={reason:7};e.hook=(label,a)=>{a.data=41;throw raw;};try{e.run(new FunctionList([e.f,e.g]));}catch(x){return String(x===raw)+':'+e.first.data;}return'bad';});

// Frozen original Source JSON remains byte-identical; only renderer-only cpp fields are projected.
assert.ok(process.argv.includes('--check'),'Source-only oracle supports read-only --check');
const frozenProfilePath=path.join(root,"docs/math-port/evidence/p48-input-jax-source.json");
const frozenBytes=fs.readFileSync(frozenProfilePath);
function sourceFields(value){if(Array.isArray(value))return value.map(sourceFields);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([key])=>key!=='cpp').map(([key,v])=>[key,sourceFields(v)]));return value;}
const actualSourceProfile={stage:'original core InputJax ordinary-option-array/bound-callback observations',hashes,rows};
assert.deepStrictEqual(sourceFields(actualSourceProfile),sourceFields(JSON.parse(frozenBytes)),'Original Source inputs/expectations/pins changed');
console.log(JSON.stringify({sourceCases:rows.length,profile:"docs/math-port/evidence/p48-input-jax-source.json",profileSha256:crypto.createHash('sha256').update(frozenBytes).digest('hex'),mode:'check'}));
