// Regenerate full upstream states in ignored output; committed hashes freeze
// expectations independently of all native implementation packets.
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const zlib=require('node:zlib');
const cp=require('node:child_process');
const assert=require('node:assert/strict');
const {createCapture}=require('./reference-capture.cjs');
const root=path.resolve(__dirname,'../..');
const corpusPath=path.join(root,'tools/math-port/profiles/reference-corpus.json');
const goldenPath=path.join(root,'tools/math-port/profiles/evidence/reference-golden.json');
const sha=v=>crypto.createHash('sha256').update(v).digest('hex');
// Git's Windows autocrlf setting must not invalidate tracked tool/corpus
// hashes after checkout. JSON string contents remain escaped and unchanged.
const textSha=v=>sha(v.toString('utf8').replaceAll('\r\n','\n'));
const json=v=>JSON.stringify(v);
const corpus=JSON.parse(fs.readFileSync(corpusPath,'utf8'));
assert.equal(corpus.schema,1);
assert.equal(corpus.session,'ordered_single_production_profile');
const names=new Set();
for (const c of corpus.cases) {
  assert.match(c.name,/^[a-z0-9_]+$/);
  assert.equal(names.has(c.name),false,`Duplicate case ${c.name}`); names.add(c.name);
}
const requests=corpus.cases.map(c=>c.request);
// A fresh process with no hooks is essential: comparing against the already
// instrumented objects would fail to establish observer neutrality.
const baseline=cp.spawnSync(process.execPath,[path.join(__dirname,'reference-capture.cjs'),'--render-only'],
  {cwd:root,input:json(requests),encoding:'utf8',maxBuffer:64*1024*1024});
assert.equal(baseline.status,0,baseline.stderr);
const renders=JSON.parse(baseline.stdout);
assert.equal(renders.length,requests.length);
const {capture,reference}=createCapture();
const outputDir=path.join(root,'build/math-port-reference');
fs.mkdirSync(outputDir,{recursive:true});
const manifest={schema:1,corpus_sha256:textSha(fs.readFileSync(corpusPath)),
  provenance:reference.provenance,tools_sha256:{},neutrality:'all_serialized_results_or_exceptions_equal_fresh_uninstrumented_profile',cases:[]};
for (const file of ['reference-profile.cjs', 'reference-capture.cjs', 'reference-fixtures.cjs',
  'reference-compare.py', 'reference-mutations.py', 'validation/common.py', 'validation/reference.py'])
  manifest.tools_sha256[file]=textSha(fs.readFileSync(path.join(__dirname,file)));
for (let i=0;i<corpus.cases.length;i++) {
  const c=corpus.cases[i],snapshot=capture(c.request),plain=renders[i];
  if (snapshot.result.kind==='exception') assert.deepEqual({name:snapshot.result.name,message:snapshot.result.message},plain.exception,c.name+' exception neutrality');
  else assert.equal(snapshot.result.serialized,plain.serialized,c.name+' serialized neutrality');
  assert.equal(snapshot.result.kind,c.expected.kind,c.name+' result kind');
  if (c.expected.exception) assert.deepEqual({name:snapshot.result.name,message:snapshot.result.message},c.expected.exception,c.name);
  if (c.expected.warning) assert.ok(snapshot.diagnostics.some(d=>d.kind==='warning'&&d.message===c.expected.warning),c.name+' warning');
  if (c.expected.minimum_fragments) assert.ok(snapshot.result.fragments.length>=c.expected.minimum_fragments,c.name+' fragments');
  const encoded=json(snapshot);
  fs.writeFileSync(path.join(outputDir,c.name+'.json.gz'),zlib.gzipSync(encoded));
  const layers={};
  for (const key of ['parsed_mml','postfilters','diagnostics','layout_runs','placements','glyph_queries','result']) layers[key]=sha(json(snapshot[key]));
  manifest.cases.push({name:c.name,request_sha256:sha(json(c.request)),snapshot_sha256:sha(encoded),layers,
    result:snapshot.result.kind,fragments:snapshot.result.fragments?.length||0,
    warnings:snapshot.diagnostics.filter(d=>d.kind==='warning').map(d=>d.message)});
}
if (process.argv.includes('--freeze')) {
  // Root-only authoring action. Native agents never have permission to freeze.
  fs.writeFileSync(goldenPath,JSON.stringify(manifest,null,2)+'\n');
} else {
  assert.deepEqual(manifest,JSON.parse(fs.readFileSync(goldenPath,'utf8')),'Frozen reference changed; investigate instead of automatically rewriting expectations');
}
console.log(JSON.stringify({cases:manifest.cases.length,neutrality:true,reference_hashes:true,
  mode:process.argv.includes('--freeze')?'root_freeze':'check',output:'build/math-port-reference'},null,2));
