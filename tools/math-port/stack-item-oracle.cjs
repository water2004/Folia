// Root gold: execute the pinned original MmlStack, never a rewritten evaluator.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory}=require(path.join(process.env.FOLIA_MATH_PORT_TOOLS||__dirname,'simple-mml-reference.cjs'));
const base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const file='ts/input/tex/StackItem.ts',bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',file));
assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),'ac138d321e1efbc54afbd961a4b6bfa24f7d0923eddba1c8d891dee9424602b5');
assert.equal(bytes.toString().replaceAll('\r\n','\n'),fs.readFileSync(path.join(base,file),'utf8').replaceAll('\r\n','\n'));
const {MmlStack}=require(path.join(base,'cjs/input/tex/StackItem.js'));
const {NodeFactory}=require(path.join(base,'cjs/input/tex/NodeFactory.js'));
const tag=n=>n===undefined?'U':n===null?'N':n.kind;
const snap=a=>'len='+a.length+';'+Array.from({length:a.length},(_,i)=>i in a?tag(a[i]):'H').join(',');
function env(){
 const trace=[],m=new MmlFactory(),nf=new NodeFactory();nf.setMmlFactory(m);nf.configuration={addNode(k){trace.push('add:'+k);}};
 const create=nf.create;nf.create=function(k,...r){trace.push('create:'+k+':'+r.length);return create.call(this,k,...r);};
 const config={get nodeFactory(){trace.push('nf');return nf;}},factory={configuration:config};
 class Probe extends MmlStack{get factory(){trace.push('factory');return this.factoryRef;}get array(){return this.nodes;}}
 const s=new Probe([]);s.factoryRef=factory;return{trace,m,nf,config,factory,s,a:m.create('mi'),b:m.create('mn'),c:m.create('mo')};
}
const rows=[];
function add(label,fn){const e=env();let result='THREW',error='ok';try{result=fn(e);}catch(x){error=x.name+'|'+x.message;}rows.push({label,expected:error+'|'+result+'|'+e.trace.join('/')});}
add('initial-source-fields',({s})=>(s.startStr===''?'empty':'bad')+':'+s.startI+':'+s.stopI+':'+snap(s.array));
add('push-pop-tagged',({s,a})=>{s.Push(undefined,null,a);const old=s.array,first=s.Pop(),second=s.Pop(),third=s.Pop(),fourth=s.Pop();return[first,second,third,fourth].map(tag).join(':')+':'+snap(old);});
add('push-zero',({s})=>{let old=s.array;s.Push();return(old===s.array?'same':'bad')+':'+snap(old);});
add('constructor-array-alias',({trace,a,b})=>{let arr=[a];class P extends MmlStack{get array(){return this.nodes;}}let other=new P(arr);arr.push(b);return(other.array===arr?'same':'bad')+':'+tag(other.First)+':'+snap(other.array);});
add('first-empty-named',({s,a})=>{s.First=a;return tag(s.First)+':'+snap(s.array)+':'+(Object.hasOwn(s.array,'-1')?'own':'missing');});
add('first-last-order',({s,a,b,c})=>{s.Push(a,b,c);let before=tag(s.First)+':'+tag(s.Last);s.First=a;s.Last=c;return before+':'+snap(s.array);});
add('last-empty',({s,b})=>{let before=tag(s.Last);s.Last=b;return before+':'+tag(s.Last)+':'+snap(s.array);});
add('clear-rebind',({s,a,b,c})=>{let old=s.array;s.First=c;s.Push(a,b);s.Clear();return(old===s.array?'same':'fresh')+':'+snap(old)+':'+snap(s.array)+':'+tag(old['-1'])+':'+tag(s.First);});
const counts=[['omitted',undefined],['null',null],['zero',0],['one',1],['two',2],['negative',-1],['large',20],['fraction',1.25],['negative-fraction',-1.25],['nan',NaN],['inf',Infinity],['-inf',-Infinity],['false',false],['string','2']];
for(const [label,n]of counts)add('peek-sparse:'+label,({s,a,b})=>{s.array.length=4;s.array[0]=a;s.array[3]=b;let old=s.array,result=s.Peek(n);return snap(result)+':'+(result===old?'same':'fresh')+':'+snap(old);});
add('sparse-prototype-pop-peek',({s,a,b,c})=>{const parent=[];parent[1]=b;parent[-1]=c;Object.setPrototypeOf(s.array,parent);s.array.length=2;s.array[0]=a;const peek=s.Peek(2),pop=s.Pop();return snap(peek)+':'+tag(pop)+':'+snap(s.array);});
for(const [label,size]of [['zero',0],['fraction',1.5],['nan',NaN],['inf',Infinity],['negative',-2]])add('first-virtual-size:'+label,({s,c,trace})=>{s.Size=()=>{trace.push('size');return size;};s.First=c;return tag(s.First)+':'+snap(s.array);});
add('first-get-captures-array',({s,a,b,trace})=>{s.Push(a,b);let old=s.array;s.Size=()=>{trace.push('size-clear');s.Clear();return 2;};return tag(s.First)+':'+snap(old)+':'+snap(s.array);});
add('first-set-captures-array',({s,a,b,c,trace})=>{s.Push(a,b);let old=s.array;s.Size=()=>{trace.push('size-clear');s.Clear();return 2;};s.First=c;return snap(old)+':'+snap(s.array);});
add('peek-captures-array',({s,a,b,trace})=>{s.Push(a,b);let old=s.array;s.Size=()=>{trace.push('size-clear');s.Clear();return 2;};return snap(s.Peek(1))+':'+snap(old)+':'+snap(s.array);});
add('last-no-size-dispatch',({s,a})=>{s.Size=()=>{throw Error('unexpected Size');};s.Last=a;return tag(s.Last);});
for(const count of [0,1,2])for(const [label,inferred]of [['default',undefined],['false',false],['null',null]])for(const force of [false,true])add('tomml:'+count+':'+label+':'+force,({s,a,b})=>{s.Push(...[a,b].slice(0,count));let value=s.toMml(inferred,force);return tag(value)+':'+snap(s.array)+':'+(value&&value.childNodes===s.array?'shared':'other');});
add('tomml-first-dispatch',({s,a,c,trace})=>{s.Push(a);Object.defineProperty(s,'First',{get(){trace.push('first');return c;}});return tag(s.toMml());});
add('tomml-does-not-use-size',({s,a,trace})=>{s.Push(a);s.Size=()=>{trace.push('size');return 0;};return tag(s.toMml())+':'+snap(s.array);});
add('tomml-virtual-create',({s,a,b,c,trace})=>{s.Push(a,b);let old=s.array;s.create=(k,...args)=>{trace.push(k+':'+args[0]+':'+(args[1]===old?'same':'copy')+':'+String(args[2]));return c;};return tag(s.toMml(false));});
add('create-forward-args',({s})=>tag(s.create('text','x')));
add('create-nodefactory-live',({s,factory,nf,trace})=>{factory.configuration={get nodeFactory(){trace.push('nf');trace.push('getter');factory.configuration={get nodeFactory(){trace.push('nf');return nf;}};return nf;}};s.create('text','x');s.create('text','x');return'ok';});
for(const [tagv,js]of [['U',undefined],['N',null]]){
 add('factory-nullish:'+tagv,({s})=>{s.factoryRef=js;return tag(s.create('text'));});
 add('configuration-nullish:'+tagv,({s,factory})=>{factory.configuration=js;return tag(s.create('text'));});
 add('nodefactory-nullish:'+tagv,({s,factory,trace})=>{factory.configuration={get nodeFactory(){trace.push('nf');return js;}};return tag(s.create('text'));});
}
const nullMethods=[['Push',s=>s.Push()],['Pop',s=>s.Pop()],['First',s=>s.First],['setFirst',(s,a)=>{s.First=a;}],['Last',s=>s.Last],['setLast',(s,a)=>{s.Last=a;}],['Peek',s=>s.Peek()],['Size',s=>s.Size()],['toMml',s=>s.toMml()]];
for(const[label,fn]of nullMethods)add('null-array:'+label,({s,a,trace})=>{const other=new s.constructor(null);other.factoryRef=s.factoryRef;fn(other,a);return'ok';});
add('null-array-clear',({s})=>{const other=new s.constructor(null);other.Clear();return snap(other.array);});
for(const method of ['First','setFirst','Peek'])add('null-array-virtual-order:'+method,
 ({s,a,trace})=>{const other=new s.constructor(null);other.Size=()=>{trace.push('size');return 0;};if(method==='First')other.First;else if(method==='setFirst')other.First=a;else other.Peek();return'ok';});
