// Root grammar-mode and lexical witnesses from actual Node, independent of Rust.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
assert.equal(process.version,'v24.13.0');const root=path.resolve(__dirname,'../..'),cases=[];
const units=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i));
function observe(pattern,flags,text,start,group){
 let re,expected;try{re=new RegExp(pattern,flags+'d');}catch(e){assert.equal(e.name,'SyntaxError');expected={constructor:e.name};}
 if(re){re.lastIndex=start;const m=re.exec(text);expected={constructor:'ok',match:m?{ranges:m.indices.map(r=>r??null),named:Object.entries(m.indices.groups??{}).map(([name,range])=>({name,range:range??null}))}:null,last_index:re.lastIndex};}
 cases.push({id:cases.length,group,pattern:units(pattern),flags,text:units(text),start,expected});
}
const bodies=['','0','41','61','Ff','D800','DC00','10FFFF','110000','FFFFFFFF','+41','-41',' 41','41 ','0x41','4_1','GG','000000000000000000000000000000000041'];
const texts=['','A','a','AA','aa','u','u'.repeat(41),'u{41}','u{+41}','+041','\ud800A','\ud800\udc00','\ud800','\udc00','\u{10ffff}','\0'];
for(const body of bodies)for(const flags of ['','i','u','iu','v','iv','g','gu','gv','y','yu','yv'])for(const pattern of ['\\u{'+body+'}','[\\u{'+body+'}]','(\\u{'+body+'})+','\\uD800\\u{'+body+'}'])for(const text of texts)for(const start of [0,1])observe(pattern,flags,text,start,'braced-mode-and-hex-digit');
for(const digits of ['0041','+041','-041',' 041','0x41','004G','D800','DBFF','DC00','DFFF','0000','ffff'])for(const flags of ['','i','u','iu','v','iv'])for(const pattern of ['\\u'+digits,'[\\u'+digits+']','\\uD800\\u'+digits])for(const text of texts)observe(pattern,flags,text,0,'hex4-lexical-and-surrogate-trial');
for(const name of ['\\u{61}','a\\u{62}','\\u0061','\\u{+61}','\\u+061','\\u{D800}','\\u{10400}','\\uD801\\uDC00'])for(const flags of ['','u','v','g','gu','gv'])for(const pattern of ['(?<'+name+'>A)','(?<'+name+'>A)\\k<'+name+'>'])for(const text of ['A','AA','BAAB',''])observe(pattern,flags,text,0,'named-identifier-independent-grammar');
const data=JSON.stringify(cases),report={stage:'actual Node Unicode escape grammar; no candidate/host accepted',node:process.version,unicode:process.versions.unicode,icu:process.versions.icu,cases:cases.length,by_group:Object.fromEntries([...new Set(cases.map(c=>c.group))].map(group=>[group,cases.filter(c=>c.group===group).length])),source_case_sha256:crypto.createHash('sha256').update(data).digest('hex')};
const file=path.join(root,'build/math-port-regexp-unicode-escape/node-reference.json'),summary=path.join(root,'tools/math-port/profiles/evidence/p21b-unicode-escape-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(file,'utf8'),data);assert.deepEqual(JSON.parse(fs.readFileSync(summary)),report);}else{fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,data);fs.writeFileSync(summary,JSON.stringify(report,null,2)+'\n');}
console.log(JSON.stringify(report));
