// Root-owned expected observations from actual pinned HtmlNode/XML/token methods.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,MmlFactory,HtmlNode}=require('./html-node-reference.cjs');
const sourcePinPaths=['core/MmlTree/MmlNodes/HtmlNode','core/MmlTree/MmlNode','core/MmlTree/MmlFactory','core/MmlTree/MML',...['mi','mo','mstyle','mtable','mtr','mathchoice','mfenced','menclose','mpadded','maligngroup','malignmark','mglyph','mfrac','msqrt','mroot'].map(n=>'core/MmlTree/MmlNodes/'+n)];
function pins(){const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));return sourcePinPaths.map(name=>{const source='ts/'+name+'.ts',entry=scope.files.find(f=>f.source===source);assert.ok(entry,'missing pinned source '+source);const original=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(require('node:crypto').createHash('sha256').update(original).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',source),'utf8').replaceAll('\r\n','\n'),original.toString().replaceAll('\r\n','\n'));return {source,sha256:entry.source_sha256};});}
function encode(v){if(v===undefined)return ['$undefined'];if(v===null)return null;if(typeof v==='number'){if(Number.isNaN(v))return ['$number','NaN'];if(v===Infinity)return ['$number','Infinity'];if(v===-Infinity)return ['$number','-Infinity'];if(Object.is(v,-0))return ['$number','-0'];return v;}if(typeof v==='string'||typeof v==='boolean')return v;if(Array.isArray(v))return ['$array',v.map(encode)];if(typeof v==='object')return ['$object',Object.keys(v).map(k=>[k,encode(v[k])])];throw new TypeError('Unsupported Source value type '+typeof v);}
function collectGraphValues(values){const unique=new Set();function visit(value){const key=JSON.stringify(value);if(unique.has(key))return;unique.add(key);if(Array.isArray(value)){if(value[0]==='$array')for(const child of value[1])visit(child);else if(value[0]==='$object')for(const [,child] of value[1])visit(child);}}for(const value of values)visit(value);return unique;}
function describeResult(v){if(v===undefined||v===null||typeof v!=='object')return encode(v);return ['$source-object',{kind:typeof v.kind==='string'?v.kind:null,id:typeof v.id==='string'?v.id:null}];}
function buildProfile(){
const sourceCases=[];let currentSourceCase=null;

let mode='',trace=[],original,wrapped,other,late,owner,received=null;
const note=s=>trace.push(s),id=n=>n===null?'null':n===undefined?'undefined':n.id;
function fail(name='HostError'){throw Object.assign(new Error('witness'),{name});}
function reset(){mode='';trace=[];original={id:'original'};wrapped={id:'wrapped'};other={id:'other'};late=null;owner=null;received=null;}
class Probe extends HtmlNode{
 getXML(){note('getXML');if(mode==='getXML-other')return other;if(mode==='getXML-throw')fail();return super.getXML();}
 setXML(x,a=null){note('setXML/'+id(x)+'/'+(a?'host':'null'));if(mode==='setXML-throw')fail();if(mode==='setXML-other')return late;return super.setXML(x,a);}
 textContent(){note('textContent');if(mode==='text-override')return 'override\ud800😀';return super.textContent();}
 mError(message,options,short){note('mError/'+message+'/'+short);received=options;if(mode==='error-throw')fail();if(options)options.observed=true;return late;}
 raw(){return this.xml;}host(){return this.adaptor;}
}
function adaptor(){return {
 getAttribute(x,k){note('attribute/'+id(x)+'/'+k);if(mode==='attribute-throw'||mode.startsWith('node-')||mode==='wrap'||mode==='fresh-wrap')fail();if(mode==='attribute-primitive')throw 7;if(mode==='attribute-foreign')throw new Error('foreign');if(mode==='attribute-rebind')owner.setXML(other,null);return mode==='attribute-false'?false:mode==='attribute-null'?null:mode==='attribute-undefined'?undefined:'hdw';},
 node(k,a,c){note('node/'+k+'/'+Object.keys(a).length+'/'+c.length+'/'+id(c[0]));if(mode==='fresh-wrap'){note('fresh/'+String(this.lastAttrs!==a));this.lastAttrs=a;a.touched=true;}if(mode==='node-throw')fail();if(mode==='node-null')return null;return wrapped;},
 outerHTML(x){note('outerHTML/'+id(x));if(mode==='outer-throw')fail();return mode==='outer-unicode'?'<标签>\ud800😀&\n</标签>':'<payload>'+id(x)+'</payload>';},
 textContent(x){note('hostText/'+id(x));if(mode==='text-throw')fail();return id(x)+'\ud800😀&\n';},
 kind(x){note('hostKind/'+id(x));if(mode==='kind-throw')fail();if(mode==='kind-rebind')owner.setXML(other,null);return mode==='kind-unicode'?'标签\ud800😀':'tag-'+id(x);},
 clone(x){note('clone/'+id(x));if(mode==='clone-throw')fail();return wrapped;},
 serializeXML(x){note('serializeXML/'+id(x));return '<xml>'+id(x)+'</xml>';}
};}
function factory(){const f=new MmlFactory();f.setNodeClass('html',HtmlNode);f.setNodeClass('probe',Probe);return f;}
function caught(fn){try{return {result:fn(),error:''};}catch(e){return {result:undefined,error:e?.name||typeof e};}}
let cases=0,assertions=0;
function begin(name){reset();++cases;currentSourceCase={name,input:{scenario:name,dimensions:name.split('_')},observations:[]};sourceCases.push(currentSourceCase);}function end(){currentSourceCase=null;}
function eq(expected){++assertions;if(currentSourceCase)currentSourceCase.observations.push({kind:'equal',expected:encode(expected)});}
function prop(expected){++assertions;if(currentSourceCase)currentSourceCase.observations.push({kind:'deep-equal',expected:encode(expected)});}
function observation(fn){trace=[];const result=caught(fn);if(currentSourceCase)currentSourceCase.observations.push({kind:'call',error:encode(result.error),result:describeResult(result.result),trace:encode(trace)});eq(result.error);prop(trace);return result.result;}
begin('default_state');{const f=factory(),n=f.create('html');eq(n.kind);eq(n.getHTML()===null);eq(n.isToken);eq(n.isEmpty);eq(n.arity);eq('textContent' in n);}end();
for(const change of ['', 'attribute-false','attribute-null','attribute-undefined','attribute-throw','attribute-primitive','attribute-foreign','attribute-rebind','wrap','node-null','node-throw','setXML-throw','setXML-other'])for(const payload of ['original','null']){
 begin('set_'+(change||'normal')+'_'+payload);const f=factory(),n=f.create('probe'),host=adaptor(),x=payload==='null'?null:original;late=f.create('XML');owner=n;mode=change;const result=observation(()=>n.setHTML(x,host));eq(result===n);eq(result===late);eq(id(n.raw()));for(const [candidate,expr]of [[original,'original'],[wrapped,'wrapped'],[other,'other'],[null,'nullptr']])eq(n.raw()===candidate);eq(n.host()===host);end();
}
for(const hostKind of ['null','xml-only']){begin('set_host_'+hostKind);const f=factory(),n=f.create('probe');observation(()=>n.setHTML(original,hostKind==='null'?null:{}));eq(id(n.raw()));eq(n.host()===null);end();}
for(const change of ['', 'getXML-other','getXML-throw']){begin('get_'+(change||'normal'));const f=factory(),n=f.create('probe'),host=adaptor();n.setXML(original,host);mode=change;const result=observation(()=>n.getHTML());if(result!==undefined)eq(id(result));end();}
for(const method of ['getSerializedHTML','textContent','toString'])for(const change of ['', 'null-payload','no-adaptor','xml-only',method==='getSerializedHTML'?'outer-throw':method==='textContent'?'text-throw':'kind-throw',...(method==='toString'?['kind-rebind','kind-unicode']:method==='getSerializedHTML'?['outer-unicode']:[])]){
 begin(method+'_'+(change||'normal'));const f=factory(),n=f.create('probe'),host=adaptor();owner=n;n.setXML(change==='null-payload'?null:original,change==='no-adaptor'?null:change==='xml-only'?{}:host);mode=change;const result=observation(()=>n[method]());if(result!==undefined){if(method==='textContent'){++assertions;}else {++assertions;}}eq(id(n.raw()));end();
}
for(const parentKind of ['none','mi','mrow'])for(const opts of ['object','null'])for(const change of ['', 'error-throw']){begin('verify_'+parentKind+'_'+opts+'_'+(change||'normal'));const f=factory(),n=f.create('probe'),options=opts==='null'?null:{check:true};if(parentKind!=='none')n.parent=f.create(parentKind);mode=change;observation(()=>n.verifyTree(options));eq(received===options&&received!==null);prop(options);end();}
begin('verify_inherited_empty_error');{const f=factory(),n=f.create('html');n.parent=f.create('mrow');observation(()=>n.verifyTree(null));}end();
for(const change of ['', 'text-override','text-throw','rebound']){begin('real_token_'+(change||'normal'));const f=factory(),n=f.create('probe'),host=adaptor(),token=f.create('mi',{},[f.create('text').setText('left'),n,f.create('XML'),f.create('text').setText('right')]);n.setXML(original,host);if(change==='rebound')n.setXML(other,host);mode=change;const result=observation(()=>token.getText());if(result!==undefined){++assertions;}end();}
for(const change of ['', 'clone-throw','no-adaptor','factory-throw']){begin('copy_'+(change||'normal'));const f=factory();f.setNodeClass('html',Probe);const n=f.create('probe'),host=adaptor();n.setXML(original,change==='no-adaptor'?null:host);mode=change;const create=f.create.bind(f);f.create=(k,...args)=>{note('factory/'+k);if(mode==='factory-throw')fail();return create(k,...args);};const result=observation(()=>n.copy());if(result){eq(result===n);eq(result instanceof Probe);eq(result.kind);eq(id(result.raw()));eq(result.host()===null);observation(()=>result.getSerializedHTML());}end();}

begin('wrapper_argument_freshness');{const f=factory(),n=f.create('probe'),host=adaptor();mode='fresh-wrap';observation(()=>{n.setHTML(original,host);n.setHTML(null,host);});eq(id(n.raw()));}end();
begin('copy_live_token_text');{const f=factory();f.setNodeClass('html',Probe);const n=f.create('probe'),host=adaptor();n.setXML(original,host);const copy=n.copy();assert.ok(copy instanceof Probe);++assertions;copy.setXML(other,host);n.setXML(null,null);const token=f.create('mi',{},[copy]);const result=observation(()=>token.getText());++assertions;}end();

for(const omitted of [true,false]){begin('set_default_'+(omitted?'omitted':'undefined'));const f=factory(),n=f.create('probe');observation(()=>omitted?n.setHTML(original):n.setHTML(original,undefined));eq(n.raw()===null);end();}
assert.equal(sourceCases.length,75,'frozen HTML Source scenario count');const deepExpected=sourceCases.flatMap(c=>c.observations.filter(o=>o.kind==='deep-equal').map(o=>o.expected));const uniqueTopLevelDeepExpected=new Set(deepExpected.map(v=>JSON.stringify(v)));const recursiveGraphValues=collectGraphValues(deepExpected);assert.equal(uniqueTopLevelDeepExpected.size,29,'29 distinct top-level deep Source expectations');assert.equal(recursiveGraphValues.size,52,'52 recursively distinct tagged Source values in those expectation trees');
const runtimeLock=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/locks/p12b-source-lock.json'),'utf8'));
const profile={schema:'folia.math.html-node-source.v1',source:{node:process.version,package:'@mathjax/src',version:'4.1.3',pins:pins(),runtime:{source:runtimeLock.source,sha256:runtimeLock.sha256,runtime:runtimeLock.runtime,runtimeSha256:runtimeLock.runtime_sha256,compiledPackage:runtimeLock.compiled_package}},migration:{originalToolSha256:'528ea0c58d78405c0cfc708968fc242cd1ed6f2811ca7c35d72d2b3d3b674bfa',frozenFixtureSha256:'1e34a50080be037172f736ad1d6986d34657c227a4525e4381fb08e8a808d714',frozenFixtureBytes:52398,scenarioCount:sourceCases.length,deepObservationCount:deepExpected.length,uniqueTopLevelDeepExpectedCount:uniqueTopLevelDeepExpected.size,recursiveGraphValueCount:recursiveGraphValues.size,note:'Source observations were migrated from the pinned CJS scenario generator. The one-time bridge proves every legacy p()/Property expectation is retained in the same order as a direct tagged deep-equal observation. Of the expectation trees, 29 are distinct at top level and recursively contain 52 distinct tagged values; legacy C++ value-slot indices are omitted. No native values enter this profile.'},scenarios:sourceCases};
return {profile,counts:{scenarios:sourceCases.length,deepObservations:deepExpected.length,uniqueTopLevelDeepExpected:uniqueTopLevelDeepExpected.size,recursiveGraphValues:recursiveGraphValues.size,assertions,cases}};
}
const profilePath=path.join(root,'tools/math-port/profiles/evidence/html-node-source.json');
if(require.main===module){assert.ok(process.argv.includes('--check'),'Source-only oracle is check-only and never creates a profile');const built=buildProfile();assert.deepEqual(JSON.parse(fs.readFileSync(profilePath,'utf8')),built.profile);console.log(JSON.stringify({...built.counts,mode:'source-profile-check'}));}
module.exports={buildProfile};
