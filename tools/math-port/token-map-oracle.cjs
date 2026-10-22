// Root actual locked Source control/data observations, never native output.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
assert.equal(process.version,'v24.13.0');const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs'),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
for(const[file,pin]of Object.entries({'input/tex/TokenMap.js':'9e3f086080a6ab052b68b22a5d9ee3dbc5e17d7b0c03a31e789664b01df36cf7','input/tex/MapHandler.js':'d82e3034776d2392689aad86500b5e60dbe0aa5540853b48f308841879f3cf67','input/tex/Token.js':'d317b026fc1ae0ff93d6939a15041d225385a826f8c33115284464bc2f527e35'}))assert.equal(hash(fs.readFileSync(path.join(base,file))),pin);
const t=require(path.join(base,'input/tex/TokenMap.js')),{Token,Macro}=require(path.join(base,'input/tex/Token.js')),{MapHandler,SubHandler}=require(path.join(base,'input/tex/MapHandler.js'));
const ids=new Map(),hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
function label(v){if(v===undefined)return 'undefined';if(v===null)return 'null';if(typeof v==='string')return 'str:'+hex(v);if(typeof v==='boolean')return v?'true':'false';if(typeof v==='number')return Number.isNaN(v)?'NaN':Object.is(v,-0)?'-0':'num:'+v;if(ids.has(v))return ids.get(v);throw Error('Unknown observation value');}
const rows=[],groups={};function group(name,fn){groups[name]=fn();}
let serial=0;const name=p=>'p22b-'+p+'-'+serial++;
const f=function(){},object={},array=[],symbol=Symbol('result');ids.set(f,'function');ids.set(object,'object');ids.set(array,'array');ids.set(symbol,'symbol');
const values=[undefined,null,false,true,0,-0,NaN,7,'','x',object,array,f,symbol];
group('parse_result',()=>values.map(v=>Object.is(t.parseResult(v),v===undefined?true:v)));
const token=new Token('token','T'),macro=new Macro('macro',f,[17]);ids.set(token,'token');ids.set(macro,'macro');
const mapped=[undefined,null,false,0,'',NaN,7,'x',object,array,token,macro,f];
// A controlled subclass supplies original abstract obligations; concrete base
// parserFor/parse and all getter/body timings execute original Source methods.
for(let method=0;method<8;method++)for(let map=0;map<mapped.length;map++)for(const contains of [false,true]){
 const events=[],resultIndex=(method*13+map)%values.length;let body;
 body=function(env,atom){events.push('body:'+label(atom));events.push(this===undefined?'bare':'wrong-receiver');return values[resultIndex];};ids.set(body,'body');
 const methods=[undefined,null,false,0,'',true,3,body];
 class Base extends t.AbstractTokenMap{contains(){events.push('contains');return contains;}lookup(){events.push('lookup');return mapped[map];}get parser(){events.push('parser');return super.parser;}set parser(v){super.parser=v;}}
 const m=new Base(name('base'),methods[method]);let result,error='';try{result=m.parse([{},'x']);}catch(e){error=e.name;}
 rows.push({method,map,contains,result_index:resultIndex,result:error?'':label(result),error,events});
}
group('registry_constructor',()=>{
 const observations=[];let observed,registered=false;
 class Inspect extends t.CharacterMap{get name(){observed=this;observations.push(this.parser===f,this.map===undefined);try{this.contains('x');observations.push(false);}catch(e){observations.push(e.name==='TypeError');}return super.name;}add(k,v){observations.push(MapHandler.getMap(super.name)===this,this.map instanceof Map);return super.add(k,v);}}
 const key=name('registration'),m=new Inspect(key,f,{x:'X'});registered=MapHandler.getMap(key)===m;observations.push(observed===m,registered,m.contains('x'));
 const old=new t.CharacterMap(key,f,{old:'O'}),next=new t.CharacterMap(key,f,{next:'N'});observations.push(MapHandler.getMap(key)===next,old!==next);
 const bad=name('partial');try{new t.CharacterMap(bad,f,null);observations.push(false);}catch(e){observations.push(e.name==='TypeError');}const partial=MapHandler.getMap(bad);observations.push(partial instanceof t.CharacterMap,partial.name===bad,!partial.contains('x'));
 let regexSeen;class InspectRegex extends t.RegExpMap{get name(){regexSeen=this;observations.push(this._regExp===undefined);try{this.contains('x');observations.push(false);}catch(e){observations.push(e.name==='TypeError');}return super.name;}}
 const regex={test(){return true;}},r=new InspectRegex(name('regex-registry'),f,regex);observations.push(regexSeen===r,r._regExp===regex,r.contains('x'));
 return observations;
});
group('ordinary_map',()=>{
 class Map extends t.AbstractParseMap{}const m=new Map(name('map'),f),key={},other={},fn=function(){};let flags=[];
 for(const k of [NaN,0,-0,undefined,null,false,7,'7',key,other,fn]){m.add(k,undefined);flags.push(m.contains(k),m.lookup(k)===undefined);m.add(k,object);flags.push(m.lookup(k)===object);m.remove(k);flags.push(!m.contains(k),m.lookup(k)===undefined);}
 m.add('a',token);m.add('b',macro);m.add('a',array);flags.push(Array.from(m.map.keys()).join(',')==='a,b',m.lookup('a')===array,m.lookup('b')===macro);m.remove('a');m.add('a',token);flags.push(Array.from(m.map.keys()).join(',')==='b,a');m.add(-0,7);flags.push(Object.is(Array.from(m.map.keys()).at(-1),0));return flags;
});
group('character_data',()=>{
 const attrs={weight:'bold'},a=['A',attrs,'unused'],proto={};Object.setPrototypeOf(proto,Array.prototype);proto[0]='Inherited';proto[1]=attrs;const sparse=new Array(2);Object.setPrototypeOf(sparse,proto);
 const short=new Array(1);Object.setPrototypeOf(short,proto);const empty=[];Object.setPrototypeOf(empty,proto);
 const json={'9':'nine','2':a,x:sparse,short,empty,number:7,like:{0:'Like',1:attrs},'\\left':'L'},keys=[];
 class Characters extends t.CharacterMap{add(k,v){keys.push(k);return super.add(k,v);}}const m=new Characters(name('characters'),f,json);const flags=[keys.join(',')==='2,9,x,short,empty,number,like,\\left'];
 for(const[k,char,attribute]of [['2','A',attrs],['9','nine',null],['x','Inherited',attrs],['short','Inherited',undefined],['empty',undefined,undefined],['number',undefined,undefined],['like','Like',attrs],['\\left','L',null]]){const v=m.lookup(k);flags.push(v instanceof Token,v.token===k,v.char===char,v.attributes===attribute,m.lookup(k)===v);}
 const numeric=m.lookup('2');flags.push(m.contains('2'),!m.contains(2),m.parserFor('2')===f,m.parserFor('absent')===null);m.parser=null;flags.push(m.parser===null,m.parserFor('2')===null,m.lookup('2')===numeric);return flags;
});
group('constructor_enumeration',()=>{
 const flags=[],charJson={a:'A',b:'B'},macroJson={a:f,b:f},g=function(){};
 class Char extends t.CharacterMap{add(k,v){if(k==='a'){charJson.b='Changed';charJson.c='C';}return super.add(k,v);}}
 const c=new Char(name('char-enumeration'),f,charJson);flags.push(c.lookup('b').char==='Changed',!c.contains('c'));
 class Mac extends t.MacroMap{add(k,v){if(k==='a'){macroJson.b=g;macroJson.c=g;}return super.add(k,v);}}
 const m=new Mac(name('macro-enumeration'),macroJson);flags.push(m.lookup('b').func===f,!m.contains('c'));
 const chars=new t.CharacterMap(name('string-keys'),f,'😀');flags.push(chars.lookup('0').char==='\uD83D',chars.lookup('1').char==='\uDE00');return flags;
});
group('macro_data',()=>{
 const g=function(){},a=[f,undefined,object],proto=Object.create(Array.prototype);proto[1]=array;const sparse=new Array(3);Object.setPrototypeOf(sparse,proto);sparse[0]=f;
 const functions={named:f},json={'9':g,'2':a,direct:f,byName:['named',7],missing:'absent',sparse};const keys=[];
 class Maps extends t.MacroMap{add(k,v){keys.push(k);return super.add(k,v);}}const m=new Maps(name('macro-data'),json,functions),s=m.lookup('sparse');
 const flags=[keys.join(',')==='2,9,direct,byName,missing,sparse',m.parser===null,m.lookup('2').func===f,m.lookup('2').args!==a,m.lookup('2').args[0]===undefined,Object.hasOwn(m.lookup('2').args,0),m.lookup('2').args[1]===object,m.lookup('direct').func===f,m.lookup('direct').args.length===0,m.lookup('byName').func===f,m.lookup('byName').args[0]===7,m.lookup('missing').func===undefined,m.parserFor('missing')===undefined,m.parserFor('absent')===null,s.args.length===2,s.args[0]===array,Object.hasOwn(s.args,0),!Object.hasOwn(s.args,1)];
 const inherited=Object.create({named:g}),n=new t.MacroMap(name('inherited-method'),{x:'named'},inherited);flags.push(n.lookup('x').func===g);const bad=name('null-methods');try{new t.MacroMap(bad,{good:f,bad:'named'},null);flags.push(false);}catch(e){flags.push(e.name==='TypeError');}const partial=MapHandler.getMap(bad);flags.push(partial.contains('good'),!partial.contains('bad'));return flags;
});
group('macro_dispatch',()=>{
 const flags=[],events=[],env={currentCS:'saved'},args=[object,array];let called;
 const one=function(e,key,...rest){events.push('first-body');called=[this,e,key,rest];return undefined;},two=function(e,key,...rest){events.push('second-body');called=[this,e,key,rest];return false;};
 const a=new Macro('first',one,args),b=new Macro('second',two,[99]);let lookups=0;
 class Maps extends t.MacroMap{lookup(k){events.push('lookup');return lookups++%2===0?a:b;}}
 const m=new Maps(name('dispatch'),{});const r=m.parse([env,'x']);flags.push(r===false,called[0]===undefined,called[1]===env,called[2]==='first',called[3][0]===object,called[3][1]===array,events.join(',')==='lookup,lookup,second-body');
 const n=new t.MacroMap(name('live-args'),{x:[one,1]});n.lookup('x').args.push(object);n.parse([env,'x']);flags.push(called[3].length===2,called[3][1]===object);n.remove('x');flags.push(n.parse([env,'x'])===null);return flags;
});
group('delimiter_dispatch',()=>{
 const flags=[],env={},calls=[];const parser=function(e,tok){calls.push([this,e,tok]);return false;};const m=new t.DelimiterMap(name('delimiter'),parser,{'\\x':'X','\\null':'N'});flags.push(m.parse([env,'x'])===false,calls[0][0]===undefined,calls[0][1]===env,calls[0][2]===m.lookup('\\x'),m.parse([env,'absent'])===null,m.parse([env,null])===false);return flags;
});
group('command_dispatch',()=>{
 const flags=[],events=[];let current='saved',thrown={id:'failure'};const env={get currentCS(){events.push('get-cs');return current;},set currentCS(v){events.push('set-cs:'+v);current=v;}};
 let observed;const parser=function(e,key,...rest){events.push('body');observed=[this,e,key,rest,current];return undefined;};const m=new t.CommandMap(name('command'),{x:[parser,object]});flags.push(m.parse([env,'x'])===true,observed[0]===undefined,observed[1]===env,observed[2]==='\\x',observed[3][0]===object,observed[4]==='\\x',current==='saved',events.join(',')==='get-cs,set-cs:\\x,body,set-cs:saved');
 const bad=function(){throw thrown;},n=new t.CommandMap(name('command-error'),{bad});try{n.parse([env,'bad']);flags.push(false);}catch(e){flags.push(e===thrown);}flags.push(current==='\\bad');
 const args=new Macro('arg-error',parser,null);class Args extends t.CommandMap{lookup(){return args;}}const a=new Args(name('command-args-error'),{});try{a.parse([env,'args']);flags.push(false);}catch(e){flags.push(e.name==='TypeError');}flags.push(current==='\\args');return flags;
});
group('environment_dispatch',()=>{
 const flags=[],events=[],env={},specific=function(){};let called,m;
 const general=function(e,key,method,args){events.push('body');called=[this,e,key,method,args];return false;};
 class Env extends t.EnvironmentMap{get parser(){events.push('get-parser');return super.parser;}set parser(v){events.push('set-parser');super.parser=v;}get name(){flags.push(super.parser===null);return super.name;}}
 m=new Env(name('environment'),general,{x:[specific,object]});flags.push(events.join(',')==='set-parser',m.parse([env,'x'])===false,events.join(',')==='set-parser,get-parser,body',called[0]===m,called[1]===env,called[2]==='x',called[3]===specific,called[4]===m.lookup('x').args,called[4][0]===object,m.parse([env,'absent'])===null,MapHandler.getMap(m.name)===m);return flags;
});
const regexRows=[];for(const sequence of [[true,true],[true,false],[false,true],[false,false]]){
 const events=[],regex={index:0,test(v){events.push('test:'+label(v));events.push(this===regex?'regex-receiver':'wrong');return sequence[this.index++%sequence.length];}};
 const parser=function(e,k){events.push('body:'+label(k));events.push(this===undefined?'bare':'wrong');return undefined;};const m=new t.RegExpMap(name('regex'),parser,regex);let result=m.parse([{},'x']);regexRows.push({sequence,result:label(result),events,calls:regex.index});
}
group('regexp_live_service_dependency',()=>{
 // Controlled dependency; this deliberately tests no regular-expression engine.
 const flags=[],regexp={lastIndex:0,test(){const matched=this.lastIndex===0;this.lastIndex=matched?1:0;return matched;}},m=new t.RegExpMap(name('live-regexp-service'),f,regexp);flags.push(m.lookup('x')==='x',regexp.lastIndex===1,m.lookup('x')===null,regexp.lastIndex===0,m.contains('x'),regexp.lastIndex===1);regexp.lastIndex=0;flags.push(m.parse([{},'x'])===null,regexp.lastIndex===0);return flags;
});
group('new_lookup_bridge',()=>{
 const flags=[],macro=new Macro('x',f,[]);class Characters extends t.CharacterMap{contains(){return true;}lookup(){return macro;}}const m=new Characters(name('handler-macro'),f,{}),h=new SubHandler();h.add([m.name]);flags.push(h.contains('x'));m.lookup=()=>null;try{h.contains('x');flags.push(false);}catch(e){flags.push(e.name==='TypeError');}return flags;
});
const report={node:process.version,scope:'complete TokenMap Source methods under frozen native data/controlled dependency profile; engine separate',rows,regex_rows:regexRows,groups};
const data=JSON.stringify(report,null,2)+'\n',target=path.join(root,'docs/math-port/evidence/p22b-token-map-source.json');assert.ok(process.argv.includes('--check'));assert.equal(fs.readFileSync(target,'utf8').replaceAll('\r\n','\n'),data);
console.log(JSON.stringify({base_cases:rows.length,regex_cases:regexRows.length,groups:Object.keys(groups).length,flags:Object.values(groups).flat().length,mode:'check'}));
