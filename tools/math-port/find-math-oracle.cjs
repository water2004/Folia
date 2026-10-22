// Root actual original FindMath constructor observations, no finder substitute.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=(process.env.FOLIA_MATH_ROOT||path.resolve(__dirname,'../..')),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src'),lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p46-parser-options-source.json'))),hashes=[];
for(const source of ['ts/core/FindMath.ts','ts/util/Options.ts']){const entry=lock.hashes.find(x=>x.source===source);assert.ok(entry,source);const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(base,source.replace(/^ts\//,'cjs/').replace(/\.ts$/,'.js')))).digest('hex'),entry.cjs_sha256);hashes.push(entry);}
const {AbstractFindMath}=require(path.join(base,'cjs/core/FindMath.js')),rows=[],val=v=>v===undefined?'U':v===null?'N':typeof v==='object'?'object':String(v);
function fixture(){class Probe extends AbstractFindMath{}Probe.OPTIONS={marker:1,record:{a:1},array:[1]};const e={Class:Probe,trace:[],captured:null,make(options){return new Probe(options);},capture(){Object.defineProperty(Probe.prototype,'constructor',{get(){e.captured=this;return Probe;}});}};return e;}
function add(label,run){const e=fixture();let result='THREW',error='ok';try{result=run(e);}catch(x){error=x.name;}rows.push({label,expected:error+'|'+result+'|'+e.trace.join('/')});}
add('initial-options',e=>{const j=e.make();return j.options.marker+':'+j.options.record.a;});
add('fresh-options-and-containers',e=>{const a=e.make(),b=e.make();return[a.options!==b.options,a.options.record!==b.options.record,a.options.array!==b.options.array].join(':');});
add('actual-user-options-merge',e=>{const j=e.make({marker:9,record:{a:7},array:[4,5]});return j.options.marker+':'+j.options.record.a+':'+j.options.array[1];});
for(const[tag,v]of [['undefined',undefined],['null',null],['false',false],['zero',0]])add('provided-parameter:'+tag,e=>String(e.make(v).options.marker));
add('captured-class-options',e=>{const other={OPTIONS:{marker:2}};Object.defineProperty(e.Class.prototype,'constructor',{get(){e.trace.push('constructor');Object.defineProperty(this,'constructor',{value:other});return e.Class;}});return String(e.make().options.marker);});
add('class-options-getter-before-publication',e=>{e.capture();const defaults=e.Class.OPTIONS;Object.defineProperty(e.Class,'OPTIONS',{get(){e.trace.push('options:'+val(e.captured.options));return defaults;}});return String(e.make().options.marker);});
add('merge-getter-throw-keeps-options-absent',e=>{e.capture();const user={get marker(){throw Error('user');}};try{e.make(user);}catch(x){return val(e.captured.options);}return'bad';});
add('inherited-original-empty-defaults',e=>{delete e.Class.OPTIONS;const a=e.make(),b=e.make();return Object.keys(a.options).length+':'+String(a.options!==b.options);});

// Frozen original Source JSON remains byte-identical; only renderer-only cpp fields are projected.
assert.ok(process.argv.includes('--check'),'Source-only oracle supports read-only --check');
const frozenProfilePath=path.join(root,"docs/math-port/evidence/p49b-find-math-source.json");
const frozenBytes=fs.readFileSync(frozenProfilePath);
function sourceFields(value){if(Array.isArray(value))return value.map(sourceFields);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([key])=>key!=='cpp').map(([key,v])=>[key,sourceFields(v)]));return value;}
const actualSourceProfile={stage:'actual original FindMath base only, not FindTeX',hashes,rows};
assert.deepStrictEqual(sourceFields(actualSourceProfile),sourceFields(JSON.parse(frozenBytes)),'Original Source inputs/expectations/pins changed');
console.log(JSON.stringify({sourceCases:rows.length,profile:"docs/math-port/evidence/p49b-find-math-source.json",profileSha256:crypto.createHash('sha256').update(frozenBytes).digest('hex'),mode:'check'}));
