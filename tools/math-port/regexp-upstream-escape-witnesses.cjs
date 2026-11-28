// Root independently classifies upstream expectations that conflict with Node.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
assert.equal(process.version,'v24.13.0');const root=path.resolve(__dirname,'../..');
const source=fs.readFileSync(path.join(root,'.reference/regress/tests/tests.rs')),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const inputs=[
 {upstream_test:'run_regexp_unicode_escape',pattern:'\\u{1}',flags:'',text:'\u0001'},
 {upstream_test:'test_escapes_folding',pattern:'\\u{41}',flags:'',text:'A'},
 {upstream_test:'test_high_folds',pattern:'\\u{100}',flags:'',text:'\u0100'}
];
for(const c of inputs)assert.ok(source.toString('utf8').includes(c.pattern),'Actual unchanged upstream pattern');
const cases=inputs.map(c=>{const m=new RegExp(c.pattern,c.flags+'d').exec(c.text);return {...c,upstream_expected_match:true,node_expected_match:!!m,node_ranges:m?.indices??null};});
assert.ok(cases.every(c=>!c.node_expected_match));
const report={stage:'upstream legacy braced-escape expectations conflict with actual Node; not waived or engine accepted',node:process.version,upstream_test_source_sha256:hash(source),cases};
const file=path.join(root,'tools/math-port/profiles/evidence/p21b-upstream-escape-witnesses.json');
if(process.argv.includes('--check'))assert.deepEqual(JSON.parse(fs.readFileSync(file)),report);else fs.writeFileSync(file,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({upstream_conflicting_tests:3,mode:process.argv.includes('--check')?'check':'generate'}));
