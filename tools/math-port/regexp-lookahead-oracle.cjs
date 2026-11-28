// Root real RegExp lookahead witnesses, independent of candidate parse code.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
assert.equal(process.version,'v24.13.0');const root=path.resolve(__dirname,'../..'),cases=[];
const units=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i));
const tails=['\\u0000','\\u0041','\\uD7FF','\\uD800','\\uDBFF','\\uDC00','\\uDFFF','\\uE000','\\u{41}','\\u{DC00}','\\uZZZZ','\\x41','u0041',''];
for(const high of ['\\uD800','\\uDBFF'])for(const tail of tails)for(const flags of ['','i','u','iu','v','iv'])for(const pattern of [high+tail,'['+high+tail+'\\u0000-\\u0004]', '('+high+tail+')+'])for(const text of ['\ud800\0','\ud800A','\udbffA','\ud800\udc00','\udbff\udfff','\ud800\ud800','\ud800','\udbff']){
 let re,expected;try{re=new RegExp(pattern,flags+'d');}catch(e){assert.equal(e.name,'SyntaxError');expected={constructor:e.name};}
 if(re){const m=re.exec(text);expected={constructor:'ok',match:m?{ranges:m.indices.map(r=>r??null),named:[]}:null,last_index:0};}
 cases.push({id:cases.length,group:'surrogate-speculative-escape',pattern:units(pattern),flags,text:units(text),start:0,expected});
}
const data=JSON.stringify(cases),report={stage:'actual Node surrogate escape lookahead; engine not accepted',node:process.version,unicode:process.versions.unicode,icu:process.versions.icu,cases:cases.length,source_case_sha256:crypto.createHash('sha256').update(data).digest('hex')};
const file=path.join(root,'build/math-port-regexp-lookahead/node-reference.json'),summary=path.join(root,'tools/math-port/profiles/evidence/p20d-lookahead-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(file,'utf8'),data);assert.deepEqual(JSON.parse(fs.readFileSync(summary)),report);}else{fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,data);fs.writeFileSync(summary,JSON.stringify(report,null,2)+'\n');}
console.log(JSON.stringify(report));