add('first-inherited-named',({s,b,c})=>{let parent=[];parent[-1]=b;Object.setPrototypeOf(s.array,parent);let before=tag(s.First);s.First=c;return before+':'+tag(s.First)+':'+tag(parent[-1])+':'+snap(s.array);});
add('raw-nodes-not-accessor',({s,a,b,c})=>{let old=s.array;Object.defineProperty(s,'nodes',{get(){throw Error('unexpected nodes getter');}});s.Push(a,b);s.First=c;s.Last=a;let result=tag(s.First)+':'+tag(s.Last)+':'+snap(s.Peek(2))+':'+tag(s.Pop())+':'+s.Size()+':'+tag(s.toMml());s.Clear();return result+':'+s.Size()+':'+snap(old);});

// Frozen original Source JSON remains byte-identical; only renderer-only cpp fields are projected.
assert.ok(process.argv.includes('--check'),'Source-only oracle supports read-only --check');
const frozenProfilePath=path.join(root,"docs/math-port/evidence/p42-mml-stack-source.json");
const frozenBytes=fs.readFileSync(frozenProfilePath);
function sourceFields(value){if(Array.isArray(value))return value.map(sourceFields);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([key])=>key!=='cpp').map(([key,v])=>[key,sourceFields(v)]));return value;}
const actualSourceProfile=rows;
assert.deepStrictEqual(sourceFields(actualSourceProfile),sourceFields(JSON.parse(frozenBytes)),'Original Source inputs/expectations/pins changed');
console.log(JSON.stringify({sourceCases:rows.length,profile:"docs/math-port/evidence/p42-mml-stack-source.json",profileSha256:crypto.createHash('sha256').update(frozenBytes).digest('hex'),mode:'check'}));
