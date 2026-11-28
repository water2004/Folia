// Root independent real-RegExp witnesses; no candidate code/data consulted.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
assert.equal(process.version,'v24.13.0');assert.equal(process.versions.unicode,'16.0');assert.equal(process.versions.icu,'77.1');
const root=path.resolve(__dirname,'../..'),work=path.join(root,'build/math-port-regexp-legacy-oracle');
const units=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i)),escape=s=>units(s).map(c=>'\\u'+c.toString(16).padStart(4,'0')).join('');
const cases=[],seen=new Set();
function add(group,pattern,flags,text,start=0){const key=JSON.stringify([pattern,flags,text,start]);if(seen.has(key))return;seen.add(key);
 let re,expected;try{re=new RegExp(pattern,flags);}catch(e){assert.equal(e.name,'SyntaxError');expected={constructor:'SyntaxError'};}
 if(re){if(!flags.includes('d'))re=new RegExp(pattern,flags+'d');re.lastIndex=start;const m=re.exec(text);expected={constructor:'ok',match:m?{ranges:m.indices.map(r=>r??null),named:Object.keys(m.indices.groups??{}).map(name=>({name,range:m.indices.groups[name]??null}))}:null,last_index:re.lastIndex};}
 cases.push({id:cases.length,group,pattern:units(pattern),flags,text:units(text),start,expected});
}
const relevant=new Set([0,9,10,13,32,48,65,90,95,97,122,127,128,0xd800,0xdbff,0xdc00,0xdfff,0xffff]);
for(let cp=0;cp<=0xffff;++cp){const c=String.fromCharCode(cp);if(c.toUpperCase()!==c||c.toLowerCase()!==c)relevant.add(cp);}
for(const cp of relevant){const c=String.fromCharCode(cp),e=escape(c),upper=c.toUpperCase(),lower=c.toLowerCase();
 const texts=[...new Set([c,upper,upper.slice(0,1),lower])];
 for(const pattern of [e,'['+e+']','[^'+e+']','['+e+'-'+e+']','['+e+'\\u0000-\\u0004]'])for(const flags of ['i','iu'])for(const text of texts)add('literal-class-range',pattern,flags,text);
 for(const flags of ['i','iu'])for(const text of [c+upper,lower+c]){
  add('backreference','('+e+')\\1',flags,text);
  add('named-backreference','(?<letter>'+e+')\\k<letter>',flags,text);
 }
 for(const pattern of ['[a-z]','[^a-z]','[A-Z]','[^A-Z]','\\w','\\W','\\b','\\B'])for(const flags of ['i','iu'])add('word-range-boundary',pattern,flags,c);
}
for(const pattern of ['s','k','[a-z]','[^a-z]','\\w+','\\W+','\\b','\\B','(s)\\1','[ᾀ-ᾇ]','[^ᾀ-ᾇ]','[\\wſK]'])for(const flags of ['i','iu','iv','ig','igu','igv','iy','iyu','iyv','ims'])for(const text of ['Kſİı','ſs','sſ','ᾀᾈ','ᾳᾼ','\ud800x\udfff','a😀s'])for(const start of [0,1,2,text.length,text.length+1])add('stateful-mode-controls',pattern,flags,text,start);
const data=JSON.stringify(cases),sha=crypto.createHash('sha256').update(data).digest('hex'),summary={stage:'independent Node legacy canonicalization witness; engine not accepted',node:process.version,unicode:process.versions.unicode,icu:process.versions.icu,relevant_bmp_units:relevant.size,cases:cases.length,source_case_sha256:sha,by_group:{}};
for(const c of cases)summary.by_group[c.group]=(summary.by_group[c.group]??0)+1;
const file=path.join(work,'node-reference.json'),report=path.join(root,'tools/math-port/profiles/evidence/p20b-legacy-regexp-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(file,'utf8'),data);assert.deepEqual(JSON.parse(fs.readFileSync(report)),summary);}
else{fs.mkdirSync(work,{recursive:true});fs.writeFileSync(file,data);fs.writeFileSync(report,JSON.stringify(summary,null,2)+'\n');}
console.log(JSON.stringify({...summary,mode:process.argv.includes('--check')?'check':'freeze'}));
