'use strict';
// Source-only oracle for the pinned MathJax 4.1.3 Common linebreak visitor.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

function findRoot(start) {
  let current = path.resolve(start);
  for (;;) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/math-port/common-layout-reference/profile-source-lock.json'))) return current;
    const parent = path.dirname(current);
    if (parent === current) return null;
    current = parent;
  }
}
const rootIndex = process.argv.indexOf('--repo-root');
assert.ok(rootIndex < 0 ? process.argv.length === 2 :
  (process.argv[rootIndex + 1] && process.argv.length === rootIndex + 2),
  'usage: node common-linebreak-source-witness.cjs [--repo-root <repository>]');
const root = rootIndex >= 0 ? path.resolve(process.argv[rootIndex + 1]) :
  findRoot(__dirname) || findRoot(process.cwd());
assert.ok(root, 'pass --repo-root <repository> or run from the repository');
const expectations = JSON.parse(fs.readFileSync(
  path.join(root, 'tests/math/fixtures/common-source/linebreak-source-expectations.json'), 'utf8'));
const scope = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/profiles/scope.json'), 'utf8'));
const tsFile = 'ts/output/common/LinebreakVisitor.ts';
const sourceRecord = scope.files.find(record => record.source === tsFile);
assert.ok(sourceRecord, 'LinebreakVisitor.ts is absent from the pinned scope');
const sourcePath = path.join(root, '.reference/MathJax-src', tsFile);
const sha = crypto.createHash('sha256').update(fs.readFileSync(sourcePath)).digest('hex');
assert.equal(sha, sourceRecord.source_sha256, 'pinned LinebreakVisitor.ts changed');
const enumFile = 'ts/core/MmlTree/MmlNode.ts';
const enumRecord = scope.files.find(record => record.source === enumFile);
assert.ok(enumRecord, 'MmlNode.ts is absent from the pinned scope');
const enumPath = path.join(root, '.reference/MathJax-src', enumFile);
const enumSource = fs.readFileSync(enumPath, 'utf8');
const enumSha = crypto.createHash('sha256').update(enumSource).digest('hex');
assert.equal(enumSha, enumRecord.source_sha256, 'pinned MmlNode.ts changed');
assert.match(enumSource, new RegExp(`CLOSE:\\s*${expectations.texclassClose}`), 'pinned TEXCLASS.CLOSE value changed');
const lock = JSON.parse(fs.readFileSync(
  path.join(root, 'tools/math-port/common-layout-reference/profile-source-lock.json'), 'utf8'));
const file = 'src/cjs/output/common/LinebreakVisitor.js';
const expectedCjsHash = lock.dependencies.find(record => record.file === file)?.sha256;
assert.ok(expectedCjsHash, 'profile source lock lacks LinebreakVisitor.js');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const cjsPath = path.join(packageRoot, file);
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(cjsPath)).digest('hex'), expectedCjsHash,
  'pinned LinebreakVisitor.js changed');
assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot, 'src/package.json'), 'utf8')).version,
  '4.1.3', 'MathJax package version');

const { LinebreakVisitor } = require(cjsPath);
const factory = { getKinds: () => ['mrow', 'mspace'] };
const visitor = new LinebreakVisitor(factory);
// Build the physical child chain Source walks: an open operator is the first
// child of an explicit row, inside an inferred row at outer index 1. The
// preceding infix operator is therefore found only by crossing the
// inferred row's physical parent edge.
const precedingCore = {
  attributes: { get(name) { return name === 'form' ? 'infix' : 'auto'; } },
};
const precedingNode = {
  isEmbellished: true,
  coreMO() { return precedingCore; },
};
const outerRow = {
  notParent: false,
  childNodes: [precedingNode, null],
  isKind(kind) { return kind === 'mrow'; },
  childIndex(child) { const index = this.childNodes.indexOf(child); return index < 0 ? null : index; },
};
const inferredRow = {
  notParent: true,
  childNodes: [null],
  parent: outerRow,
  isKind() { return false; },
  childIndex(child) { const index = this.childNodes.indexOf(child); return index < 0 ? null : index; },
};
const innerRow = {
  notParent: false,
  childNodes: [null],
  parent: inferredRow,
  isKind(kind) { return kind === 'mrow'; },
  childIndex(child) { const index = this.childNodes.indexOf(child); return index < 0 ? null : index; },
};
const nestedOpenNode = {
  parent: innerRow,
  texClass: 4,
  prevClass: 0,
  Parent: null,
  attributes: { getList() { return { linebreak: 'auto', fence: false, form: 'prefix' }; } },
};
innerRow.childNodes[0] = nestedOpenNode;
inferredRow.childNodes[0] = innerRow;
outerRow.childNodes[1] = inferredRow;
precedingNode.parent = outerRow;
const nestedPrevious = visitor.getPrevious({ node: nestedOpenNode });
assert.equal(nestedPrevious, precedingCore);
visitor.state = { width: 80, w: 0, mathLeft: 80, depth: 0 };
const nestedOpenPenalty = visitor.moPenalty({ node: nestedOpenNode });
assert.equal(nestedOpenPenalty, expectations.nestedOpenPenalty);

