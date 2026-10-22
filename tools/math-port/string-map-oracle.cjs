// Source-only check; original profile bytes and all Source fields are preserved.
// Original builtin Map observations for the parser family's actual string-key owners.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');
const sources=['ts/input/tex/Configuration.ts','ts/input/tex/Tags.ts','ts/input/tex/ParseOptions.ts'];
const hashes=sources.map(source=>({source,sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'.reference/MathJax-src',source))).digest('hex')}));
const rows=[];
function add(label,run){rows.push({label,expected:run()});}
function txt(x){return x===undefined?'U':x===null?'N':typeof x==='object'?'O':String(x);}
function keys(m){return [...m.keys()].join(',');}
add('empty-get-and-has',()=>{const m=new Map();return txt(m.get('a'))+':'+m.has('a')+':'+keys(m);});
add('replacement-preserves-order',()=>{const m=new Map([['b',1],['a',2]]);m.set('b',3);return keys(m)+':'+m.get('b')+':'+m.size;});
add('prototype-names-are-map-keys',()=>{const m=new Map([['__proto__',1],['constructor',2],['toString',3]]);return keys(m)+':'+m.get('__proto__');});
add('undefined-value-is-present',()=>{const m=new Map([['a',undefined]]);return m.has('a')+':'+txt(m.get('a'))+':'+m.has('a')+':'+m.has('b');});
add('null-false-zero-empty-values',()=>{const m=new Map([['n',null],['b',false],['z',0],['s','']]);return ['n','b','z','s'].map(k=>txt(m.get(k))).join(':');});
add('map-handle-alias',()=>{const m=new Map(),alias=m;alias.set('x',1);m.set('x',2);return keys(m)+':'+alias.get('x');});
add('fresh-map-independent',()=>{const a=new Map([['x',1]]),b=new Map([['x',2]]);return a.get('x')+':'+b.get('x');});
add('object-values-retain-identity',()=>{const o={},m=new Map([['o',o]]);return String(m.get('o')===o);});
add('keys-iterator-sees-appends',()=>{const m=new Map([['a',1]]),i=m.keys(),a=i.next().value;m.set('b',2);return a+':'+i.next().value+':'+txt(i.next().value);});
add('keys-completion-is-sticky',()=>{const m=new Map([['a',1]]),i=m.keys(),a=i.next().value,end=i.next().value;m.set('b',2);return a+':'+txt(end)+':'+txt(i.next().value);});
add('keys-iterator-retains-owner',()=>{const i=new Map([['a',1]]).keys();return i.next().value+':'+txt(i.next().value);});
add('two-key-iterators-independent',()=>{const m=new Map([['a',1],['b',2]]),a=m.keys(),b=m.keys();return a.next().value+':'+a.next().value+':'+b.next().value;});
const report={stage:'native string-keyed Map host; not parser algorithm acceptance',node:process.version,hashes,rows};
const profile=path.join(root,'docs/math-port/evidence/p53-string-map-source.json');
const sourceGold=JSON.parse(fs.readFileSync(profile,'utf8'));
const projected={...sourceGold,rows:sourceGold.rows.map(({cpp,...row})=>row)};
assert.deepEqual(report,projected);
console.log(JSON.stringify({sourceCases:rows.length,mode:'read-only-source-check'}));
