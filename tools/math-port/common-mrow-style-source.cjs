'use strict';

// Development-only pinned Source observation; native rendering never runs it.
// Own style is added through the MML attributes API after real TeX compilation.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const { createReference } = require('./reference-profile.cjs');

const ref = createReference();
const rootDir = path.resolve(__dirname, '../..');
const load = file => ref.require('@mathjax/src/js/' + file);
const { TeX } = load('input/tex.js');
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const { AbstractMathItem } = load('core/MathItem.js');
const { adaptor, output, document } = ref;

function tree(node) {
  return {
    kind: node.kind,
    text: node.isToken ? node.getText() : undefined,
    children: node.childNodes.map(tree),
  };
}

function run(label, tex, style) {
  const input = new TeX({ packages: ['base'] });
  input.setMmlFactory(new MmlFactory());
  const item = new AbstractMathItem(tex, input, false);
  item.setMetrics(16, 8, 1280, 1);
  const root = input.compile(item, document);
  item.root = root;
  const rows = [];
  root.walkTree(node => {
    if (node.kind === 'inferredMrow') rows.push(node);
  });
  assert.equal(rows.length, 1, `${label}: expected one compiled inferred row`);
  const row = rows[0];
  if (style) {
    // Ordinary authored CSS on this real compiled MmlNode. No wrapper method
    // or property is replaced; this probes a conditional MML attribute state.
    row.attributes.set('style', style);
  }

  output.setDocument(document);
  output.math = item;
  output.pxPerEm = 8 / output.font.params.x_height;
  output.nodeMap = new Map();
  root.setTeXclass(null);
  const mathWrapper = output.factory.wrap(root);
  const wrapper = output.nodeMap.get(row);
  assert.ok(wrapper, `${label}: wrapper missing from actual factory map`);
  wrapper.getBBox();
  const dxBefore = wrapper.dx;
  const parent = adaptor.node('g');
  output.container = parent;
  mathWrapper.toSVG([parent]);
  const child = wrapper.childNodes[0];
  const rendered = adaptor.serializeXML(parent);
  const record = {
    label,
    tex,
    actualMml: tree(root),
    kind: row.kind,
    linebreakCount: wrapper.linebreakCount,
    isStack: wrapper.isStack,
    style: row.attributes.get('style'),
    styleData: wrapper.styleData,
    dxBefore,
    dxAfterActualToSVG: wrapper.dx,
    childDx: child && child.dx,
    childBBox: child && child.bbox,
    renderedParentSvg: rendered,
    upstreamCommit: ref.provenance.upstream.commit,
    profileSha256: ref.provenance.profile_sha256,
    packageVersion: '4.1.3',
  };
  console.log(JSON.stringify(record));
  output.nodeMap = null;
  return record;
}

const ordinary = run('inferred_no_style', 'x+y', null);
assert.equal(ordinary.linebreakCount, 0);
assert.equal(ordinary.dxAfterActualToSVG, 0);

const unbroken = run(
  'inferred_with_own_padding_border_no_break',
  'x+y',
  'padding-left: 1em; border-left: .125em solid red',
);
assert.equal(unbroken.linebreakCount, 0);
assert.equal(unbroken.dxAfterActualToSVG, 0);

const broken = run(
  'inferred_forced_break',
  String.raw`x\mmlToken{mo}[linebreak="newline" linebreakstyle="before"]{+}y`,
  'padding-left: 1em; border-left: .125em solid red',
);
assert.equal(broken.linebreakCount, 1);
assert.equal(broken.dxAfterActualToSVG, 1.125);

// Reachability check: the public TeX mmlToken command does not create a
// styled inferred mrow; it rejects mrow as a token element.
const reachabilityInput = new TeX({ packages: ['base'] });
reachabilityInput.setMmlFactory(new MmlFactory());
const reachabilityTex = String.raw`\mmlToken{mrow}{x}`;
const reachabilityItem = new AbstractMathItem(reachabilityTex, reachabilityInput, false);
const reachabilityRoot = reachabilityInput.compile(reachabilityItem, document);
const mmlErrors = [];
reachabilityRoot.walkTree(node => {
  if (node.kind === 'merror') mmlErrors.push(node.attributes.get('data-mjx-error'));
});
assert.ok(mmlErrors.some(error => error === 'mrow is not a token element'));
console.log(JSON.stringify({
  label: 'tex_mmlToken_mrow_reachability',
  tex: reachabilityTex,
  mmlErrors,
  note: 'Actual TeX compile with base package; inferred-row own style above was injected through MmlNode.attributes.set.',
  upstreamCommit: ref.provenance.upstream.commit,
  profileSha256: ref.provenance.profile_sha256,
  packageVersion: '4.1.3',
}));

const scope = JSON.parse(fs.readFileSync(path.join(rootDir, 'tools/math-port/profiles/scope.json')));
const modules = [
  'ts/core/MmlTree/MmlNodes/mrow.ts',
  'ts/output/common/Wrapper.ts',
  'ts/output/common/Wrappers/mrow.ts',
  'ts/output/svg/Wrapper.ts',
  'ts/output/svg/Wrappers/mrow.ts',
];
const sourceModuleHashes = Object.fromEntries(modules.map(source => {
  const locked = scope.files.find(item => item.source === source);
  assert.ok(locked, `scope lock missing ${source}`);
  const bytes = fs.readFileSync(path.join(rootDir, '.reference/MathJax-src', source));
  const sha256 = crypto.createHash('sha256').update(bytes).digest('hex');
  assert.equal(sha256, locked.source_sha256, `pinned source hash mismatch ${source}`);
  const cjs = source.replace(/^ts\//, 'cjs/').replace(/\.ts$/, '.js');
  const compiledBytes = fs.readFileSync(path.join(
    rootDir, 'tools/mathjax-bundle/node_modules/@mathjax/src', cjs,
  ));
  return [source, {
    sourceSha256: sha256,
    compiledCjsSha256: crypto.createHash('sha256').update(compiledBytes).digest('hex'),
  }];
}));
console.log(JSON.stringify({
  label: 'pinned_source_module_hashes',
  upstreamCommit: ref.provenance.upstream.commit,
  packageVersion: '4.1.3',
  profileSha256: ref.provenance.profile_sha256,
  sourceModuleHashes,
}));
