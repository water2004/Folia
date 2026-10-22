// Root actual-CJS observations. Native bindings test only the shared call seam.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const pins={
 'input/tex/TokenMap':'9e3f086080a6ab052b68b22a5d9ee3dbc5e17d7b0c03a31e789664b01df36cf7',
 'input/tex/MapHandler':'d82e3034776d2392689aad86500b5e60dbe0aa5540853b48f308841879f3cf67',
 'util/FunctionList':'571f57801b863b2d9f01a66dc72f2836b7fd06a1a48a3fe0057e4ec3d51a7278'};
for(const [name,hash]of Object.entries(pins))assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(base,'cjs',name+'.js'))).digest('hex'),hash);
const {CharacterMap,MacroMap,CommandMap,EnvironmentMap}=require(path.join(base,'cjs/input/tex/TokenMap.js'));
const {SubHandler}=require(path.join(base,'cjs/input/tex/MapHandler.js'));
const {FunctionList}=require(path.join(base,'cjs/util/FunctionList.js'));
const observations=[];let count=0;
const add=(name,flags)=>{assert.ok(flags.every(v=>typeof v==='boolean'));observations.push({name,flags});count+=flags.length;};
for(const kind of ['character','macro','command']){
 const parser={currentCS:'saved'},box={value:7},arg=[false],flags=[];let map;
 function callback(p,atom,...rest){flags.push(this===undefined,p===parser,atom===(kind==='character'?map.lookup('x'):kind==='command'?'\\x':'x'),rest.length===(kind==='character'?0:2));if(kind!=='character')flags.push(rest[0]===box,rest[1]===arg);return false;}
 if(kind==='character')map=new CharacterMap('p19b2-'+kind,callback,{x:'y'});
 else map=new (kind==='macro'?MacroMap:CommandMap)('p19b2-'+kind,{x:[callback,box,arg]});
 map.parse([parser,'x']);add(kind,flags);
}
{
 const parser={},args=[false],envParser=function(){},flags=[];let map,saved;
 function callback(p,atom,f,a){saved=this;flags.push(this===map,p===parser,atom==='x',f===envParser,a===map.lookup('x').args);return args;}
 map=new EnvironmentMap('p19b2-env',callback,{x:[envParser,false]});
 flags.push(map.parse([parser,'x'])===args,saved===map);
 map.parser=function(p,atom,f,a){flags.push(this===saved,p===parser,f===envParser,a===map.lookup('x').args);return false;};
 flags.push(map.parse([parser,'x'])===false);add('environment',flags);
}
{
 const parser={},flags=[],handler=new SubHandler();let saved,record;
 function callback(p,atom){saved=this;flags.push(this===record,this.item===callback,this.priority===13,p===parser,atom==='x');handler.remove([],callback);this.item=other;this.priority=81;return false;}
 function other(){}
 const map=new MacroMap('p19b2-shared',{x:callback});handler.add([],callback,13);record=Array.from(handler._fallback)[0];
 flags.push(map.lookup('x').func===record.item);handler.parse([parser,'x']);
 flags.push(saved===record,saved.item===other,saved.priority===81,Array.from(handler._fallback).length===0);add('fallback',flags);
}
{
 const list=new FunctionList(),flags=[],parser={},token={},rest={},second=function(){flags.push(false);return true;};let record;
 function callback(p,a,r){flags.push(this===record,this.item===callback,p===parser,a===token,r===rest);return false;}
 list.add(callback,5);list.add(second,10);record=Array.from(list)[0];flags.push(list.execute(parser,token,rest)===false);add('typed_false',flags);
}
{
 const reason={value:7},parser={},flags=[];function callback(){throw reason;}
 const map=new MacroMap('p19b2-throw',{x:callback});try{map.parse([parser,'x']);flags.push(false);}catch(e){flags.push(e===reason);}
 const list=new FunctionList();list.add(callback);try{list.execute(parser,'x');flags.push(false);}catch(e){flags.push(e===reason);}add('throw',flags);
}
{
 let typedCount=0,genericCount=0;const flags=[],parser={};function typed(){return ++typedCount;}
 const map=new MacroMap('p19b2-closure',{x:typed});flags.push(map.parse([parser,'x'])===1,map.parse([parser,'x'])===2);
 const list=new FunctionList();list.add(function(){return ++genericCount;});list.execute();flags.push(genericCount===1);list.execute();flags.push(genericCount===2);add('closure_state',flags);
}

const file=path.join(root,'docs/math-port/evidence/callable-receiver-source.json');
const frozen=JSON.parse(fs.readFileSync(file,'utf8'));assert.deepStrictEqual(pins,frozen.source.pins);assert.deepStrictEqual(observations,frozen.observations);assert.equal(observations.length,8);assert.equal(count,50);
console.log(JSON.stringify({sourceCases:observations.length,sourceAssertions:count,profileSha256:crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex'),mode:'check'}));
