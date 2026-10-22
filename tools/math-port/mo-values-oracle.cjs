// Root actual-CJS observations of raw operator definition value branches.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,M}=require('./mo-pattern-reference.cjs');
const {MmlFactory}=require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlFactory.js'));
class Mo extends M{table(s){this.checkOperatorTable(s);}op(s){return this.getOperatorDef(s);}}
let assertions=0,cases=[],currentCase;function value(x){if(x===undefined)return {type:'missing'};if(x===null)return {type:'null'};if(typeof x==='boolean')return {type:'boolean',value:x};if(typeof x==='number')return {type:'number',value:Number.isNaN(x)?'NaN':Object.is(x,-0)?'-0':x===Infinity?'Infinity':x};if(typeof x==='string')return {type:'string',units:Array.from({length:x.length},(_,i)=>x.charCodeAt(i))};if(Array.isArray(x))return {type:'array',items:x.map(value)};if(typeof x==='object')return {type:'object',entries:Object.keys(x).map(k=>[k,value(x[k])])};throw Error('Unbound observation');}
function observe(o){currentCase.observations.push(o);assertions++;}function pp(v){observe({kind:'value',expected:value(v)});}function begin(n){currentCase={id:'math_port_mo_value_'+n,observations:[]};cases.push(currentCase);}function end(){}
function sourceNode(){const f=new MmlFactory();f.setNodeClass('mo',Mo);return f.create('mo',{},[f.create('text').setText('!')]);}
for(const [name,value]of [['string','😀xy'],['number',17],['boolean',true],['array',['x','y']],['null',null],['undefined',undefined],['false',false],['zero',0]]){
 begin('definition_properties_'+name);const old=M.OPTABLE,n=sourceNode();M.OPTABLE={infix:{'!':[9,8,6,value]},prefix:{},postfix:{}};n.table('!');;
 for(const k of Object.keys(value||{}))pp(n.attributes.getInherited(k));observe({kind:'keys',value:n.attributes.getInheritedNames()});pp(n._texClass);pp(n.lspace);pp(n.rspace);M.OPTABLE=old;end();
}
for(const [name,table]of [['null',null],['undefined',undefined],['missing-form',{}],['null-form',{infix:null}],['false-form',{infix:false}],['number-form',{infix:3}]]){
 begin('table_failure_'+name);const old=M.OPTABLE,n=sourceNode();M.OPTABLE=table;let error;try{n.table('!');}catch(e){error=e;};assert.ok(error,'Expected failure must originate from actual CJS');observe({kind:'error',name:error.name});pp(n.attributes.getInherited('form'));pp(n.getProperty('noDictDef'));pp(n._texClass);pp(n.lspace);pp(n.rspace);M.OPTABLE=old;end();
}

const profile=({cases,assertions});const frozen=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p24-mo-values-source.json'),'utf8'));assert.deepEqual(profile,frozen);assert.ok(process.argv.includes('--check'),'Source-only owner supports --check');console.log(JSON.stringify({profile:'p24-mo-values-source.json',cases:profile.cases?.length??profile.cases,assertions:profile.assertions??null,mode:'check'}));
