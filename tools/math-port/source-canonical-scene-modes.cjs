'use strict';

// Independent pinned MathJax Source witnesses for canonical scene inputs in
// separate none/local cache sessions. This is test evidence only.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const cp = require('node:child_process');
const {byteField, binary64, table} = require('./fixture-data.cjs');

const root = path.resolve(__dirname, '../..');
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const casesPath = path.join(__dirname, 'common-layout-reference/profile-cases.json');
const lockPath = path.join(__dirname, 'common-layout-reference/profile-source-lock.json');
const profileCases = JSON.parse(fs.readFileSync(casesPath, 'utf8'));
const sourceLock = JSON.parse(fs.readFileSync(lockPath, 'utf8'));
const {canonicalSceneCases} = require('./common-scene-cases.cjs');
const cases = canonicalSceneCases(profileCases);
assert.equal(cases.length, 75);
assert.equal(sha(fs.readFileSync(scopePath)), sourceLock.scopeHash, 'profile scope lock changed');
assert.equal(sha(fs.readFileSync(casesPath)), sourceLock.corpusSha256, '75-case corpus changed');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
for (const dependency of sourceLock.dependencies)
  assert.equal(sha(fs.readFileSync(path.join(packageRoot, dependency.file))), dependency.sha256,
    `pinned profile/font dependency changed: ${dependency.file}`);

function runChild(mode) {
  const evidence = path.join(root, `build/math-port/canonical-scene-modes-${mode}.json`);
  fs.mkdirSync(path.dirname(evidence), {recursive:true});
  try { fs.unlinkSync(evidence); } catch (error) {
    if (error.code !== 'ENOENT') throw error;
  }
  const child = cp.spawnSync(process.execPath,
    ['--max-old-space-size=512', __filename, '--capture-child', mode, evidence],
    {cwd:root, encoding:'utf8', maxBuffer:64 * 1024 * 1024,
      timeout:180000, windowsHide:true});
  if (child.stdout) process.stdout.write(child.stdout);
  if (child.stderr) process.stderr.write(child.stderr);
  if (child.error) throw child.error;
  assert.equal(child.signal, null, `${mode} Source child terminated by ${child.signal}`);
  assert.equal(child.status, 0, `${mode} Source child failed with status ${child.status}`);
  assert.ok(fs.existsSync(evidence), `${mode} Source child did not produce evidence`);
  const result = JSON.parse(fs.readFileSync(evidence, 'utf8'));
  assert.equal(result.mode, mode);
  assert.equal(result.records.length, 75);
  assert.deepEqual(result.records.map(x=>x.name), cases.map(x=>x.name));
  assert.equal(new Set(result.records.map(x=>x.name)).size, 75);
  for (const record of result.records) {
    assert.equal(record.error_name, '', `${mode}/${record.name}: Source conversion failed`);
    assert.ok(record.svg.includes('<svg'), `${mode}/${record.name}: missing Source SVG`);
    assert.ok(Object.values(record.metrics).every(Number.isFinite),
      `${mode}/${record.name}: nonfinite Source outer metrics`);
    assert.equal(record.initial_allow_tex_html, false,
      `${mode}/${record.name}: canonical initial allowTexHTML changed`);
    assert.equal(record.listener_dom_unchanged, true);
    assert.equal(record.listener_count, record.listeners.length);
    if (mode === 'none') assert.equal(record.local_cache_prefixes.length, 0);
    else assert.ok(record.local_cache_prefixes.length > 0);
  }
  return result;
}

