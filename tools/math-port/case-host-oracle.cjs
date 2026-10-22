'use strict';
// Candidate canonical profile for the complete original Node Unicode source corpus.
// Do not execute here; Root performs the authorized Source comparison.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
assert.equal(process.version,'v24.13.0');assert.equal(process.versions.unicode,'16.0');assert.equal(process.versions.icu,'77.1');
const root=path.resolve(__dirname,'../..'),changed=[],contexts=[],examples=[];let begin=0,last=-1;
const units=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i));
for(let cp=0;cp<=0x10FFFF;++cp){const s=String.fromCodePoint(cp),lower=s.toLowerCase();if(s!==lower)changed.push({cp,out:units(lower)});const before=(s+'Σ').toLowerCase().endsWith('ς'),ignored=('A'+s+'Σ').toLowerCase().endsWith('ς'),after=('AΣ'+s).toLowerCase().charCodeAt(1)===0x03C2;const flags=(before?1:0)|(ignored?2:0)|(after?4:0);if(last!==flags){if(last>=0)contexts.push({first:begin,last:cp-1,flags:last});begin=cp;last=flags;}}
contexts.push({first:begin,last:0x10FFFF,flags:last});
const fixed=['','TOOLTIP','Tooltip','toggle','αΣ','ΑΣΑ','ΑΣʰ','ΑΣʰΑ','ʰΣ','AΣ\u0301','AΣ\u0301A','AΣ\u200d','AΣ\u200d\u0301B','A\u0301\u0301Σ','A\u0301 Σ','ΣΣΣ','İIı','ẞß','\ud800AΣ\udfff','\ud800\udc00Σ','𐐀𐐁𐐧','AΣ\u0301\u0301\u0301!','A\u200dΣ\u200d','\u0345Σ','A\u0345Σ','AΣ\u0345A'];
let seed=0x182806;const random=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed;};
const alphabet=['A','Σ','B','σ','ς','İ','𐐀','\u0301','\u200d','\u0345',"'",'ʰ','!',' ','\ud800','\udfff'];
for(let n=0;n<320;++n){let s='';for(let i=0,count=random()%48;i<count;++i)s+=alphabet[random()%alphabet.length];fixed.push(s);}
for(let n=0;n<128;++n){let s='';for(let i=0;i<24;++i)s+=String.fromCodePoint(random()%0x110000);fixed.push(s);}
for(const input of fixed)examples.push({input:units(input),output:units(input.toLowerCase())});
assert.ok(process.argv.includes('--check'));assert.deepEqual({changed,contexts,examples},JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p59-case-host-source.json'),'utf8')));console.log(JSON.stringify({scalarDomain:0x110000,mappings:changed.length,contextRanges:contexts.length,examples:examples.length,mode:'check'}));
