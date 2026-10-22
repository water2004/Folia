// Root original core/MathItem observations; native callbacks are stimuli only.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory}=require(path.join(process.env.FOLIA_MATH_PORT_TOOLS||__dirname,'simple-mml-reference.cjs')),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src'),source='ts/core/MathItem.ts',sha='c1c7308320ae15514fd1114c3f94642a4e8a8268bfe6f68192e8a557829626ce';
const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),sha);assert.equal(bytes.toString().replaceAll('\r\n','\n'),fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'));
const {AbstractMathItem,STATE,protoItem,newState}=require(path.join(base,'cjs/core/MathItem.js')),defaults={UNPROCESSED:0,FINDMATH:10,COMPILED:20,CONVERT:100,METRICS:110,RERENDER:125,TYPESET:150,INSERTED:200,LAST:10000};
const val=p=>p===undefined?'U':p===null?'N':typeof p==='object'?'object':String(p),rec=p=>p&&typeof p==='object'?Object.keys(p).map(k=>k+'='+val(p[k])).join(','):val(p);
function reset(){for(const k of Object.keys(STATE))delete STATE[k];Object.setPrototypeOf(STATE,Object.prototype);Object.assign(STATE,defaults);}
function fixture(){
 reset();const trace=[],m=new MmlFactory(),a=m.create('mi'),b=m.create('mn'),output1={},output2={};
 const actions={renderMath(item,doc,start){trace.push('render:'+val(start));},renderConvert(item,doc,end){trace.push('convert:'+val(end));}};
 const output={typeset(item,doc){trace.push('typeset');return output1;},escaped(item,doc){trace.push('escaped');return output2;}};
 const input={compile(item,doc){trace.push('compile');return a;}};
 const doc={renderActions:actions,outputJax:output},x=new AbstractMathItem('x',input),y=new AbstractMathItem('y',input);
 return{trace,m,a,b,output1,output2,actions,output,input,doc,x,y};
}
const rows=[];function add(label,run){const e=fixture();let result='THREW',error='ok';try{result=run(e);}catch(x){error=x.name+'|'+x.message;}rows.push({label,expected:error+'|'+result+'|'+e.trace.join('/')});reset();}
add('initial',e=>[val(e.x.math),val(e.x.display),rec(e.x.start),rec(e.x.end),val(e.x.root),val(e.x.typesetRoot),rec(e.x.metrics),rec(e.x.inputData),rec(e.x.outputData),val(e.x.state())].join(':'));
add('constructor-independent-data',e=>[e.x.metrics!==e.y.metrics,e.x.inputData!==e.y.inputData,e.x.outputData!==e.y.outputData,e.x.start!==e.x.end,e.x.start!==e.y.start].map(val).join(':'));
for(const[tag,d]of [['U',undefined],['N',null],['false',false],['true',true],['zero',0],['empty','']])add('constructor-display:'+tag,e=>{const p=new AbstractMathItem('z',e.input,d);return val(p.display)+':'+val(p.isEscaped);});
add('constructor-location-alias',e=>{const start={i:2},end={n:7},p=new AbstractMathItem('z',e.input,true,start,end);start.i=9;return val(p.start===start)+':'+rec(p.start)+':'+val(p.end===end);});
add('constructor-location-null',e=>{const p=new AbstractMathItem('z',e.input,true,null,null);return val(p.start)+':'+val(p.end);});
add('constructor-native-location-identity',e=>{const node={},location={node,i:1,n:2,delim:'$'},p=new AbstractMathItem('z',e.input,true,location,location);return val(p.start===location)+':'+val(p.start===p.end);});
add('isEscaped-live-display',e=>{e.x.display=null;const first=e.x.isEscaped;e.x.display=false;return val(first)+':'+val(e.x.isEscaped);});
add('setMetrics-replaces-identity',e=>{const old=e.x.metrics;e.x.setMetrics(10,4,100,1.2);return rec(e.x.metrics)+':'+val(old!==e.x.metrics)+':'+rec(old);});
add('setMetrics-null-missing',e=>{e.x.setMetrics(undefined,null,false,0);return rec(e.x.metrics);});
for(const[from,target]of [[0,0],[0,20],[20,10],[20,20],[20,30],[150,20],[150,149],[150,150],[150,151],[200,150],[200,199],[200,200],[201,0],[199,0],[149,0],[-1,0],[NaN,0],[Infinity,0]]){add('state:'+from+':'+target,e=>{e.x._state=from;const input=e.x.inputData,output=e.x.outputData,result=e.x.state(target);return val(result)+':'+val(input===e.x.inputData)+':'+val(output===e.x.outputData);});}
for(const[tag,p]of [['U',undefined],['N',null]])add('state-getter:'+tag,e=>{e.x._state=200;return val(e.x.state(p));});
add('state-removal-current-state',e=>{e.x._state=200;e.x.removeFromDocument=r=>{e.trace.push('remove:'+val(r));e.x._state=20;};const input=e.x.inputData,output=e.x.outputData,result=e.x.state(10,true);return val(result)+':'+val(input===e.x.inputData)+':'+val(output===e.x.outputData);});
add('state-removal-error-prefix',e=>{e.x._state=200;e.x.removeFromDocument=r=>{e.trace.push('remove:'+val(r));e.x._state=77;throw Error('remove');};try{e.x.state(0);}catch(x){return val(e.x._state)+':'+rec(e.x.inputData)+':'+rec(e.x.outputData);}return'bad';});
add('state-clear-record-aliases',e=>{e.x._state=200;const input=e.x.inputData,output=e.x.outputData;input.i=1;output.o=2;e.x.state(0);return rec(input)+':'+rec(output)+':'+rec(e.x.inputData)+':'+rec(e.x.outputData);});
add('state-live-threshold-after-remove',e=>{e.x._state=200;e.x.removeFromDocument=()=>{STATE.TYPESET=10;STATE.COMPILED=5;};const input=e.x.inputData,output=e.x.outputData,result=e.x.state(7);return val(result)+':'+val(input===e.x.inputData)+':'+val(output===e.x.outputData);});
for(const restore of [undefined,false,true]){add('reset:'+String(restore),e=>{e.x._state=200;e.x.state=function(n,r){e.trace.push('state:'+val(n)+':'+val(r));return AbstractMathItem.prototype.state.call(this,n,r);};e.x.reset(restore);return val(e.x._state);});}
add('remove-virtual-clear',e=>{e.x.clear=()=>e.trace.push('clear');e.x.removeFromDocument(true);return'ok';});
add('base-noops',e=>{e.x._state=150;e.x.updateDocument(null);e.x.clear();return val(e.x._state);});
add('render-delegation',e=>{e.actions.renderMath=(item,d,s)=>e.trace.push('render:'+val(s)+':'+val(item===e.x)+':'+val(d===e.doc));e.x.render(e.doc);return val(e.x._state);});
for(const[from,start]of [[0,undefined],[125,undefined],[200,125],[124,125],[200,null],[200,0]])add('rerender:'+from+':'+String(start),e=>{e.x._state=from;e.x.state=function(n,r){e.trace.push('state:'+val(n)+':'+val(r));return AbstractMathItem.prototype.state.call(this,n,r);};e.x.rerender(e.doc,start);return val(e.x._state);});
add('rerender-default-before-state',e=>{e.x._state=200;e.x.state=function(n,r){if(n===undefined)STATE.RERENDER=999;return AbstractMathItem.prototype.state.call(this,n,r);};e.x.rerender(e.doc);return val(e.x._state);});
add('rerender-live-actions-after-state',e=>{const other={renderMath(item,doc,n){e.trace.push('live-render:'+val(n));}};e.x._state=200;e.x.state=function(n,r){if(n!==undefined)e.doc.renderActions=other;return AbstractMathItem.prototype.state.call(this,n,r);};e.x.rerender(e.doc);return val(e.x._state);});
for(const[tag,end]of [['U',undefined],['N',null],['zero',0],['twenty',20]])add('convert:'+tag,e=>{e.x.convert(e.doc,end);return'ok';});
for(const from of [0,19,20,21,NaN]){add('compile-gate:'+String(from),e=>{e.x._state=from;e.x.root=e.b;e.x.compile(e.doc);return e.x.root.kind+':'+val(e.x._state);});}
add('compile-return-then-live-state',e=>{e.input.compile=item=>{e.trace.push('compile:'+val(item===e.x));STATE.COMPILED=77;return e.a;};e.x.compile(e.doc);return e.x.root.kind+':'+val(e.x._state);});
add('compile-gate-after-virtual-state',e=>{e.x.state=function(n,r){if(n===undefined){STATE.COMPILED=5;return 10;}return AbstractMathItem.prototype.state.call(this,n,r);};e.x.compile(e.doc);return val(e.x.root)+':'+val(e.x._state);});
add('compile-error-keeps-prior-root',e=>{e.x.root=e.b;e.input.compile=()=>{throw Error('compile');};try{e.x.compile(e.doc);}catch(x){return e.x.root.kind+':'+val(e.x._state);}return'bad';});
add('compile-state-setter-error-after-root',e=>{e.x.state=n=>{if(n!==undefined){e.trace.push('set:'+e.x.root.kind);throw Error('state');}return e.x._state;};e.x.compile(e.doc);return'ok';});
for(const escaped of [false,true])for(const from of [0,149,150])add('typeset-gate:'+escaped+':'+from,e=>{e.x._state=from;e.x.display=escaped?null:true;e.x.typeset(e.doc);return(e.x.typesetRoot===e.output1?'one':e.x.typesetRoot===e.output2?'two':val(e.x.typesetRoot))+':'+val(e.x._state);});
add('typeset-receiver-before-escaped',e=>{const other={typeset(){e.trace.push('wrong-output');return e.output2;}};Object.defineProperty(e.doc,'outputJax',{configurable:true,get(){e.trace.push('output-get');return e.output;}});Object.defineProperty(e.x,'isEscaped',{get(){e.trace.push('escaped-get');Object.defineProperty(e.doc,'outputJax',{get:()=>other});return false;}});e.x.typeset(e.doc);return e.x.typesetRoot===e.output1?'one':'two';});
add('typeset-live-state-after-output',e=>{e.output.typeset=()=>{e.trace.push('typeset');STATE.TYPESET=88;return e.output1;};e.x.typeset(e.doc);return(e.x.typesetRoot===e.output1?'one':'bad')+':'+val(e.x._state);});
add('typeset-error-retains-output',e=>{e.x.typesetRoot=e.output2;e.output.typeset=()=>{throw Error('typeset');};try{e.x.typeset(e.doc);}catch(x){return(e.x.typesetRoot===e.output2?'two':'bad')+':'+val(e.x._state);}return'bad';});
add('typeset-output-before-setter-error',e=>{e.x.state=function(n,r){if(n!==undefined)throw Error('state');return AbstractMathItem.prototype.state.call(this,n,r);};try{e.x.typeset(e.doc);}catch(x){return(e.x.typesetRoot===e.output1?'one':'bad')+':'+val(e.x._state);}return'bad';});
for(const[tag,p]of [['U',undefined],['N',null]])for(const method of ['render','typeset'])add('doc-nullish:'+method+':'+tag,e=>{e.x[method](p);return'ok';});
add('nullish-renderActions',e=>{e.doc.renderActions=null;e.x.render(e.doc);return'ok';});
add('nullish-inputJax',e=>{e.x.inputJax=null;e.x.compile(e.doc);return'ok';});
add('nullish-outputJax',e=>{e.doc.outputJax=undefined;e.x.typeset(e.doc);return'ok';});
for(const name of ['custom','COMPILED','toString','__proto__',''])add('newState:'+name,()=>{newState(name,42);return val(STATE[name]);});
add('newState-own-undefined-still-present',()=>{STATE.custom=undefined;newState('custom',42);return'ok';});
for(const[tag,d]of [['U',undefined],['N',null],['false',false],['true',true]])add('protoItem:'+tag,()=>{const p=protoItem('$','x','$',2,3,4,d);return rec(p)+':'+rec(p.start)+':'+rec(p.end);});

// Frozen original Source JSON remains byte-identical; only renderer-only cpp fields are projected.
assert.ok(process.argv.includes('--check'),'Source-only oracle supports read-only --check');
const frozenProfilePath=path.join(root,"docs/math-port/evidence/p45-math-item-source.json");
const frozenBytes=fs.readFileSync(frozenProfilePath);
function sourceFields(value){if(Array.isArray(value))return value.map(sourceFields);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([key])=>key!=='cpp').map(([key,v])=>[key,sourceFields(v)]));return value;}
const actualSourceProfile={source,sha256:sha,rows};
assert.deepStrictEqual(sourceFields(actualSourceProfile),sourceFields(JSON.parse(frozenBytes)),'Original Source inputs/expectations/pins changed');
console.log(JSON.stringify({sourceCases:rows.length,profile:"docs/math-port/evidence/p45-math-item-source.json",profileSha256:crypto.createHash('sha256').update(frozenBytes).digest('hex'),mode:'check'}));