function sceneFixtures(records, provenance) {
  const cases = table(
    ['name_bytes', 'tex_bytes', 'tag_side_bytes', 'requested_display',
      'em_binary64', 'ex_binary64', 'width_binary64'],
    records.map(r => [byteField(r.name), byteField(r.tex), byteField(r.tag_side),
      +r.requested_display, binary64(r.em), binary64(r.ex), binary64(r.width)]),
    [`Source session provenance SHA256: ${provenance}`]);
  const modes = table(
    ['case_index', 'mode_index', 'display', 'initial_allow_tex_html', 'width_binary64',
      'height_binary64', 'depth_binary64', 'error_name_bytes', 'error_message_bytes',
      'svg_bytes', 'prefix_count', 'prefixes_bytes_separated_by_pipe', 'listener_count',
      'listener_dom_unchanged'],
    records.flatMap((r, ci) => r.modes.map((m, mi) => [ci, mi, +m.display,
      +m.initial_allow_tex_html, binary64(m.metrics.width), binary64(m.metrics.height),
      binary64(m.metrics.depth), byteField(m.error_name), byteField(m.error_message),
      byteField(m.svg), m.local_cache_prefixes.length,
      m.local_cache_prefixes.map(value => byteField(value, true)).join('|'),
      m.listener_count, +m.listener_dom_unchanged])));
  return {'canonical-cases.tsv': cases, 'canonical-modes.tsv': modes};
}

function writeOrCheckFixture(records, provenance) {
  for (const [name, fixture] of Object.entries(sceneFixtures(records, provenance))) {
    const target = path.join(root, 'tests/math/fixtures/scenes', name);
    const previous = fs.existsSync(target) ? fs.readFileSync(target, 'utf8') : null;
    if (process.argv[2] === '--write-fixture') {
      if (previous !== fixture) fs.writeFileSync(target, fixture);
    } else assert.equal(previous, fixture,
      'Canonical hosted Source fixture differs; review before --write-fixture');
    process.stdout.write(`Verified ${records.length} canonical cases x 2 Source modes; ${name} SHA256 ${sha(Buffer.from(fixture))}\n`);
  }
}

