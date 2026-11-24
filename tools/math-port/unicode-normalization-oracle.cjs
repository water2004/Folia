// Root original String.normalize('NFD') observations, Unicode 16.0 host profile.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),source=fs.readFileSync(path.join(root,'.reference/unicode16/NormalizationTest.txt'));
const hash=crypto.createHash('sha256').update(source).digest('hex');
assert.equal(hash,'d811971453e7075e1ad56fb1b301eece5aa80757b81f6156e74a1bfb3ae5ceb1');
assert.equal(process.versions.unicode,'16.0');
const from=x=>String.fromCodePoint(...x.trim().split(' ').filter(Boolean).map(x=>parseInt(x,16)));
const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
const rows=[];
for(const line of source.toString().split(/\r?\n/)){
 const content=line.split('#')[0].trim();if(!content||content.startsWith('@'))continue;
 const fields=content.split(';').slice(0,5),inputs=fields.map(from),outputs=inputs.map(x=>x.normalize('NFD'));
 assert.equal(fields.length,5);for(let i=0;i<5;++i)assert.equal(outputs[i],inputs[i<3?2:4]);
 rows.push({inputs,outputs});
}
const extras=['','abc','é','Å','Å','ά','𝔸','ﬁ','\u212b\u0315\u0300','a\u0315\u0300','\u0315\u0300','a\u0301\u0300','a\u034f\u0315\u0300','a\u200d\u0315\u0300','\ud800','\udc00','\ud800é\udc00','a\u0315\ud800\u0300','😀é𝔸','\ud800\ud800\udc00\udc00','\u0000é\u0000'];
let state=0x514e4644;const random=()=>{state^=state<<13;state^=state>>>17;state^=state<<5;return state>>>0;};
const pool=[...'aAéÅάᾂÅ한각𝔸😀'].map(x=>x).concat(['\u0315','\u0300','\u0323','\u034f','\u0301','\ud800','\udc00','\u200d','\ufe0f']);
for(let i=0;i<64;++i){let input='';for(let j=0;j<40;++j)input+=pool[random()%pool.length];extras.push(input);}
const extraRows=extras.map(input=>({input,output:input.normalize('NFD')}));
const planeDigests=[];for(let plane=0;plane<=16;++plane){let digest=0xcbf29ce484222325n;const update=x=>{digest=BigInt.asUintN(64,(digest^BigInt(x))*0x100000001b3n);};for(let cp=plane*65536;cp<(plane+1)*65536;++cp){const output=String.fromCodePoint(cp).normalize('NFD');update(cp);update(output.length);for(let i=0;i<output.length;++i)update(output.charCodeAt(i));}planeDigests.push(digest.toString(16));}
const corpus = rows.map(row => [...row.inputs, ...row.outputs].map(hex).join('\t')).join('\n') + '\n';
const {utf16Field, writeTables} = require('./fixture-data.cjs');
writeTables(path.join(root, 'tests/math/fixtures/unicode'), {
  'normalization-metadata': {
    columns: ['name', 'value'],
    metadata: ['source=hash-locked Unicode 16 corpus and actual Node NFD.', `corpus_sha256=${hash}`],
    rows: [['corpus_rows', rows.length]]
  },
  'normalization-sequences': {columns: ['input_utf16', 'output_utf16'],
    rows: extraRows.map(row => [utf16Field(row.input), utf16Field(row.output)])},
  'normalization-digests': {columns: ['plane', 'digest_hex64'],
    rows: planeDigests.map((digest, plane) => [plane, digest.padStart(16, '0')])}
}, !process.argv.includes('--check'));
const manifest=JSON.stringify({stage:'original NFD host observations; not French math-style algorithm acceptance',node:process.version,icu:process.versions.icu,unicode:process.versions.unicode,corpus_sha256:hash,corpus_rows:rows.length,corpus_comparisons:rows.length*5,extra_rows:extraRows.map(x=>({input:hex(x.input),output:hex(x.output)})),plane_digests:planeDigests,plane_codepoints:17*65536},null,2)+'\n';
for(const[file,data]of [['tests/math/fixtures/unicode/normalization-corpus.tsv',corpus],['docs/math-port/evidence/p54-unicode-normalization-source.json',manifest]]){const target=path.join(root,file);if(process.argv.includes('--check'))assert.equal(fs.readFileSync(target,'utf8').replaceAll('\r\n','\n'),data);else { fs.mkdirSync(path.dirname(target), {recursive:true}); fs.writeFileSync(target,data); }}
console.log(JSON.stringify({corpus_rows:rows.length,corpus_comparisons:rows.length*5,sequence_cases:extraRows.length,planes:planeDigests.length,all_codepoints:17*65536}));
