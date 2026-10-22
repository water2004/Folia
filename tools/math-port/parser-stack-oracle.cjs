// Root original Stack.ts/CJS observations. The native test fixture supplies
// observable virtual item/factory hooks, never an alternative stack algorithm.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory}=require(path.join(process.env.FOLIA_MATH_PORT_TOOLS||__dirname,'simple-mml-reference.cjs')),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const source='ts/input/tex/Stack.ts',sha='36be7583b14111961367e9e3f1dd5911456d235b096a2fcee62f438d37c7e11a';
const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),sha);assert.equal(bytes.toString().replaceAll('\r\n','\n'),fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'));
const Stack=require(path.join(base,'cjs/input/tex/Stack.js')).default,SF=require(path.join(base,'cjs/input/tex/StackItemFactory.js')).default,{BaseItem}=require(path.join(base,'cjs/input/tex/StackItem.js'));
const val=p=>p===undefined?'U':p===null?'N':typeof p==='object'?'object':String(p);
const rec=p=>p&&typeof p==='object'?Object.keys(p).map(k=>k+'='+val(p[k])).join(','):val(p);
function fixture(){
 const trace=[],f=new SF(),m=new MmlFactory(),a=m.create('mi'),b=m.create('mn');let s=null;
 class Probe extends BaseItem{constructor(f,...args){super(f);this.label='probe';this._env=undefined;this._kind='probe';if(args.length)this.Push(...args);}get kind(){return this._kind;}checkItem(item){trace.push('check:'+this.label+':'+item.label);return[null,true];}toString(){return this.label;}}
 class Start extends Probe{constructor(f,global){super(f);this.label='start';this._kind='start';this._env={start:1};this.global=global;}}
 class Mml extends Probe{constructor(f,node){super(f,node);this.label='mml';this._kind='mml';}}
 f.setNodeClass('start',Start);f.setNodeClass('mml',Mml);f.setNodeClass('probe',Probe);
 function item(label,kind=label){const x=f.create('probe');x.label=label;x._kind=kind;return x;}
 function make(env,inner=false){s=new Stack(f,env,inner);return s;}
 return{trace,f,m,a,b,item,make,get s(){return s;},set s(x){s=x;}};
}
const rows=[];function add(label,run){const e=fixture();let result='THREW',error='ok';try{result=run(e);}catch(x){error=x.name+'|'+x.message;}const state=e.s?'h='+e.s.height+':env='+rec(e.s._env):'no-stack';rows.push({label,expected:error+'|'+result+'|'+e.trace.join('/')+'|'+state});}
for(const[tag,value]of [['U',undefined],['N',null],['false',false],['0',0],['empty',''],['env',{q:2}]])add('constructor:'+tag,e=>{e.make(value);return e.s.Top().label+':'+rec(e.s.global)+':'+(e.s.global===e.s.Top().global?'same':'bad');});
add('constructor-inner',e=>{e.make(undefined,true);return rec(e.s.global);});
add('constructor-global-factory-identity',e=>{const original=e.f.node.start;e.f.node.start=function(...args){e.trace.push('arg:'+rec(args[0]));return original.apply(this,args);};e.make();return rec(e.s.Top().global);});
for(const[tag,value]of [['U',undefined],['N',null],['false',false],['0',0],['empty',''],['record',{a:3}]])add('set-env:'+tag,e=>{e.make();e.s.env=value;return rec(e.s.env);});
for(const[n]of [[undefined],[null],[0],[1],[2],[-1],[0.5],[NaN],[Infinity],['1'],['2']])add('top:'+String(n),e=>{e.make();const top=e.s.Top(n);return top?top.label:val(top);});
add('top-multiple',e=>{e.make();e.s.Push(e.item('x'),e.item('y'));return[1,2,3,4].map(n=>{const p=e.s.Top(n);return p?p.label:val(p);}).join(':');});
add('push-falsy',e=>{e.make();e.s.Push(undefined,null,false,0,'',undefined);return e.s.Top().label;});
add('push-node-wrap',e=>{e.make();e.s.Push(e.a);const x=e.s.Top();return x.kind+':'+x.First.kind+':'+(x.global===e.s.global?'same':'bad');});
add('push-success-item-identity',e=>{e.make();const x=e.item('x');e.s.Push(x);return(e.s.Top()===x?'same':'bad')+':'+rec(x.env);});
for(const[v]of [[false],[0],[''],[undefined],[null],[true]])add('check-success:'+String(v),e=>{e.make();const x=e.item('x');e.s.Top().checkItem=received=>{e.trace.push('global:'+(received.global===e.s.global?'same':'bad'));return[null,v];};e.s.Push(x);return e.s.Top().label+':'+rec(x.env);});
add('check-throws-global-retained',e=>{e.make();const x=e.item('x');e.s.Top().checkItem=received=>{e.trace.push('global:'+(received.global===e.s.global?'same':'bad'));throw Error('check');};e.s.Push(x);return'ok';});
for(const[v]of [[undefined],[null]])add('check-null-tuple:'+String(v),e=>{e.make();e.s.Top().checkItem=()=>v;e.s.Push(e.item('x'));return'ok';});
add('null-kind-still-environment',e=>{e.make();const x=e.item('x','null');x.env={nullenv:7};e.s.Push(x);return e.s.Top().label+':'+rec(x.env);});
for(const copy of [false,true])add('copy-env:'+copy,e=>{e.make({a:1,b:2});const x=e.item('x');x.env={a:9,c:3};Object.defineProperty(x,'copyEnv',{get:()=>copy});e.s.Push(x);return rec(x.env);});
add('env-receiver-before-source-live-reads',e=>{e.make({old:1});const x=e.item('x'),target={target:2},last={last:3};let n=0;Object.defineProperty(x,'env',{get(){e.trace.push('item-env:'+ ++n);if(n===2)e.s.env={current:4};return n===3?last:target;}});Object.defineProperty(x,'copyEnv',{get(){e.trace.push('copy');return true;}});e.s.Push(x);return rec(target)+':'+rec(last);});
add('env-falsy-setter-current',e=>{e.make({e:1});const x=e.item('x');Object.defineProperty(x,'env',{get(){e.trace.push('item-env');e.s.env={changed:2};return false;},set(p){e.trace.push('set:'+rec(p));x._env=p;}});e.s.Push(x);return'ok';});
add('env-second-read-null-retains-append',e=>{e.make();const x=e.item('x');let n=0;Object.defineProperty(x,'env',{get(){e.trace.push('env:'+ ++n);return n===1?{}:null;}});e.s.Push(x);return'ok';});
add('env-getter-throw-retains-append',e=>{e.make();const x=e.item('x');Object.defineProperty(x,'env',{get(){throw Error('env');}});e.s.Push(x);return'ok';});
add('copy-getter-throw-retains-append',e=>{e.make();const x=e.item('x');x.env={};Object.defineProperty(x,'copyEnv',{get(){throw Error('copy');}});e.s.Push(x);return'ok';});
add('recursive-replacement-discard-original',e=>{e.make();const x=e.item('x'),y=e.item('y'),z=e.item('z');e.s.Top().checkItem=()=>{e.trace.push('replace');return[[y,z],true];};y.checkItem=received=>{e.trace.push('check:y:'+received.label);return[null,true];};e.s.Push(x);return e.s.Top().label+':'+rec(x.env)+':'+(y.global===e.s.global&&z.global===e.s.global?'same':'bad');});
add('recursive-empty-array-truthy',e=>{e.make();e.s.Top().checkItem=()=>[[],true];e.s.Push(e.item('x'));return val(e.s.Top());});
add('recursive-global-live-after-pop',e=>{e.make();const x=e.item('x'),y=e.item('y');e.s.Top().checkItem=()=>[[y],true];Object.defineProperty(e.s.Top(),'isOpen',{get(){e.s.global={next:8};return false;}});e.s.Push(x);return rec(y.global);});
for(const open of [false,true])add('pop-env-preserved:'+open,e=>{e.make();const x=e.item('x');x.env={x:2};Object.defineProperty(x,'isOpen',{get:()=>open});e.s.Push(x);const popped=e.s.Pop();return popped.label+':'+rec(x.env)+':'+e.s.Top().label;});
add('pop-last-fresh-env',e=>{e.make();const old=e.s.env,popped=e.s.Pop();return popped.label+':'+(e.s.env!==old?'fresh':'bad')+':'+val(e.s.Top());});
add('pop-empty-error',e=>{e.make();e.s.Pop();e.s.Pop();return'ok';});
add('pop-open-getter-error-after-remove',e=>{e.make();const x=e.item('x');x.env={x:2};e.s.Push(x);Object.defineProperty(x,'isOpen',{get(){throw Error('open');}});e.s.Pop();return'ok';});
add('pop-top-env-error-after-remove',e=>{e.make();e.s.Push(e.item('x'));Object.defineProperty(e.s.stack[0],'env',{get(){throw Error('top-env');}});e.s.Pop();return'ok';});
add('pop-top-live-after-open-callback',e=>{e.make();const x=e.item('x'),y=e.item('y');y.env={y:9};e.s.Push(x);Object.defineProperty(x,'isOpen',{get(){e.s.Push(y);return false;}});e.s.Pop();return e.s.Top().label;});
for(const[v]of [[undefined],[false],[true],[0],[''],['yes']])add('prev:'+String(v),e=>{e.make();const start=e.s.Top();start.Push(e.a,e.b);return e.s.Prev(v).kind+':'+start.Size();});
for(const[v]of [[false],[true]])add('prev-empty:'+v,e=>{e.make();e.s.Pop();return val(e.s.Prev(v));});
add('replacement-captured-tuple-slot',e=>{e.make();const x=e.item('x'),y=e.item('y'),z=e.item('z'),t=[[y],true];e.s.Top().checkItem=()=>t;Object.defineProperty(e.s.Top(),'isOpen',{get(){t[0]=[z];return false;}});e.s.Push(x);return e.s.Top().label;});
add('replacement-spread-after-pop',e=>{e.make();const x=e.item('x'),y=e.item('y'),z=e.item('z'),t=[[y],true];e.s.Top().checkItem=()=>t;Object.defineProperty(e.s.Top(),'isOpen',{get(){t[0][0]=z;return false;}});e.s.Push(x);return e.s.Top().label;});
add('input-rest-array-snapshot',e=>{e.make();const x=e.item('x'),y=e.item('y'),z=e.item('z'),args=[x,y];e.s.Top().checkItem=()=>{args[1]=z;return[null,true];};e.s.Push(...args);return e.s.Top().label;});
add('environment-own-only-alias',e=>{const source=Object.create({inherited:8}),nested={v:1};source.nested=nested;e.make(source);const x=e.item('x'),target={};x.env=target;e.s.Push(x);nested.v=2;return rec(target)+':'+rec(target.nested)+':'+val(target.inherited)+':'+(target.nested===nested?'same':'bad');});
add('string-basic',e=>{e.make();e.s.Push(e.item('x'),e.item('y'));return e.s.toString();});
add('string-empty',e=>{e.make();e.s.Pop();return e.s.toString();});
add('string-live-captured-length',e=>{e.make();const start=e.s.Top(),x=e.item('x'),y=e.item('y'),z=e.item('z');e.s.Push(x);start.toString=()=>{e.trace.push('string:start');e.s.Pop();e.s.Push(y,z);return'S';};y.toString=()=>{e.trace.push('string:y');return'Y';};return e.s.toString();});
add('string-shrink-live-slots',e=>{e.make();const start=e.s.Top();e.s.Push(e.item('x'));start.toString=()=>{e.s.Pop();return'S';};return e.s.toString();});

// Frozen original Source JSON remains byte-identical; only renderer-only cpp fields are projected.
assert.ok(process.argv.includes('--check'),'Source-only oracle supports read-only --check');
const frozenProfilePath=path.join(root,"docs/math-port/evidence/p43-parser-stack-source.json");
const frozenBytes=fs.readFileSync(frozenProfilePath);
function sourceFields(value){if(Array.isArray(value))return value.map(sourceFields);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([key])=>key!=='cpp').map(([key,v])=>[key,sourceFields(v)]));return value;}
const actualSourceProfile={source,sha256:sha,rows};
assert.deepStrictEqual(sourceFields(actualSourceProfile),sourceFields(JSON.parse(frozenBytes)),'Original Source inputs/expectations/pins changed');
console.log(JSON.stringify({sourceCases:rows.length,profile:"docs/math-port/evidence/p43-parser-stack-source.json",profileSha256:crypto.createHash('sha256').update(frozenBytes).digest('hex'),mode:'check'}));