async function captureChild(mode, evidencePath) {
  assert.ok(mode==='none'||mode==='local');
  const {createReference}=require('./reference-profile.cjs');
  const ref=createReference();
  const scope=JSON.parse(fs.readFileSync(scopePath,'utf8'));
  assert.deepEqual(ref.provenance.packages,scope.profile.tex_packages);
  ref.output.options.fontCache=mode;
  ref.output.options.localID=null;
  const fontCachePrototype=Object.getPrototypeOf(ref.output.fontCache);
  const originalUseLocalID=fontCachePrototype.useLocalID;
  let activePrefixes=null;
  fontCachePrototype.useLocalID=function(...args) {
    const result=originalUseLocalID.apply(this,args);
    if(activePrefixes) activePrefixes.push(this.localID);
    return result;
  };
  const litePrototype=Object.getPrototypeOf(ref.adaptor.node('svg',{}));
  const originalListenerDescriptor=Object.getOwnPropertyDescriptor(litePrototype,'addEventListener');
  const eventHosts=new WeakMap();
  let nextHost=1,activeListeners=null;
  litePrototype.addEventListener=function(type,handler) {
    assert.ok(activeListeners,'unexpected Source event registration outside a case');
    assert.equal(typeof type,'string');
    assert.equal(typeof handler,'function');
    let host=eventHosts.get(this);
    if(!host) { host={id:nextHost++, target:new EventTarget()}; eventHosts.set(this,host); }
    const before=ref.adaptor.serializeXML(this);
    host.target.addEventListener(type,handler);
    activeListeners.push({receiver:this,target:host.target,hostId:host.id,type,handler,before,
      after:ref.adaptor.serializeXML(this)});
  };
  const removeListeners=list=>{
    for(const item of list??[]) item.target.removeEventListener(item.type,item.handler);
    if(list) list.length=0;
  };
  const configuredAllow=ref.input.parseOptions.options.allowTexHTML;
  const records=[];
  const originalProcessMath=ref.output.processMath;
  let activeRoot=null,activeRootSeen=false;
  ref.output.processMath=function(wrapper,node) {
    const capture=!activeRootSeen;
    if(capture) activeRootSeen=true;
    const result=originalProcessMath.apply(this,arguments);
    if(capture) {
      const b=wrapper.getOuterBBox();
      activeRoot={display:this.math.display,metrics:{width:b.w,height:b.h,depth:b.d}};
    }
    return result;
  };
  try {
    for(const item of cases) {
      const initialAllow=ref.input.options.allowTexHTML ?? configuredAllow ?? false;
      let svg='',errorName='',errorMessage='';
      activePrefixes=[]; activeListeners=[]; activeRoot=null; activeRootSeen=false;
      try {
        ref.input.parseOptions.options.tagSide=item.tagSide??'right';
        const node=ref.document.convert(item.tex,{display:Boolean(item.display),
          em:item.em??16,ex:item.ex??8,containerWidth:item.width??1280});
        svg=ref.adaptor.serializeXML(node);
        const afterListeners=ref.adaptor.serializeXML(node);
        assert.equal(afterListeners,svg,`${item.name}: passive listener registration changed serialized DOM`);
      } catch(error) {
        errorName=error?.name||'Error'; errorMessage=String(error?.message||error);
      }
      const listeners=activeListeners;
      for(const listener of listeners) assert.equal(listener.before,listener.after,
        `${item.name}: event host registration mutated receiver DOM`);
      const root=activeRoot;
      assert.ok(root || errorName, `${item.name}: successful conversion did not expose root layout`);
      const result={name:item.name,display:root?.display??Boolean(item.display),
        initial_allow_tex_html:Boolean(initialAllow),
        metrics:root?.metrics??{width:0,height:0,depth:0},error_name:errorName,
        error_message:errorMessage,svg,local_cache_prefixes:activePrefixes.slice(),
        listener_count:listeners.length,listener_dom_unchanged:listeners.every(x=>x.before===x.after),
        listeners:listeners.map(x=>({type:x.type,host_id:x.hostId,before:x.before,after:x.after}))};
      records.push(result);
      process.stdout.write(`${mode} ${item.name}: ${errorName||'ok'} svg=${Buffer.byteLength(svg)} listeners=${listeners.length} prefixes=${activePrefixes.length}\n`);
      removeListeners(activeListeners);
      activeListeners=null; activePrefixes=null; activeRoot=null; activeRootSeen=false;
    }
  } finally {
    if(activeListeners) removeListeners(activeListeners);
    activeListeners=null; activePrefixes=null;
    ref.output.processMath=originalProcessMath;
    if (originalListenerDescriptor)
      Object.defineProperty(litePrototype,'addEventListener',originalListenerDescriptor);
    else delete litePrototype.addEventListener;
    fontCachePrototype.useLocalID=originalUseLocalID;
  }
  fs.writeFileSync(evidencePath,JSON.stringify({mode,provenance:ref.provenance,
    scope_sha256:sha(fs.readFileSync(scopePath)),input_corpus_sha256:sha(Buffer.from(JSON.stringify(cases))),records},null,2)+'\n');
}

async function main() {
  if(process.argv[2]==='--capture-child') {
    assert.equal(process.argv.length,5);
    await captureChild(process.argv[3],process.argv[4]);
    return;
  }
  assert.ok(process.argv.length===2||(process.argv.length===3&&process.argv[2]==='--write-fixture'),
    'usage: source-canonical-scene-modes.cjs [--write-fixture]');
  const none=runChild('none'),local=runChild('local');
  const combined=cases.map((item,index)=>({name:item.name,tex:item.tex,
    tag_side:item.tagSide??'',requested_display:Boolean(item.display),em:item.em??16,
    ex:item.ex??8,width:item.width??1280,
    modes:[none.records[index],local.records[index]]}));
  assert.deepEqual(combined.map(x=>x.name),cases.map(x=>x.name));
  const evidence={corpus_sha256:sha(Buffer.from(JSON.stringify(cases))),
    none:sha(fs.readFileSync(path.join(root,'build/math-port/canonical-scene-modes-none.json'))),
    local:sha(fs.readFileSync(path.join(root,'build/math-port/canonical-scene-modes-local.json')))};
  const evidencePath=path.join(root,'build/math-port/canonical-scene-modes.json');
  fs.writeFileSync(evidencePath,JSON.stringify({provenance:evidence,records:combined},null,2)+'\n');
  writeOrCheckFixture(combined,sha(Buffer.from(JSON.stringify(evidence))));
}
main().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
