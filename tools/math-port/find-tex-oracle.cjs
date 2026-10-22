// Root complete original FindTeX observations; no replacement scanner.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=(process.env.FOLIA_MATH_ROOT||path.resolve(__dirname,'../..')),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src'),lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p46-parser-options-source.json'))),hashes=[];
for(const source of ['ts/input/tex/FindTeX.ts','ts/core/FindMath.ts','ts/core/MathItem.ts','ts/util/string.ts','ts/util/Options.ts']){const entry=lock.hashes.find(x=>x.source===source);assert.ok(entry,source);const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(base,source.replace(/^ts\//,'cjs/').replace(/\.ts$/,'.js')))).digest('hex'),entry.cjs_sha256);hashes.push(entry);}
const {FindTeX}=require(path.join(base,'cjs/input/tex/FindTeX.js')),rows=[];
function hex(s){return Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');}
function text(v){return v===undefined?'U':v===null?'N':typeof v==='object'?'O':String(v);}
function item(v){return[hex(v.open),hex(v.math),hex(v.close),v.n,v.start.n,v.end.n,text(v.display)].join(',');}
function serialize(items){return items.map(item).join('/');}
function fields(j){return[text(j.options),j.start?'R':'U',text(j.end),text(j.hasPatterns),text(j.env),text(j.sub)].join('/');}
function fixture(){const e={trace:[],hooks:{phase:false,scan:false,capture:false,mutation:0,input:null,saved:null},j:null};class Probe extends FindTeX{
getPatterns(){if(e.hooks.phase)e.trace.push('before:'+fields(this));super.getPatterns();if(e.hooks.phase)e.trace.push('after:'+fields(this));}
findMathInString(math,n,s){if(e.hooks.scan)e.trace.push('scan:'+n+':'+text(s));if(n===0&&e.hooks.mutation)e.hooks.input[e.hooks.mutation===1?1:e.hooks.input.length]='$z$';super.findMathInString(math,n,s);}
findEnd(s,n,start,end){const result=super.findEnd(s,n,start,end);if(e.hooks.capture&&result)e.hooks.saved=result;return result;}}
e.make=o=>(e.j=new Probe(o));return e;}
function add(label,run){const e=fixture();let result='THREW',error='ok';try{result=run(e);}catch(x){error=x.name;}rows.push({label,expected:error+'|'+result+'|'+e.trace.join('~')});}


function scan(label,options,strings){add(label,e=>serialize(e.make(options).findMath(strings)));}
const dollar={inlineMath:[['$','$']]},disabled={inlineMath:[],displayMath:[],processEscapes:false,processEnvironments:false,processRefs:false};
for(const[label,input]of [
 ['default-empty',[]],['default-plain',['abc']],['default-dollar-not-inline',['$x$']],['default-inline',['a \\(x+1\\) b']],['default-display-dollar',['a $$x$$ b']],['default-display-bracket',['\\[x\\]']],['default-mixed',['\\(x\\) $$y$$ \\[z\\]']],
 ['default-multiple-strings',['a \\(x\\)','$$y$$','plain']],['default-unmatched',['\\(x']],['default-empty-math',['\\(\\)']],['default-control-sequence-skips-close',['\\(a\\foo b\\)']],
 ['default-braces-hide-close',['\\(a{b\\)c}d\\)']],['default-unbalanced-braces',['\\(a{b\\)']],['default-extra-close-brace',['\\(a}b\\)']],['default-newlines',['\\(a\nb\\)']],
 ['default-environment',['a \\begin{align}x&y\\end{align} b']],['default-environment-whitespace',['\\begin\u00a0{align}x\\end\ufeff{align}']],['default-empty-environment-name',['\\begin{}x\\end{}']],
 ['default-punctuation-environment',['\\begin{a-b:c}x\\end{a-b:c}']],['default-unmatched-environment',['\\begin{align}x\\end{other}']],['default-ref-and-eqref',['a \\ref{r} \\eqref{s}']],
 ['default-ref-whitespace',['\\ref\u00a0{r}']],['default-escaped-dollar',['a \\$ b']],['default-escaped-backslash',['a \\\\ b']],['default-substitutions-mixed',['\\$ \\ref{r} \\(x\\)']],
 ['default-astral-offsets',['😀 \\(😀+x\\) 🧮']],['default-surrogate-halves',['\ud800 \\(\udc00\\)']],['default-slash-astral-control',['\\(\\😀{x}\\)']]
])scan(label,{},input);
for(const[label,input]of [['inline-dollar',['a $x$ b']],['inline-dollar-empty',['$$']],['display-dollar-wins-prefix',['$$x$$ $y$']],['multiple-dollar',['$x$ $y$']],['escaped-dollar-with-inline',['\\$x $y$']],['dollar-in-braces',['$a{b$c}d$']],['dollar-escaped-close',['$a\\$b$']],['incomplete-then-valid',['$a \\(x\\)']]])scan(label,dollar,input);
scan('adjacent-default-math',{},['\\(x\\)\\(y\\)']);
scan('adjacent-inline-dollar',dollar,['$x$$y$']);
scan('disabled-no-patterns',disabled,['$x$ \\(y\\) \\ref{r}']);
scan('escapes-disabled',{...dollar,processEscapes:false},['\\$x$']);
scan('environments-disabled',{processEnvironments:false},['\\begin{align}x\\end{align}']);
scan('references-disabled',{processRefs:false},['\\ref{r} \\eqref{s}']);
scan('only-references',{...disabled,processRefs:true},['\\ref{r} \\eqref{s}']);
scan('only-escapes',{...disabled,processEscapes:true},['\\$ \\\\']);
scan('only-environments',{...disabled,processEnvironments:true},['\\begin{x}a\\end{x}']);
scan('literal-quoted-punctuation',{inlineMath:[[':(','-]']],displayMath:[]},['a :(x-] b']);
scan('literal-all-regex-punctuation',{inlineMath:[['^$(){}.+*?-|[]:\\','END.*']],displayMath:[]},['^$(){}.+*?-|[]:\\xEND.*']);
scan('longer-opener-wins',{inlineMath:[['@','@'],['@@','@@']],displayMath:[]},['@@x@@ @y@']);
scan('same-length-opener-lexical-sort',{inlineMath:[['b@','@b'],['a@','@a']],displayMath:[]},['b@x@b a@y@a']);
scan('duplicate-opener-last-end-wins',{inlineMath:[['@','!'],['@','?']],displayMath:[]},['@x!y?']);
scan('display-definition-overwrites-inline',{inlineMath:[['@','!']],displayMath:[['@','?']]},['@x?']);
scan('ordinary-end-prototype-key',{inlineMath:[['__proto__','END']],displayMath:[]},['__proto__xEND']);
scan('ordinary-end-constructor-key',{inlineMath:[['constructor','END']],displayMath:[]},['constructorxEND']);
scan('ordinary-end-tostring-key',{inlineMath:[['toString','END']],displayMath:[]},['toStringxEND']);
scan('astral-delimiter-and-utf16-offsets',{inlineMath:[['😀','🧮']],displayMath:[]},['a😀x🧮b']);
scan('empty-close-delimiter',{inlineMath:[['@','']],displayMath:[]},['@x']);
scan('null-inline-array-is-type-error',{inlineMath:null},[]);
scan('false-inline-array-is-type-error',{inlineMath:false},[]);
scan('missing-close-is-type-error',{inlineMath:[['@']]},[]);
scan('nonstring-close-is-type-error',{inlineMath:[['@',4]]},[]);
add('constructor-derived-fields-and-virtual-phase',e=>{e.hooks.phase=true;return fields(e.make({}));});
add('fresh-results-and-reset-start-cursor',e=>{const j=e.make(dollar),a=j.findMath(['$x$']),b=j.findMath(['$x$']);return[a!==b,serialize(a),serialize(b)].join(':');});
add('environment-mutates-same-proto-record',e=>{e.hooks.capture=true;const result=e.make({}).findMath(['\\begin{x}a\\end{x}']);return[e.hooks.saved===result[0],serialize(result),item(e.hooks.saved)].join(':');});
add('input-slots-read-live-after-first-scan',e=>{e.hooks.scan=true;e.hooks.mutation=1;e.hooks.input=['$x$','$y$'];return serialize(e.make(dollar).findMath(e.hooks.input));});
add('input-length-captured-before-scan',e=>{e.hooks.scan=true;e.hooks.mutation=2;e.hooks.input=['$x$'];return serialize(e.make(dollar).findMath(e.hooks.input));});
add('input-getters-read-once-in-order',e=>{const j=e.make(dollar),input=[];Object.defineProperty(input,0,{get(){e.trace.push('get0');return '$x$';}});Object.defineProperty(input,1,{get(){e.trace.push('get1');return '$y$';}});return serialize(j.findMath(input));});
add('input-hole-exec-coercion-is-deferred',e=>{const a=new Array(1);return serialize(e.make({}).findMath(a));});
scan('primitive-input-slot-with-no-start',{},[null,undefined,7,false]);
add('matched-nonstring-slot-substring-is-type-error',e=>serialize(e.make({inlineMath:[['n','l']],displayMath:[]}).findMath([null])));
add('disabled-finder-does-not-inspect-null-input',e=>serialize(e.make(disabled).findMath(null)));
add('enabled-finder-null-input-is-type-error',e=>serialize(e.make({}).findMath(null)));
add('getpatterns-replaces-end-and-regex-identities',e=>{const j=e.make({}),end=j.end,start=j.start;j.getPatterns();return[end!==j.end,start!==j.start].join(':');});
add('getpatterns-real-looked-up-forEach',e=>{const j=e.make({});j.options.inlineMath.forEach=()=>e.trace.push('forEach');j.getPatterns();return fields(j);});
add('delimiter-getters-consumed-in-source-order',e=>{const j=e.make({...disabled,inlineMath:[['@','!']]}),t=j.options.inlineMath[0];Object.defineProperty(t,0,{get(){e.trace.push('open');return '@';}});Object.defineProperty(t,1,{get(){e.trace.push('close');return '!';}});j.getPatterns();return serialize(j.findMath(['@x!']));});
for(const shrink of [false,true])add(shrink?'delimiter-iterator-rechecks-length-after-open-getter':'delimiter-iterator-skips-inherited-close-beyond-length',
e=>{const j=e.make({...disabled,inlineMath:[['@','!']]}),tuple=j.options.inlineMath[0],prototype=Object.create(Array.prototype);Object.defineProperty(prototype,1,{get(){e.trace.push('inherited-close');return '!';}});Object.setPrototypeOf(tuple,prototype);if(!shrink)tuple.length=1;Object.defineProperty(tuple,0,{get(){e.trace.push('open');if(shrink)tuple.length=1;return '@';}});j.getPatterns();return 'unexpected';});
add('actual-constructor-metadata-and-defaults',()=>{let out=[FindTeX.name,FindTeX.length,Object.hasOwn(FindTeX,'OPTIONS')].join(':');for(const name of ['inlineMath','displayMath'])for(const pair of FindTeX.OPTIONS[name])out+=':'+pair.join(',');for(const name of ['processEscapes','processEnvironments','processRefs'])out+=':'+FindTeX.OPTIONS[name];return out;});

// Frozen original Source JSON remains byte-identical; only renderer-only cpp fields are projected.
assert.ok(process.argv.includes('--check'),'Source-only oracle supports read-only --check');
const frozenProfilePath=path.join(root,"docs/math-port/evidence/p52-find-tex-source.json");
const frozenBytes=fs.readFileSync(frozenProfilePath);
function sourceFields(value){if(Array.isArray(value))return value.map(sourceFields);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([key])=>key!=='cpp').map(([key,v])=>[key,sourceFields(v)]));return value;}
const actualSourceProfile={stage:'complete actual FindTeX algorithms; not TeXParser',node:process.version,hashes,rows};
assert.deepStrictEqual(sourceFields(actualSourceProfile),sourceFields(JSON.parse(frozenBytes)),'Original Source inputs/expectations/pins changed');
console.log(JSON.stringify({sourceCases:rows.length,profile:"docs/math-port/evidence/p52-find-tex-source.json",profileSha256:crypto.createHash('sha256').update(frozenBytes).digest('hex'),mode:'check'}));