const noPreviousOuter = {
  notParent: false,
  childNodes: [null],
  isKind(kind) { return kind === 'mrow'; },
  childIndex(child) { const index = this.childNodes.indexOf(child); return index < 0 ? null : index; },
};
const noPreviousMath = {
  childNodes: [noPreviousOuter],
  isKind(kind) { return kind === 'math'; },
  childIndex(child) { const index = this.childNodes.indexOf(child); return index < 0 ? null : index; },
};
noPreviousOuter.parent = noPreviousMath;
const noPreviousInferred = {
  notParent: true,
  childNodes: [null],
  parent: noPreviousOuter,
  isKind() { return false; },
  childIndex(child) { const index = this.childNodes.indexOf(child); return index < 0 ? null : index; },
};
const noPreviousInner = {
  notParent: false,
  childNodes: [null],
  parent: noPreviousInferred,
  isKind(kind) { return kind === 'mrow'; },
  childIndex(child) { const index = this.childNodes.indexOf(child); return index < 0 ? null : index; },
};
const noPreviousOpenNode = { ...nestedOpenNode, parent: noPreviousInner };
noPreviousInner.childNodes[0] = noPreviousOpenNode;
noPreviousInferred.childNodes[0] = noPreviousInner;
noPreviousOuter.childNodes[0] = noPreviousInferred;
assert.equal(visitor.getPrevious({ node: noPreviousOpenNode }), null);
visitor.state = { width: 80, w: 0, mathLeft: 80, depth: 0 };
const noPreviousOpenPenalty = visitor.moPenalty({ node: noPreviousOpenNode });
assert.equal(noPreviousOpenPenalty, expectations.noPreviousOpenPenalty);
const makeLine = width => ({
  L: 0, w: width, R: 0, rscale: 1,
  getIndentData() { this.indentData = [[0, 0], [0, 0], [0, 0]]; },
});
const makeSpace = (id, width, linebreak, canBreak) => {
  const line = makeLine(width);
  let appliedBreakStyle = null;
  return {
    kind: 'mspace',
    id,
    node: {
      isEmbellished: false,
      kind: 'mspace',
      isKind(kind) { return kind === 'mspace'; },
      attributes: { get(name) { return name === 'linebreak' ? linebreak : null; } },
    },
    canBreak,
    styleData: null,
    getBBox() { return { w: width }; },
    getLineBBox() { return line; },
    processIndent() { return ['left', 0]; },
    coreMO() {
      return {
        node: { attributes: { get() { return null; } } },
        setBreakStyle(style) { appliedBreakStyle = style; },
      };
    },
    invalidateBBox() {},
    get appliedBreakStyle() { return appliedBreakStyle; },
  };
};
const first = makeSpace('first', 4, 'nobreak', false);
const second = makeSpace('break', 6, 'newline', true);
const rootLine = makeLine(10);
rootLine.start = [0, 0];
rootLine.end = [1, 0];
const rootWrapper = {
  kind: 'mrow',
  node: { isEmbellished: false, isToken: false, linebreakContainer: false, kind: 'mrow' },
  childNodes: [first, second],
  lineBBox: [],
  styleData: null,
  breakCount: 0,
  getBBox() { return { w: 10 }; },
  getLineBBox() { return rootLine; },
};
visitor.state = visitor.createState(rootWrapper);
visitor.state.width = 5;
visitor.visitNode(rootWrapper, 0);
const selectedBreaks = [...visitor.state.breaks].map(([wrapper, index]) => [wrapper.id, index]);
assert.deepEqual(selectedBreaks, expectations.selectedBreaks);
assert.equal(visitor.state.w, expectations.postBreakWidth);
const postBreakWidth = visitor.state.w;

