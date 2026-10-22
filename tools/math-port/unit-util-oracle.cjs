// Root pinned original UnitUtil observations. No native conversion evaluator.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root}=require('./simple-mml-reference.cjs'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src'),source='ts/input/tex/UnitUtil.ts',sha='90f461310aa6d07b494cda98dc5398e9737231f046e20099ea0f5c19fa24c66e';
const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),sha);assert.equal(bytes.toString().replaceAll('\r\n','\n'),fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'));
const {UnitUtil:u}=require(path.join(base,'cjs/input/tex/UnitUtil.js')),UnitMap=u.UNIT_CASES.constructor,defaults=[['em',1],['ex',.43],['pt',.1],['pc',1.2],['px',7.2/72],['in',7.2],['cm',7.2/2.54],['mm',7.2/25.4],['mu',1/18]];
const val=p=>p===undefined?'U':p===null?'N':Object.is(p,-0)?'-0':typeof p==='object'?'object':String(p),hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join(','),shape=t=>t.map(val).join(':');
const rows=[];function add(label,run){u.UNIT_CASES=new UnitMap(defaults);let result='THREW',error='ok';try{result=run();}catch(e){error=e.name+'|'+e.message;}rows.push({label,expected:error+'|'+result});}
const strings=['0em','-0em','+2em','2 ex','12pt','.4pc','4.px','2,5in',',5cm','-3mm','18mu','.001mu','0.01mu','-0.01mu','18.1234mu','1','', 'abc','1EM','1e2em','1.2.3em','+em','1,2,3em','1 em ',' \t-2.25 em\n','\u00A02em\uFEFF','\u20282em\u2029','١em','１em','2emtail','2em tail','2em  tail','2em\ttail','2em😀','2µ','2em\nrest','0'.repeat(325)+'1em','9'.repeat(325)+'em','0.'+'0'.repeat(325)+'1em'];
for(const dim of strings)for(const rest of [false,true])add('match:'+JSON.stringify([dim,rest]),()=>shape(u.matchDimen(dim,rest)));
for(const dim of strings)add('dimen2em:'+JSON.stringify(dim),()=>val(u.dimen2em(dim)));
for(const n of [0,-0,.000599999999,.0006,-.0006,.000600000001,.001,-.001,.12345,1.2345,1.2344999999,-1.2345,1000.1,1e21,1e22,1e30,-1e30,NaN,Infinity,-Infinity]){;add('em:'+String(n)+':'+Object.is(n,-0),()=>u.em(n));}
for(const code of [9,10,11,12,13,32,0xA0,0x1680,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,0x2009,0x200A,0x2028,0x2029,0x202F,0x205F,0x3000,0xFEFF,0x85,0x180E]){const ws=String.fromCharCode(code),dim=ws+'2em'+ws;add('whitespace-match:'+code,()=>shape(u.matchDimen(dim)));add('whitespace-trim:'+code,()=>hex(u.trimSpaces(ws+'a'+ws)));}
for(const [tag,p]of [['U',undefined],['N',null],['zero',0],['empty',''],['one',1],['string','rest'],['object',{}]])add('rest-truthiness:'+tag,()=>shape(u.matchDimen('2emtail',p)));
for(const s of ['','  abc  ',' \t\nabc\u00A0','\\ ','\\  ','\\\t','\\\n','a\\ ',' a\\\t ','a\\\t','a\\\u00A0','a\\\u00A0 ','😀 ','\uFEFFa\uFEFF','\u0085a\u0085'])add('trim:'+JSON.stringify(s),()=>hex(u.trimSpaces(s)));
for(const [tag,value]of [['U',undefined],['N',null],['false',false],['zero',0]]){
 add('trim-pass-through:'+tag,()=>{const p=value;return(u.trimSpaces(p)===p?'same':'bad')+':'+val(u.trimSpaces(p));});
 add('non-string-match:'+tag,()=>shape(u.matchDimen(value)));
}
add('trim-record-identity',()=>{const p={a:1};return u.trimSpaces(p)===p?'same':'bad';});
add('initial-map-metadata',()=>[u.UNIT_CASES.num,u.UNIT_CASES.unit,u.UNIT_CASES.dimenEnd.source,u.UNIT_CASES.dimenRest.source].join(':'));
for(const name of ['em','ex','pt','pc','px','in','cm','mm','mu','bad',''])add('map-get:'+name,()=>val(u.UNIT_CASES.get(name)));
add('map-null-get',()=>val(u.UNIT_CASES.get(null)));
add('map-update-retains-key-order-and-this',()=>{const map=u.UNIT_CASES,old=map.dimenEnd,r=map.set('em',2);map.set('zz',3);return(r===map?'same':'bad')+':'+map.unit+':'+(old!==map.dimenEnd?'new':'bad')+':'+val(u.dimen2em('2em'))+':'+val(u.dimen2em('2zz'));});
add('map-zero-fallback',()=>{u.UNIT_CASES.set('em',0);return val(u.UNIT_CASES.get('em'))+':'+val(u.dimen2em('2em'));});
add('map-delete-pattern-identity',()=>{const map=u.UNIT_CASES,old=map.dimenEnd,absent=map.delete('bad'),same=old===map.dimenEnd,removed=map.delete('em');return val(absent)+':'+val(same)+':'+val(removed)+':'+(old!==map.dimenEnd?'new':'bad')+':'+shape(u.matchDimen('2em'));});
add('map-delete-pt-no-fallback',()=>{u.UNIT_CASES.delete('pt');return val(u.UNIT_CASES.get('bad'))+':'+val(u.dimen2em('bad'));});
add('map-reinsert-key-at-end',()=>{u.UNIT_CASES.delete('em');u.UNIT_CASES.set('em',4);return u.UNIT_CASES.unit;});
add('map-constructor-duplicate-key',()=>{const map=new UnitMap([['x',1],['pt',.1],['x',2]]);return map.unit+':'+val(map.get('x'));});
add('map-empty-constructor',()=>{u.UNIT_CASES=new UnitMap([]);return u.UNIT_CASES.unit+':'+shape(u.matchDimen('2'))+':'+val(u.dimen2em('2'));});
add('map-alternative-prefix-order',()=>{u.UNIT_CASES=new UnitMap([['e',1],['em',2],['pt',.1]]);return shape(u.matchDimen('2emtail',true))+':'+shape(u.matchDimen('2em'));});
add('map-unicode-literal-key',()=>{u.UNIT_CASES.set('😀',2);return shape(u.matchDimen('2😀tail',true));});
add('live-mu-factor',()=>{u.UNIT_CASES.set('mu',.25);return shape(u.matchDimen('2mu'))+':'+val(u.dimen2em('2mu'));});
const report={source,sha256:sha,rows},sourceGold=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p44-unit-util-source.json'),'utf8'));const projected={...sourceGold,rows:sourceGold.rows.map(({cpp,...row})=>row)};assert.ok(process.argv.includes('--check'));assert.deepEqual(report,projected);console.log(JSON.stringify({rows:rows.length,mode:'check'}));
