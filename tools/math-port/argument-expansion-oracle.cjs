// Root oracle: actual pinned MacroMap.parse/CJS helpers, never a helper model.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const hashes={
 'ts/input/tex/TokenMap.ts':'0209895b13569008b2129b61c88050e7c70e70ce9477d234b9a6e303a86a738e',
 'cjs/input/tex/TokenMap.js':'9e3f086080a6ab052b68b22a5d9ee3dbc5e17d7b0c03a31e789664b01df36cf7'};
for(const [file,sha] of Object.entries(hashes))assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(file.startsWith('ts/')?path.join(root,'.reference/MathJax-src'):base,file))).digest('hex'),sha);
assert.equal(fs.readFileSync(path.join(root,'.reference/MathJax-src/ts/input/tex/TokenMap.ts'),'utf8').replaceAll('\r\n','\n'),fs.readFileSync(path.join(base,'ts/input/tex/TokenMap.ts'),'utf8').replaceAll('\r\n','\n'));
const {MacroMap}=require(path.join(base,'cjs/input/tex/TokenMap.js')),{Macro}=require(path.join(base,'cjs/input/tex/Token.js'));
const rows=[];function value(x){if(x===undefined)return {type:'missing'};if(x===null)return {type:'null'};if(typeof x==='boolean')return {type:'boolean',value:x};if(typeof x==='number')return {type:'number',value:Number.isNaN(x)?'NaN':Object.is(x,-0)?'-0':x===Infinity?'Infinity':x===-Infinity?'-Infinity':x};if(typeof x==='string')return {type:'string',units:Array.from({length:x.length},(_,i)=>x.charCodeAt(i))};if(Array.isArray(x))return {type:'array',items:x.map(value)};return {type:'object',entries:Object.keys(x).map(k=>[k,value(x[k])])};}
let observed,calls;const callback=(env,token,...args)=>{assert.equal(token,'x');observed=args;calls++;};
const map=new MacroMap('root-argument-expansion',{});map.add('x',new Macro('x',callback));
function witness(raw){observed=undefined;calls=0;map.lookup('x')._args=raw;try{map.parse([{},'x']);assert.equal(calls,1);return observed;}catch(e){assert.equal(calls,0);return {error:e.name};}}
function add(name,raw){rows.push(value(witness(raw)));}
const raw=[undefined,null,false,true,0,-0,NaN,Infinity,-Infinity,42,'','abc','A𝄞e\u0301','\ud800x\udfff','\ud800\ud800\udc00\udc00','a\0b',[],[false,null,undefined,-0,NaN],{}, {length:3,0:'zero',2:'two'}];
raw.forEach((r,i)=>add('raw-'+i,r));
for(const length of [undefined,null,false,true,NaN,-Infinity,Infinity,-5,-0,0,.1,.9,1,1.9,3.9,8,4294967296,9007199254740992,'','2',' 3.8 ','0x3','0b10','-1','Infinity','wat',[2],[],{}, {valueOf:0,toString:0}]){
 for(const shape of ['empty','slots','inherited','undefined']){
  const parent={0:'parent',2:[7,8]},o=shape==='inherited'?Object.create(parent):{};o.length=length;
  if(shape==='slots'){o[0]='zero';o[2]={shared:true};}if(shape==='undefined')o[0]=undefined;
  ;
  add('length-'+shape,o);
 }
}
for(const length of [0,1,4,8])for(let mask=0;mask<16;mask++)for(const proto of [false,true]){
 const a=new Array(length),parent=Object.create(Array.prototype);;
 if(proto){parent[0]='parent';parent[3]=[1,2];Object.setPrototypeOf(a,parent);;}
 for(let i=0;i<length;i++)if(mask&(1<<(i%4))){const value=i%3===0?undefined:i%3===1?'slot-'+i:{index:i};a[i]=value;;}
 add('sparse',a);
}
const codeunits=[0,1,0x61,0x301,0xd7ff,0xd800,0xdbff,0xdc00,0xdfff,0xe000,0xffff];
for(const a of codeunits)for(const b of codeunits)add('string-units',String.fromCharCode(a,b));
for(let i=0;i<40;i++){const a=String.fromCodePoint(0x10000+i*25517);add('string-supplementary','A'+a+'e\u0301'+a);}
const intrinsics={constructor:Object,__defineGetter__:Object.prototype.__defineGetter__,__defineSetter__:Object.prototype.__defineSetter__,hasOwnProperty:Object.prototype.hasOwnProperty,lookupGetter:Object.prototype.__lookupGetter__,lookupSetter:Object.prototype.__lookupSetter__,isPrototypeOf:Object.prototype.isPrototypeOf,propertyIsEnumerable:Object.prototype.propertyIsEnumerable,toString:Object.prototype.toString,valueOf:Object.prototype.valueOf,toLocaleString:Object.prototype.toLocaleString,prototype:Object.prototype,arrayConstructor:Array,arrayToString:Array.prototype.toString,arrayJoin:Array.prototype.join,arrayPrototype:Array.prototype,numberPrototype:Number.prototype,numberConstructor:Number,numberToString:Number.prototype.toString,numberValueOf:Number.prototype.valueOf,functionPrototype:Function.prototype,functionConstructor:Function,functionCall:Function.prototype.call,functionApply:Function.prototype.apply,functionBind:Function.prototype.bind,functionToString:Function.prototype.toString};
;
for(const [key,value] of Object.entries(intrinsics))add('intrinsic-'+key,value);
const shared={x:7},input=[shared,,shared],sourceIdentity=witness(input);assert.equal(sourceIdentity[0],shared);assert.equal(sourceIdentity[2],shared);
input[0]=false;input.length=0;assert.equal(sourceIdentity.length,3);assert.equal(sourceIdentity[0],shared);shared.x=99;assert.equal(sourceIdentity[2].x,99);

const report=({rows});assert.ok(process.argv.includes('--check'),'Source-only owner supports --check');const gold=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p60-argument-expansion-source.json'),'utf8'));function diff(a,b,p=''){if(Object.is(a,b))return null;if(a===null||b===null||typeof a!=='object'||typeof b!=='object')return {path:p,actual:a,expected:b};const ak=Object.keys(a),bk=Object.keys(b);if(ak.length!==bk.length)return{path:p,actualKeys:ak,expectedKeys:bk};for(const k of ak){const d=diff(a[k],b[k],p+'/'+k);if(d)return d;}return null;}assert.equal(JSON.stringify(diff(report,gold)), 'null');console.log(JSON.stringify({profile:'p60-argument-expansion-source.json',cases:report.cases?.length??report.rows?.length,mode:'check'}));