// Exercise the public Source operation too: it creates state, scans the real
// line geometry, dispatches the mrow/mspace methods, applies the chosen mark,
// invalidates the target, then restores the caller's state.
visitor.breakToWidth(rootWrapper, 5);
assert.equal(second.appliedBreakStyle, expectations.breakStyle);

const recursiveVisitor = new LinebreakVisitor({
  getKinds: () => ['mrow', 'mspace', 'mfrac', 'msqrt', 'mroot'],
});
const makeRow = (id, spaces, width) => {
  const line = makeLine(width);
  line.start = [0, 0];
  line.end = [spaces.length - 1, 0];
  return {
    id,
    kind: 'mrow',
    node: { isEmbellished: false, isToken: false, linebreakContainer: false, kind: 'mrow' },
    childNodes: spaces,
    breakCount: 0,
    lineBBox: [],
    styleData: null,
    getBBox() { return { w: width }; },
    getLineBBox() { return line; },
  };
};
const numeratorBreak = makeSpace('numerator-break', 6, 'newline', true);
const denominatorBreak = makeSpace('denominator-break', 6, 'newline', true);
const numerator = makeRow('numerator', [makeSpace('numerator-prefix', 4, 'nobreak', false), numeratorBreak], 10);
const denominator = makeRow('denominator', [makeSpace('denominator-prefix', 4, 'nobreak', false), denominatorBreak], 10);
const fractionLine = makeLine(20);
const fraction = {
  kind: 'mfrac',
  node: {
    isEmbellished: false,
    isToken: false,
    linebreakContainer: false,
    kind: 'mfrac',
    attributes: { get(name) { return name === 'bevelled' ? false : null; } },
  },
  childNodes: [numerator, denominator],
  breakCount: 0,
  lineBBox: [],
  styleData: null,
  getBBox() { return { w: 20 }; },
  getOuterBBox() { return { w: 20 }; },
  getLineBBox() { return fractionLine; },
};
recursiveVisitor.breakToWidth(fraction, 5);
assert.equal(numeratorBreak.appliedBreakStyle, expectations.breakStyle);
assert.equal(denominatorBreak.appliedBreakStyle, expectations.breakStyle);

const radicalBreak = makeSpace('radical-break', 6, 'newline', true);
const radicalBase = makeRow('radical-base', [makeSpace('radical-prefix', 4, 'nobreak', false), radicalBreak], 10);
const radicalLine = makeLine(20);
const radical = {
  kind: 'msqrt',
  node: { isEmbellished: false, isToken: false, linebreakContainer: false, kind: 'msqrt' },
  childNodes: [radicalBase],
  base: 0,
  breakCount: 0,
  lineBBox: [],
  styleData: null,
  rootWidth() { return 1; },
  getBBox() { return { w: 20 }; },
  getOuterBBox() { return { w: 20 }; },
  getLineBBox() { return radicalLine; },
  getStretchedSurd() { this.stretched = true; },
};
recursiveVisitor.breakToWidth(radical, 5);
assert.equal(radicalBreak.appliedBreakStyle, expectations.breakStyle);
assert.equal(radical.stretched, true);

