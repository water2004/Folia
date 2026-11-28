'use strict';
// Runtime oracle for MathJax 4.1.3 core/Tree/Visitor and its real Factory.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

function findRoot(start) {
  let current = path.resolve(start);
  while (true) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/math-port/common-layout-reference/profile-source-lock.json'))) {
      return current;
    }
    const parent = path.dirname(current);
    if (parent === current) return null;
    current = parent;
  }
}
const rootArg = process.argv.indexOf('--repo-root');
const root = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1] || '') :
  (findRoot(__dirname) || findRoot(process.cwd()));
assert.ok(root, 'pass --repo-root <repository> or run from a repository checkout');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const lockPath = path.join(root, 'tools/math-port/common-layout-reference/profile-source-lock.json');
const lock = JSON.parse(fs.readFileSync(lockPath, 'utf8'));
const expectedCjs = new Map(lock.dependencies.map(record => [record.file, record.sha256]));
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/profiles/scope.json'), 'utf8'));
const tsPath = '.reference/MathJax-src/ts/core/Tree/Visitor.ts';
const scopeRow = scope.files.find(record => record.source === 'ts/core/Tree/Visitor.ts');
assert.ok(scopeRow, 'scope does not contain Visitor.ts');
assert.equal(scopeRow.source_sha256, '29c372210da67492445b92a8a3a754c6d30fa91cc38c7f25eb5b4ffc51129106');
assert.equal(digest(path.join(root, tsPath)), scopeRow.source_sha256, 'pinned Visitor.ts changed');
const cjsFiles = ['src/cjs/core/Tree/Factory.js', 'src/cjs/core/Tree/Visitor.js'];
for (const file of cjsFiles) {
  const expected = expectedCjs.get(file);
  assert.ok(expected, `profile source lock lacks ${file}`);
  assert.equal(digest(path.join(packageRoot, file)), expected, `pinned CJS changed: ${file}`);
}
assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot, 'src/package.json'), 'utf8')).version,
  '4.1.3', 'MathJax package version');

const { AbstractFactory } = require(path.join(packageRoot, 'src/cjs/core/Tree/Factory.js'));
const { AbstractVisitor } = require(path.join(packageRoot, 'src/cjs/core/Tree/Visitor.js'));
class Product {
  constructor(factory, kind, id) {
    this.factory = factory;
    this.kind = kind;
    this.id = id;
    this.childNodes = [];
  }
}
class Branch extends Product { constructor(factory, id) { super(factory, 'branch', id); } }
class Leaf extends Product { constructor(factory, id) { super(factory, 'leaf', id); } }
class Result extends Product { constructor(factory, id) { super(factory, 'result', id); } }
const factory = new AbstractFactory({ branch: Branch, leaf: Leaf, result: Result });

class Visitor extends AbstractVisitor {
  visitLeafNode(node, argument) {
    argument.order.push(node.id);
    argument.received.push(argument.token);
    if (node.id === 1) argument.root.childNodes.push(factory.create('leaf', 2));
    return `leaf:${argument.token}:${node.id}`;
  }
  visitResultNode(_node, argument) { return `result:${argument.token}`; }
  visitGhostNode(_node, argument) { argument.ghostCalls++; }
}

const visitor = new Visitor(factory);
const branch = factory.create('branch', 0);
const first = factory.create('leaf', 1);
branch.childNodes.push(first);
const state = { order: [], received: [], root: branch, token: 61, ghostCalls: 0 };
const capturedMethod = visitor.visitLeafNode;
visitor.visitLeafNode = function () { throw new Error('post-construction method replacement must not apply'); };
const rootResult = visitor.visitTree(branch, state);
assert.equal(rootResult, undefined);
assert.deepEqual(state.order, [1, 2]); // Source for-of observes the appended child.
assert.deepEqual(state.received, [61, 61]);

const ghost = new Product(factory, 'ghost', 3);
visitor.visitNode(ghost, state);
assert.equal(state.ghostCalls, 0); // method exists but kind was absent from factory snapshot.

let replacementCalls = 0;
visitor.setNodeHandler('leaf', (node, argument) => {
  replacementCalls++;
  return `replacement:${argument.token}:${node.id}`;
});
assert.equal(visitor.visitNode(first, state), 'replacement:61:1');
assert.equal(replacementCalls, 1);
visitor.removeNodeHandler('leaf');
visitor.visitNode(first, state);
assert.equal(replacementCalls, 1);
visitor.setNodeHandler('leaf', null);
visitor.visitNode(first, state);
assert.equal(replacementCalls, 1); // empty handler selects default traversal.

const result = visitor.visitTree(factory.create('result', 9), state);
assert.equal(result, 'result:61');
assert.equal(typeof capturedMethod, 'function');

const mutations = [];
class MutatingVisitor extends AbstractVisitor {
  constructor(factory) {
    super(factory);
    this.calls = 0;
  }
  visitLeafNode(node) {
    this.removeNodeHandler('leaf');
    mutations.push(`removed:${node.id}`);
  }
  visitResultNode(node) {
    this.setNodeHandler('result', function (nextNode) {
      mutations.push(`replacement:${nextNode.id}`);
    });
    mutations.push(`replaced:${node.id}`);
  }
  visitBranchNode(node) {
    mutations.push(`state:${++this.calls}:${node.id}`);
  }
}
const mutatingVisitor = new MutatingVisitor(factory);
mutatingVisitor.visitNode(first, state); // self-removal must complete safely
const resultNode = factory.create('result', 10);
mutatingVisitor.visitNode(resultNode, state); // self-replacement must complete safely
mutatingVisitor.visitNode(resultNode, state);
const branchNode = factory.create('branch', 11);
mutatingVisitor.visitNode(branchNode, state);
mutatingVisitor.visitNode(branchNode, state);
assert.deepEqual(mutations, [
  'removed:1', 'replaced:10', 'replacement:10', 'state:1:11', 'state:2:11',
]);
const witness = {
  source: {
    version: '4.1.3',
    files: {
      [tsPath]: scopeRow.source_sha256,
      ...Object.fromEntries(cjsFiles.map(file => [file, expectedCjs.get(file)])),
    },
  },
  visitor: {
    factory_kind_snapshot: factory.getKinds(),
    method_captured_before_replacement: true,
    live_default_preorder: state.order,
    same_typed_argument_forwarded: state.received,
    unregistered_method_filtered: state.ghostCalls === 0,
    handler_replacement_result: 'replacement:61:1',
    removed_and_empty_handler_select_default: replacementCalls === 1,
    typed_handler_result: result,
    default_root_result: rootResult,
    self_removal_and_replacement: mutations.slice(0, 3),
    mutable_handler_state: mutations.slice(3),
  },
};
const output = path.join(root, 'build/math-port/tree-visitor-source-witness.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(witness, null, 2) + '\n');
process.stdout.write(JSON.stringify({ output: path.relative(root, output), kinds: witness.visitor.factory_kind_snapshot,
  liveTraversal: witness.visitor.live_default_preorder, typedResult: result }) + '\n');
