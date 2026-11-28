'use strict';
// Compact pinned Source oracle for the Factory/Wrapper method roles replaced
// by this packet. No generated bundle or native executable is involved.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

function repoRoot(start) {
  let current = path.resolve(start);
  while (true) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/math-port/common-layout-reference/profile-source-lock.json'))) return current;
    const parent = path.dirname(current);
    if (parent === current) return null;
    current = parent;
  }
}
const rootFlag = process.argv.indexOf('--repo-root');
const root = rootFlag >= 0 ? path.resolve(process.argv[rootFlag + 1] || '') :
  (repoRoot(__dirname) || repoRoot(process.cwd()));
assert.ok(root, 'pass --repo-root <repository> or run from a repository checkout');
const packages = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const lock = JSON.parse(fs.readFileSync(path.join(root,
  'tools/math-port/common-layout-reference/profile-source-lock.json'), 'utf8'));
const files = [
  'src/cjs/core/Tree/Factory.js',
  'src/cjs/core/Tree/WrapperFactory.js',
  'src/cjs/core/Tree/Wrapper.js',
];
const pinned = new Map(lock.dependencies.map(row => [row.file, row.sha256]));
const hashes = {};
for (const file of files) {
  const expected = pinned.get(file);
  assert.ok(expected, `source lock lacks ${file}`);
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(packages, file))).digest('hex');
  assert.equal(actual, expected, `pinned CJS changed: ${file}`);
  hashes[file] = actual;
}
assert.equal(JSON.parse(fs.readFileSync(path.join(packages, 'src/package.json'), 'utf8')).version, '4.1.3');

const { AbstractFactory } = require(path.join(packages, 'src/cjs/core/Tree/Factory.js'));
const { AbstractWrapperFactory } = require(path.join(packages, 'src/cjs/core/Tree/WrapperFactory.js'));
const { AbstractWrapper } = require(path.join(packages, 'src/cjs/core/Tree/Wrapper.js'));

class BaseProduct { constructor(factory, value) { this.factory = factory; this.value = value; } }
class DerivedProduct extends BaseProduct {}
const factory = new AbstractFactory({ base: BaseProduct, derived: DerivedProduct });
factory.defaultKind = 'base';
const oldCreator = factory.node.derived;
factory.setNodeClass('derived', class Replaced extends BaseProduct {});
const capturedBeforeReplacement = oldCreator.call(null, 7);
assert.equal(capturedBeforeReplacement.constructor, DerivedProduct);
const baseProduct = factory.create('not-registered', 9);
assert.equal(baseProduct.value, 9);
assert.equal(factory.nodeIsKind(capturedBeforeReplacement, 'base'), true);
assert.deepEqual(factory.getKinds(), ['base', 'derived']);

const constructorFactory = new AbstractFactory({ first: BaseProduct, second: DerivedProduct });
assert.deepEqual(constructorFactory.getKinds(), ['first', 'second']);
let missingDefaultThrew = false;
try { constructorFactory.create('missing', 1); }
catch (error) { missingDefaultThrew = error instanceof TypeError; }
assert.equal(missingDefaultThrew, true);
constructorFactory.defaultKind = 'second';
assert.equal(constructorFactory.create('missing', 2).constructor, DerivedProduct);

class Source { constructor(value) { this.value = value; this.kind = 'wrapped'; } }
class Wrapped extends AbstractWrapper {
  constructor(factory, source, parent) {
    super(factory, source);
    this.parent = parent;
    this.childNodes = [];
  }
}
class WrappedFactory extends AbstractWrapperFactory {}
const wrapperFactory = new WrappedFactory({ wrapped: Wrapped });
const sourceRoot = new Source(1), sourceChild = new Source(2);
const rootWrapper = wrapperFactory.wrap(sourceRoot, 'root-parent');
const childWrapper = wrapperFactory.wrap(sourceChild, rootWrapper);
assert.equal(rootWrapper.node, sourceRoot);
assert.equal(rootWrapper.parent, 'root-parent');
assert.equal(childWrapper.parent, rootWrapper);
rootWrapper.childNodes.push(childWrapper);
const walk = [];
const data = { shared: true };
const returned = rootWrapper.walkTree((wrapper, state) => {
  assert.equal(state, data);
  walk.push(wrapper.node.value);
  if (wrapper === rootWrapper) wrapper.childNodes.push(new Wrapped(wrapperFactory, new Source(3), null));
}, data);
assert.equal(returned, data);
assert.deepEqual(walk, [1, 2, 3]);

const witness = {
  schema: 1,
  source: { version: '4.1.3', files: hashes },
  factory: {
    constructor_captured_before_replacement: capturedBeforeReplacement.constructor.name,
    requested_missing_kind_falls_back_to: baseProduct.constructor.name,
    base_instance_predicate_accepts_derived: true,
    registry_order: factory.getKinds(),
    constructor_ingestion_order: constructorFactory.getKinds(),
    missing_without_registered_default_throws: missingDefaultThrew,
    fallback_after_explicit_default: constructorFactory.create('missing', 3).constructor.name,
  },
  wrapper: {
    source_kind: rootWrapper.kind,
    source_forwarded: rootWrapper.node === sourceRoot,
    parent_forwarded: rootWrapper.parent === 'root-parent' && childWrapper.parent === rootWrapper,
    preorder_with_live_append: walk,
    returns_same_data: returned === data,
  },
};
const output = path.join(root, 'build/math-port/typed-tree-factory/tree-factory-source-witness.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(witness, null, 2) + '\n');
console.log(JSON.stringify({ output: path.relative(root, output), sourceVersion: witness.source.version,
  registryOrder: witness.factory.registry_order, walk: witness.wrapper.preorder_with_live_append }));