const indexedRadicalBreak = makeSpace('indexed-radical-break', 6, 'newline', true);
const indexedRadicalBase = makeRow('indexed-radical-base', [
  makeSpace('indexed-radical-prefix', 4, 'nobreak', false), indexedRadicalBreak,
], 10);
const indexedRadical = {
  kind: 'mroot',
  node: { isEmbellished: false, isToken: false, linebreakContainer: false, kind: 'mroot' },
  childNodes: [indexedRadicalBase, makeSpace('root-index', 1, 'nobreak', false)],
  base: 0,
  breakCount: 0,
  lineBBox: [],
  styleData: null,
  rootWidth() { return 1; },
  getBBox() { return { w: 20 }; },
  getOuterBBox() { return { w: 20 }; },
  getLineBBox() { return makeLine(20); },
  getStretchedSurd() { this.stretched = true; },
};
recursiveVisitor.breakToWidth(indexedRadical, 5);
assert.equal(indexedRadicalBreak.appliedBreakStyle, expectations.breakStyle);
assert.equal(indexedRadical.stretched, true);

visitor.state.width = 80;
visitor.state.w = 20;
visitor.state.mathLeft = 50;
visitor.state.depth = 1;
const mo = {
  node: {
    texClass: 2,
    prevClass: 0,
    Parent: null,
    attributes: { getList() { return { linebreak: 'goodbreak', fence: false, form: 'infix' }; } },
  },
};
const binGoodbreakPenalty = visitor.moPenalty(mo);
assert.equal(binGoodbreakPenalty, expectations.binaryGoodbreakPenalty);
// Source uses an ordinary signed JS number for depth. A closing-class token
// without a matching open token decrements depth below zero; preserve that
// behavior instead of unsigned underflow or clamping.
visitor.state.width = 80;
visitor.state.w = 0;
visitor.state.mathLeft = 80;
visitor.state.depth = 0;
const unmatchedClosePenalty = visitor.moPenalty({
  node: {
    texClass: 5, // pinned TEXCLASS.CLOSE from core/MmlTree/MmlNode.ts
    prevClass: 0,
    Parent: null,
    attributes: { getList() { return { linebreak: 'auto', fence: false, form: 'postfix' }; } },
  },
});
assert.equal(visitor.state.depth, -1);
assert.equal(unmatchedClosePenalty, expectations.unmatchedClose.penalty);
assert.equal(visitor.state.depth, expectations.unmatchedClose.depth);
const mspaceNewlinePenalty = visitor.mspacePenalty(second);
assert.equal(mspaceNewlinePenalty, expectations.mspaceNewlinePenalty);
second.node.attributes.get = name => name === 'linebreak' ? 'nobreak' : null;
assert.equal(visitor.mspacePenalty(second), expectations.mspaceNobreakPenalty);
const sourceEvidence = {
  version: '4.1.3',
  files: { [tsFile]: sha, [enumFile]: enumSha, [file]: expectedCjsHash },
  cases: {
    overfull_line_selected_break: selectedBreaks,
    post_break_width: postBreakWidth,
    break_to_width_applied_style: second.appliedBreakStyle,
    nested_inferred_previous: {
      foundPriorCore: nestedPrevious === precedingCore,
      penalty: nestedOpenPenalty,
      noPreviousPenalty: noPreviousOpenPenalty,
    },
    fraction_recursive_breaks: [numeratorBreak.appliedBreakStyle, denominatorBreak.appliedBreakStyle],
    radical_recursive_break: radicalBreak.appliedBreakStyle,
    radical_stretched: radical.stretched,
    indexed_radical_recursive_break: indexedRadicalBreak.appliedBreakStyle,
    indexed_radical_stretched: indexedRadical.stretched,
    binary_goodbreak_penalty: binGoodbreakPenalty,
    unmatched_close_fence: { penalty: unmatchedClosePenalty, depth: visitor.state.depth },
    mspace_newline_penalty: mspaceNewlinePenalty,
    mspace_nobreak_penalty: visitor.mspacePenalty(second),
  },
};
const output = path.join(root, 'build/math-port/common-source/linebreak-source.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(sourceEvidence, null, 2) + '\n');
process.stdout.write(JSON.stringify({ output: path.relative(root, output),
  selectedBreaks, binGoodbreakPenalty, comparedWith: path.relative(root,
    path.join(root, 'tests/math/fixtures/common-source/linebreak-source-expectations.json')) }) + '\n');
