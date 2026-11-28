'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
function findRoot(start=__dirname) {
  for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))
    if(fs.existsSync(path.join(current,'tools/math-port/profiles/scope.json')) && fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  return null;
}
const rootArg=process.argv.indexOf('--repo-root');
assert.ok(rootArg<0 ? process.argv.length===2 : (process.argv[rootArg+1] && process.argv.length===rootArg+2),'usage: node common-fence-action-source-witness.cjs [--repo-root <repository>]');
const root=rootArg<0?findRoot():path.resolve(process.argv[rootArg+1]);
assert.ok(root,'pass --repo-root <repository> or run from within the repository');
const source = path.join(root, '.reference/MathJax-src/ts/output/common/Wrappers');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const sha256 = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const lock=JSON.parse(fs.readFileSync(path.join(root,'tests/math/fixtures/common-source/fence-action-source-lock.json'),'utf8'));
assert.equal(JSON.parse(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))).version,lock.version,'MathJax package version');
for(const [file,hash] of Object.entries(lock.sourceTsSha256)) assert.equal(sha256(path.join(root,file)),hash,`pinned Source changed: ${file}`);
const { CommonMfencedMixin } = require(path.join(packageRoot, 'output/common/Wrappers/mfenced.js'));
const { CommonMactionMixin } = require(path.join(packageRoot, 'output/common/Wrappers/maction.js'));
for(const [relative,hash] of Object.entries(lock.cjsSha256)) assert.equal(sha256(path.join(packageRoot,relative)),hash,`installed CJS differs from pinned build: ${relative}`);

function CommonWrapper(factory, node, parent = null) {
  this.factory = factory;
  this.node = node;
  this.parent = parent;
  this.childNodes = node.initialChildWrappers || [];
}
CommonWrapper.prototype.wrap = function(node) { return this.factory.wrap(node, this); };
CommonWrapper.prototype.setChildPWidths = function(recompute) { this.lastPWidthRecompute = recompute; };
CommonWrapper.prototype.getChildLineBBox = function(child, index) { return child.getLineBBox(index); };
function InferredRow(factory, node, parent) {
  CommonWrapper.call(this, factory, node, parent);
  this.stretched = false;
  this.box = { w: 2, h: 3, d: 1 };
}
InferredRow.prototype = Object.create(CommonWrapper.prototype);
InferredRow.prototype.constructor = InferredRow;
InferredRow.prototype.stretchChildren = function() { this.stretched = true; };
Object.defineProperty(InferredRow.prototype, 'breakCount', { get() { return this.childNodes.length; } });
InferredRow.prototype.getOuterBBox = function() { return this.box; };
InferredRow.prototype.getLineBBox = function(index) { return { index, row: true }; };
const Fenced = CommonMfencedMixin(CommonWrapper);
const fencedFactory = {
  wrap(node, parent) {
    const wrapper = node.kind === 'inferredMrow'
      ? new InferredRow(this, node, parent)
      : new CommonWrapper(this, node, parent);
    this.wrapped.push(wrapper);
    return wrapper;
  },
  wrapped: [],
  inferredNode: {
    kind: 'inferredMrow',
    inheritAttributesFrom(source) { this.inheritedFrom = source; }
  },
  mmlFactory: { create(kind) { assert.equal(kind, 'inferredMrow'); return fencedFactory.inferredNode; } }
};
const open = { kind: 'open' }, separator = { kind: 'separator' }, close = { kind: 'close' };
const firstWrapper = { node: { kind: 'first' } }, secondWrapper = { node: { kind: 'second' } };
const fencedNode = { factory: fencedFactory.mmlFactory, open, separators: [separator], close,
  initialChildWrappers: [firstWrapper, secondWrapper] };
const fence = new Fenced(fencedFactory, fencedNode);
assert.deepEqual(fence.mrow.childNodes.map(wrapper => wrapper.node.kind),
  ['open', 'first', 'separator', 'second', 'close']);
