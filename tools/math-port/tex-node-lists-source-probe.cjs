'use strict';
// Focused pinned-CJS observation of NodeFactory.create -> ParseOptions.addNode
// and the filter-time getList rewrite for real xArrow parsing.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const pkg = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const scopeBytes = fs.readFileSync(path.join(root, 'tools/math-port/profiles/scope.json'));
const scope = JSON.parse(scopeBytes.toString('utf8'));
assert.equal(crypto.createHash('sha256').update(scopeBytes).digest('hex'),
  '9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1');
const locks = {
  'input/tex.js': 'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
  'input/tex/NodeFactory.js': '426e3ca8822d209edc8a22b5e9f11c5631e52213b232220a2a55e81828374db5',
  'input/tex/ParseOptions.js': 'ff19d70539cb76911c5f1d7673ddfd908183dcf5849c91cb068b19e58da8a9ec',
  'input/tex/FilterUtil.js': '2e626da51d95be455ddde976660abb8ebab035a8712db0f0820870b95e9b1bd8',
  'input/tex/ams/AmsConfiguration.js': 'ff8e1aa5936fb48e68da3cc5ff8745f1365d5dbd2416c1a908c6a06842989f79',
  'core/MmlTree/MmlFactory.js': 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'core/MathItem.js': 'e6ed5008854e4d2ac6b34ab98c53bec6afb34e460895e8320c90a0d8d700d261'
};
for (const [file, expected] of Object.entries(locks)) {
  const bytes = fs.readFileSync(path.join(pkg, 'cjs', file));
  assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'), expected, file);
}
for (const source of ['ts/input/tex/NodeFactory.ts', 'ts/input/tex/ParseOptions.ts',
  'ts/input/tex/FilterUtil.ts']) {
  const lock = scope.files.find(file => file.source === source);
  assert.ok(lock, `scope missing ${source}`);
  const bytes = fs.readFileSync(path.join(root, '.reference/MathJax-src', source));
  assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'), lock.source_sha256, source);
  assert.equal(fs.readFileSync(path.join(pkg, source), 'utf8').replaceAll('\r\n', '\n'),
    bytes.toString('utf8').replaceAll('\r\n', '\n'), `${source} installed Source`);
}

const load = file => require(path.join(pkg, 'cjs', file));
const { TeX } = load('input/tex.js');
const NodeFactory = load('input/tex/NodeFactory.js').NodeFactory;
const ParseOptions = load('input/tex/ParseOptions.js').default;
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const { AbstractMathItem } = load('core/MathItem.js');
load('input/tex/ams/AmsConfiguration.js');

const events = [];
const originalCreate = NodeFactory.prototype.create;
const originalAddNode = ParseOptions.prototype.addNode;
const originalGetList = ParseOptions.prototype.getList;
NodeFactory.prototype.create = function(kind, ...rest) {
  const actualKind = rest[0];
  const result = originalCreate.call(this, kind, ...rest);
  if (kind === 'node') events.push({ op: 'create-return', kind: actualKind, nodeKind: result?.kind });
  return result;
};
ParseOptions.prototype.addNode = function(property, node) {
  const before = this.nodeLists[property]?.length || 0;
  const result = originalAddNode.call(this, property, node);
  events.push({ op: 'addNode', property, nodeKind: node.kind, before, after: this.nodeLists[property]?.length || 0 });
  return result;
};
ParseOptions.prototype.getList = function(property) {
  const before = (this.nodeLists[property] || []).map(node => node?.kind || null);
  const result = originalGetList.call(this, property);
  events.push({ op: 'getList', property, rootKind: this.root?.kind || null,
    before, after: result.map(node => node?.kind || null) });
  return result;
};

let outcome;
try {
  const jax = new TeX({ packages: ['base', 'ams'] });
  jax.setMmlFactory(new MmlFactory());
  const math = new AbstractMathItem('\\xrightarrow[]{}+\\xleftarrow{}', jax, true);
  const node = jax.compile(math, null);
  outcome = { rootKind: node?.kind || null, result: 'ok' };
} catch (error) {
  outcome = { result: null, error: { name: error.name || error.constructor.name,
    id: error.id ?? null, message: String(error.message) } };
} finally {
  NodeFactory.prototype.create = originalCreate;
  ParseOptions.prototype.addNode = originalAddNode;
  ParseOptions.prototype.getList = originalGetList;
}
const output = { provenance: locks, outcome, events };
const destination = path.join(root, 'build/math-port/tex-node-lists-source-probe.json');
fs.mkdirSync(path.dirname(destination), { recursive: true });
fs.writeFileSync(destination, JSON.stringify(output, null, 2) + '\n');
console.log(JSON.stringify({ result: outcome.result, events: events.length,
  destination: path.relative(root, destination) }));
