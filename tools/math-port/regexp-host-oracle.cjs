// srell_sha256 is frozen historical native-artifact metadata, not a Source input.
// Root observations of actual ECMAScript RegExp and pinned FindTeX generators.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src'),lock=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p46-parser-options-source.json'))),hashes=[];
for(const source of ['ts/input/tex/FindTeX.ts','ts/util/string.ts']){const entry=lock.hashes.find(x=>x.source===source);assert.ok(entry,source);const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),entry.source_sha256);assert.equal(fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(base,source.replace(/^ts\//,'cjs/').replace(/\.ts$/,'.js')))).digest('hex'),entry.cjs_sha256);hashes.push(entry);}
const {FindTeX}=require(path.join(base,'cjs/input/tex/FindTeX.js')),{quotePattern}=require(path.join(base,'cjs/util/string.js'));
const finder=new FindTeX({}),rows=[];
function hex(s){return Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');}
function observe(r,input,count){const out=[];for(let i=0;i<count;i++){const m=r.exec(input);out.push((m?'index='+m.index+',caps='+Array.from(m,x=>x===undefined?'U':hex(x)).join('/')+',input='+hex(m.input):'null')+';last='+String(r.lastIndex));}return out.join('|');}
function add(label,pattern,input,flags='g',lastIndex=0,count=3){let expected;try{const r=new RegExp(pattern,flags);r.lastIndex=lastIndex;expected=observe(r,input,count);}catch(x){expected=x.name;}rows.push({label,pattern,input,flags,lastIndex:Number.isFinite(lastIndex)?lastIndex:String(lastIndex),count,expected});}
add('dot-splits-surrogate-pair','.','😀');
add('dot-starting-at-low-surrogate','.','😀','g',1,2);
add('negated-class-splits-surrogates','[^}]','😀');
add('literal-astral-pair','😀','a😀b');
add('optional-captures-are-undefined','(a)?(b*)','b');
add('unmatched-alternative-captures','(a)|(b)','b a');
add('absolute-index-with-nonzero-cursor','x','abx x','g',3);
add('preceding-context-is-not-sliced','\\bx','ax x','g',1);
add('global-failure-resets-cursor','x','ab','g',1);
add('cursor-after-end','x','x','g',2);
add('negative-cursor-is-zero','x','x','g',-4);
add('fractional-cursor-is-floored','x','xx','g',1.9);
add('nan-cursor-is-zero','x','x','g',NaN);
add('infinite-cursor-fails','x','x','g',Infinity);
add('large-cursor-fails','x','x','g',9007199254740992);
add('plain-scan-ignores-and-retains-cursor','x','ax','',17);
add('plain-negative-cursor-retained','x','ax','',-3);
add('empty-global-match-does-not-advance','','abc','g',1);
add('lookahead-zero-width-does-not-advance','(?=x)','ax','g',1);
add('sticky-failure-resets-cursor','x','ax','y',0);
add('sticky-starts-at-cursor','x','ax','y',1);
add('global-and-sticky','x','xx','gy',1);
add('dot-excludes-all-four-line-terminators','.','\n\r\u2028\u2029a');
add('space-includes-ecmascript-whitespace','\\s+','\u0085\u00a0\u1680\u2000\u2028\u2029\u202f\u205f\u3000\ufeff');
add('identity-escape-colon','\\:',':');
add('identity-escape-hyphen','\\-','-');
const punctuation='^$(){}.+*?-|[]:\\';add('all-quoted-delimiter-punctuation',quotePattern(punctuation),'a'+punctuation+'b');
add('actual-start-environment-and-refs',finder.start.source,'\\begin\u00a0{align}x\\ref\ufeff{a}\\$\\\\');
add('actual-end-pattern-skips-low-surrogate',finder.endPattern('\\)').source,'\\😀{x}\\)');
add('actual-end-pattern-skips-control-sequences',finder.endPattern('$$').source,'\\a{b}$$');
add('actual-environment-end-capture',finder.endPattern(null,'\\\\end\\s*(\\{a\\-b\\})').source,'x\\end\u00a0{a-b}');
add('invalid-group-is-syntax-error','(','x');
add('duplicate-flags-are-syntax-error','x','x','gg');
add('unknown-flag-is-syntax-error','x','x','z');
const manifest={stage:'native numeric-cursor non-Unicode scanner RegExp host; not complete ECMAScript regex',node:process.version,srell_sha256:'eac98b722d702048e7448eb885f918d2343503b1ba9174d7efe86e2a791a0b9a',hashes,rows};;assert.ok(process.argv.includes('--check'));assert.equal(fs.readFileSync(path.join(root,'docs/math-port/evidence/p50-regexp-host-source.json'),'utf8').replaceAll('\r\n','\n'),JSON.stringify(manifest,null,2)+'\n');console.log(JSON.stringify({rows:rows.length,mode:'check'}));