assert.equal(fence.mrow.parent, fence);
assert.equal(fence.mrow.stretched, true);
assert.equal(fence.breakCount, 5);
const fenceBox = { updateFrom(value) { Object.assign(this, value); } };
fence.computeBBox(fenceBox, true);
assert.deepEqual([fenceBox.w, fenceBox.h, fenceBox.d], [2, 3, 1]);
assert.equal(fence.lastPWidthRecompute, true);
assert.deepEqual(fence.computeLineBBox(4), { index: 4, row: true });

function ActionBase(factory, node, parent) { CommonWrapper.call(this, factory, node, parent); }
ActionBase.prototype = Object.create(CommonWrapper.prototype);
ActionBase.prototype.constructor = ActionBase;
ActionBase.prototype.length2em = function(value) { return Number.parseFloat(value); };
ActionBase.prototype.getOuterBBox = function() { return { w: this.node.w || 0, h: this.node.h || 0, d: this.node.d || 0 }; };
Object.defineProperty(ActionBase.prototype, 'breakCount', { get() { return this.node.breakCount || 0; } });
ActionBase.prototype.getLineBBox = function(index) { return { index, child: this.node.kind }; };
ActionBase.prototype.coreMO = function() { return this.node.mo || null; };
const Action = CommonMactionMixin(ActionBase);
Action.actions = new Map();
const actionFactory = {
  wrap(node, parent) { const wrapper = new ActionBase(this, node, parent); this.wrapped.push(wrapper); return wrapper; },
  wrapped: []
};
const children = [{ kind: 'first', w: 1, breakCount: 2 }, { kind: 'second', w: 4, breakCount: 7 }];
const attrs = new Map([['selection', 2], ['data-offsets', '1em -2em']]);
const actionNode = {
  initialChildWrappers: children.map(node => new ActionBase(actionFactory, node)),
  attributes: { get(key) { return attrs.get(key); } },
  isEmbellished: false,
  get selected() {
    const raw = attrs.get('selection');
    if (Number.isNaN(raw)) return { kind: 'mrow' };
    const i = Math.max(1, Math.min(children.length, raw)) - 1;
    return children[i] || { kind: 'mrow' };
  }
};
const action = new Action(actionFactory, actionNode);
assert.equal(action.selected, action.childNodes[1]);
assert.equal(action.tipDx, 1);
assert.equal(action.tipDy, -2);
const actionBox = { updateFrom(value) { Object.assign(this, value); } };
action.computeBBox(actionBox, false);
assert.equal(actionBox.w, 4);
assert.equal(action.childNodes[1].lastPWidthRecompute, false);
assert.equal(action.breakCount, 7);
assert.deepEqual(action.computeLineBBox(3), { index: 3, child: 'second' });

attrs.set('selection', 0);
assert.equal(action.selected, action.childNodes[0]);
attrs.set('selection', Number.NaN);
const fallback = action.selected;
assert.equal(fallback.node.kind, 'mrow');
assert.equal(fallback.parent, action);
assert.equal(action.childNodes.length, 2);

const report={version:lock.version,source:'pinned CommonMfenced/CommonMaction mixins with isolated wrapper fixtures',cases:{fenced:{childOrder:fence.mrow.childNodes.map(wrapper=>wrapper.node.kind),parentLinked:fence.mrow.parent===fence,stretched:fence.mrow.stretched,breakCount:fence.breakCount,bbox:[fenceBox.w,fenceBox.h,fenceBox.d],lineBBox:fence.computeLineBBox(4)},action:{selectedKind:action.selected.node.kind,tipOffset:[action.tipDx,action.tipDy],bboxWidth:actionBox.w,breakCount:action.breakCount,lineBBox:action.computeLineBBox(3),clampedFirst:action.selected===action.childNodes[0],fallbackKind:fallback.node.kind,fallbackParentLinked:fallback.parent===action,normalChildCount:action.childNodes.length}}};
const output=path.join(root,'build/math-port/common-source/fence-action-source.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(`verified fence/action cases; evidence ${path.relative(root,output)}`);
