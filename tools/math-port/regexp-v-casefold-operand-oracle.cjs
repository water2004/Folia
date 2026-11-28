// Root supplements survived singleton mutants using actual Node, never native output.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
assert.equal(process.version,'v24.13.0');const root=path.resolve(__dirname,'../..'),cases=[],units=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i));
const operands=['a','A','k','K','K','s','S','ſ','σ','Σ','ς','µ','Μ','μ','\\u{10400}','\\u{10428}','[a-z]','[^a-z]','\\p{ASCII}','\\P{ASCII}','\\w','\\W'],texts=['','a','A','k','K','K','s','S','ſ','σ','Σ','ς','µ','Μ','μ','0','_',' ','\n','\ud800','\udc00','\u{10400}','\u{10428}','BaKz'];
for(const left of operands)for(const right of operands)for(const op of ['&&','--',''])for(const flags of ['v','vi']){
 const pattern='['+left+op+right+']',re=new RegExp(pattern,flags+'d');for(const text of texts){re.lastIndex=0;const m=re.exec(text);cases.push({id:cases.length,group:'bare-operand-algebra',pattern:units(pattern),flags,text:units(text),start:0,expected:{constructor:'ok',match:m?{ranges:m.indices.map(r=>r??null),named:Object.entries(m.indices.groups??{}).map(([name,range])=>({name,range:range??null}))}:null,last_index:re.lastIndex}});}
}
// Range appearing after an existing union operand follows a separate parser site.
for(const prefix of ['0','a','A','k','K','\\d','\\w','[A]'])for(const range of ['a-z','A-Z','σ-ω','\\u{10400}-\\u{1044f}'])for(const neg of ['', '^'])for(const flags of ['v','vi']){
 const pattern='['+neg+prefix+range+']',re=new RegExp(pattern,flags+'d');for(const text of texts){re.lastIndex=0;const m=re.exec(text);cases.push({id:cases.length,group:'range-after-union-operand',pattern:units(pattern),flags,text:units(text),start:0,expected:{constructor:'ok',match:m?{ranges:m.indices.map(r=>r??null),named:Object.entries(m.indices.groups??{}).map(([name,range])=>({name,range:range??null}))}:null,last_index:re.lastIndex}});}
}
const data=JSON.stringify(cases),report={stage:'actual Node supplemental bare-operand algebra and later union ranges; coverage extension after singleton/range mutants survived',node:process.version,unicode:process.versions.unicode,patterns:operands.length**2*3+64,cases:cases.length,source_case_sha256:crypto.createHash('sha256').update(data).digest('hex')},file=path.join(root,'build/math-port-regexp-v-casefold/operand-node-reference.json'),summary=path.join(root,'tools/math-port/profiles/evidence/p21d-v-casefold-operand-source.json');
if(process.argv.includes('--check')){assert.equal(fs.readFileSync(file,'utf8'),data);assert.deepEqual(JSON.parse(fs.readFileSync(summary)),report);}else{fs.writeFileSync(file,data);fs.writeFileSync(summary,JSON.stringify(report,null,2)+'\n');}console.log(JSON.stringify(report));
