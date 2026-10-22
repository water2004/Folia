// Root-owned expected values from pristine pinned CJS, never native output.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');

// Normal locked Source load. operator-data.cjs remains the independent production-table generator.
const crypto=require('node:crypto');
function findRoot(){let p=process.env.FOLIA_MATH_ROOT||__dirname;for(;;){if(fs.existsSync(path.join(p,'docs/math-port/evidence/p11-source-lock.json')))return p;const next=path.dirname(p);if(next===p)throw Error('Repository root not found');p=next;}}
const root=findRoot(),digest=x=>crypto.createHash('sha256').update(x).digest('hex');
const lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p11-source-lock.json'),'utf8'));
assert.equal(process.version,lock.node);assert.equal(lock.source,'ts/core/MmlTree/OperatorDictionary.ts');
assert.equal(lock.sha256,'9afac6e3a8f3b778247a5a678c529b9edddb8172850df7384511bf61dfbffd37');
assert.equal(lock.runtime_sha256,'db65e13a8f3c6290e711303d46cfb6f2fe8ec24e12c499bdf9b1bf4f42027ff8');
const npm=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src'),source=fs.readFileSync(path.join(root,'.reference/MathJax-src',lock.source));
assert.equal(digest(source),lock.sha256);assert.equal(fs.readFileSync(path.join(npm,lock.source),'utf8').replaceAll('\r\n','\n'),source.toString().replaceAll('\r\n','\n'));
const resolved=require.resolve('@mathjax/src/js/'+lock.runtime.replace(/^cjs\//,''),{paths:[path.join(root,'tools/mathjax-bundle')]});
assert.equal(fs.realpathSync(resolved),fs.realpathSync(path.join(npm,lock.runtime)));assert.equal(digest(fs.readFileSync(resolved)),lock.runtime_sha256);
const D=require(resolved),nodes=[],ids=new Map();
function collect(v){if(v===null||typeof v!=='object'||ids.has(v))return;ids.set(v,nodes.length);nodes.push(v);for(const key of Object.keys(v))collect(v[key]);}
for(const v of [D.MO,D.RANGES,D.MMLSPACING,D.OPTABLE])collect(v);
function encode(v){if(v===undefined)return {$type:'undefined'};if(typeof v==='number'){if(Number.isNaN(v))return {$type:'number',value:'NaN'};if(Object.is(v,-0))return {$type:'number',value:'-0'};if(!Number.isFinite(v))return {$type:'number',value:String(v)};}if(v&&typeof v==='object'){if(ids.has(v))return {$ref:ids.get(v)};if(Array.isArray(v))return v.map(encode);throw Error('Unbound Source observation');}return v;}
const records=[];let current;
function record(v){current.observations.push(encode(v));}
// Runs contain scalar coordinates; they are distinct from dictionary object identities.
function recordRuns(runs){current.observations.push(runs.map(r=>r.slice()));}






function eq(v){record(v);}
function pp(v){record(v);}
function keys(v){current.observations.push(v.slice());}
function begin(n){current={name:'math_port_operator_'+n,observations:[]};records.push(current);}function end(){}
function range(v){eq(v.length);for(let i=0;i<v.length;i++)pp(v[i]);}
begin('entire_pristine_identity_graph');eq(nodes.length);
for(let i=0;i<nodes.length;i++){const v=nodes[i];eq(Array.isArray(v));eq(Object.getPrototypeOf(v)===Array.prototype);eq(Object.getPrototypeOf(v)===Object.prototype);if(Array.isArray(v))eq(v.length);keys(Object.keys(v));for(const key of Object.keys(v))pp(v[key]);}end();
begin('every_operator_key_range_lookup_and_freshness');
for(const table of Object.values(D.OPTABLE))for(const key of Object.keys(table)){const value=D.getRange(key);range(value);eq(D.getRange(key)!==D.getRange(key));}end();
begin('OPDEF_missing_null_fresh_and_property_identity');
for(const tc of [undefined,null,false,0,'2'])for(const props of [undefined,null,false,0,'data']){const v=D.OPDEF('l',null,tc,props);range(v);eq(v!==D.OPDEF('l',null,tc,props));}
{const props={accent:true},v=D.OPDEF(1,2,undefined,props);eq(v[3]===props);}end();
begin('inherited_names_empty_multichar_surrogates');
for(const text of ['', 'constructor','toString','__proto__','hasOwnProperty','valueOf','<=','!=','!!','\u2a7d\u0338','\ud800','\udfff','😀','字tail','+tail']){range(D.getRange(text));}end();
begin('every_range_boundary_with_pristine_tables');
for(const cp of new Set(D.RANGES.flatMap(row=>[row[0]-1,row[0],row[0]+1,row[1]-1,row[1],row[1]+1]).filter(cp=>cp>=0&&cp<=0x10ffff))){const text=String.fromCodePoint(cp);range(D.getRange(text));}end();
const forms={...D.OPTABLE},savedRanges=[...D.RANGES];
function reset(){Object.assign(D.OPTABLE,forms);D.RANGES.splice(0,D.RANGES.length,...savedRanges);}
begin('entire_unicode_range_domain_without_operator_tables');

Object.assign(D.OPTABLE,{infix:{},prefix:{},postfix:{}});
const runs=[];for(let cp=0;cp<=0x10ffff;cp++){const row=D.getRange(String.fromCodePoint(cp)),id=D.RANGES.indexOf(row);if(!runs.length||runs.at(-1)[2]!==id)runs.push([cp,cp,id]);else runs.at(-1)[1]=cp;}


recordRuns(runs);end();reset();
begin('table_priority_raw_truthy_and_live_aliases');
for(const [a,b,c] of [[null,[0,0,5],[0,0,6]],[false,0,[0,0,6]],[[0,0,0],[0,0,5],[0,0,6]],['xyz',[0,0,5],[0,0,6]],[true,0,0],[1,0,0]]){

  Object.assign(D.OPTABLE,{infix:{X:a},prefix:{X:b},postfix:{X:c}});
  range(D.getRange('X'));
}reset();

// Mutate the exported named definition: all table aliases must observe it.
const shared=D.MO.REL,oldClass=shared[2];shared[2]='mutated';

for(const [form,table] of Object.entries(D.OPTABLE))for(const key of Object.keys(table))if(table[key]===shared){range(D.getRange(key));}
shared[2]=oldClass;end();
begin('malformed_tables_short_circuit_and_range_rows');
for(const [infix,prefix,postfix] of [[null,{},{}],[{},null,{}],[{X:[0,0,2]},null,null],[{}, {},null]]){
  Object.assign(D.OPTABLE,{infix,prefix,postfix});let value,error;try{value=D.getRange('X');}catch(e){error=e.name;}


  if(!error)range(value);eq(!!error);
}reset();Object.assign(D.OPTABLE,{infix:{},prefix:{},postfix:{}});
const rowCases=[[[100,200,2,'mo'],[0,300,1,'mi']],[[0,50,2,'mo'],[0,300,1,'mi']],[null],['abc'],[{0:0,1:100,2:7,3:'custom'}],[[0,100,2,'mo'],null]];
for(const rows of rowCases){D.RANGES.splice(0,D.RANGES.length,...rows);let value,error;try{value=D.getRange('X');}catch(e){error=e.name;}


  if(!error){if(rows.includes(value))eq(true);else range(value);}eq(!!error);
}end();reset();

function check(){const name='operator-source-profile.json',file=path.join(root,'docs/math-port/evidence',name),bytes=fs.readFileSync(file);
 assert.equal(digest(bytes),'c5f2913f724b4a987fec5f0036a363670fb8a9faae883bd8a1ada30e3e6e09d3','Frozen operator Source profile bytes changed');const expected=JSON.parse(bytes);assert.deepEqual(records,expected.records);
 console.log(JSON.stringify({mode:'check',cases:records.length,observations:records.reduce((n,r)=>n+r.observations.length,0),unicodeCodePoints:0x110000,profileSha256:digest(bytes)}));}
module.exports={records,check};if(require.main===module)check();
