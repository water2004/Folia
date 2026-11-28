'use strict';

const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2)) {
  throw new Error('usage: node mml-native-base-source-witness.cjs [--repo-root <repository>]');
}
const root = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) : findRepositoryRoot();
assert.ok(root, 'pass --repo-root <repository> or run this script from within the repository');
const sourceRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const packageInfo = require(path.join(sourceRoot, 'package.json'));
assert.equal(packageInfo.version, '4.1.3');

const pinned = {
  'cjs/core/Tree/Factory.js': '739db2e5760c29d7bb4f1bc7baa08ff8d727f4d117522aff5ede017f812cffef',
  'cjs/core/Tree/Node.js': '94174004b89f19bf3c98e8eec5fabdd0bcbf1e13e0b740963fa69554ad8a12a7',
  'cjs/core/Tree/NodeFactory.js': '148c9a3a2e469fc464036b88deb7067cd779989a43356812d31365a84ec75c54',
  'cjs/core/MmlTree/Attributes.js': 'efa567c0c573f13f136f1d37b0af6bdc1a675f70cc13a9ae1cd7334321f30749',
  'cjs/core/MmlTree/MML.js': '380332b7f0cc13104130affb65532ebca2ac214ccd897b93edda551cea443684',
  'cjs/core/MmlTree/MmlFactory.js': 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'cjs/core/MmlTree/MmlNode.js': '86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
  'cjs/core/MmlTree/MmlNodes/mfrac.js': 'b2e053ce68304c03ea740fb22baa5b01a28d540002faeddc30ad5720a4172770',
  'cjs/core/MmlTree/MmlNodes/merror.js': 'cd9b48970d5d0878d3806e8ff4e4f091d2db38f5021a63aa043acfc50d952d6a',
  'cjs/core/MmlTree/MmlNodes/mi.js': '47058d7ddb5b24ed530936baffe834a06c0557ec7dbdc03312d3ffa0aca17241',
  'cjs/core/MmlTree/MmlNodes/mo.js': '7c68f0570d82f585eb6ca7e8bbca119b96a02666e0d4b268a413bf4035ff0b2c',
  'cjs/core/MmlTree/MmlNodes/mpadded.js': '675aae98959b257582171d58584f220da4543deb8e0e3d3b8b07a48ab6fc23a2',
  'cjs/core/MmlTree/MmlNodes/mrow.js': '5846d2b0334223b759a4059dfb0ac5e31aef1f5a96c041da45b3de94c0689896',
  'cjs/core/MmlTree/MmlNodes/mtd.js': '0210a0d98d9098fcdab7362302841a77625341bf1459582e72f57f39314d9699',
};
const sources = {};
for (const [relative, expectedHash] of Object.entries(pinned)) {
  const absolute = path.join(sourceRoot, relative);
  const actualHash = crypto.createHash('sha256').update(fs.readFileSync(absolute)).digest('hex');
  assert.equal(actualHash, expectedHash, `Pinned source hash mismatch: ${relative}`);
  sources[relative] = actualHash;
}

const { MmlFactory } = require(path.join(sourceRoot, 'cjs/core/MmlTree/MmlFactory.js'));
const { TEXCLASS } = require(path.join(sourceRoot, 'cjs/core/MmlTree/MmlNode.js'));
const factory = new MmlFactory();
const cases = [];
function witness(name, actual, expected) {
  assert.deepStrictEqual(actual, expected, name);
  cases.push({ name, actual });
}

// Origin-aware exclusions, ordinary inheritance, stop-before-descendants,
// always-inherited keys, and the TeX prime-style property.
const padded = factory.create('mpadded');
padded.setInheritedAttributes({
  width: ['mstyle', '5em'], mathsize: ['math', '120%'], scriptminsize: ['math', '8pt'],
});
witness('mstyle width excluded from mpadded while normal/always inheritance survives', {
  widthInheritedOwn: Object.hasOwn(padded.attributes.getAllInherited(), 'width'),
  widthDefault: padded.attributes.getDefault('width'),
  mathsize: padded.attributes.getInherited('mathsize'),
  scriptminsize: padded.attributes.getInherited('scriptminsize'),
}, { widthInheritedOwn: false, widthDefault: '', mathsize: '120%', scriptminsize: '8pt' });
padded.setInheritedAttributes({}, false, 0, true);
witness('prime style set through typed-source property owner', padded.getProperty('texprimestyle'), true);

const cell = factory.create('mtd');
const nested = factory.create('mtd');
cell.appendChild(nested);
cell.setInheritedAttributes({ columnalign: ['mtable', 'right'] });
witness('mtd receives columnalign but consumes it before descendants', {
  cell: cell.attributes.getInherited('columnalign'),
  inferredOwn: Object.hasOwn(cell.childNodes[0].attributes.getAllInherited(), 'columnalign'),
  nestedOwn: Object.hasOwn(nested.attributes.getAllInherited(), 'columnalign'),
}, { cell: 'right', inferredOwn: false, nestedOwn: false });

// The matrix entry ORD -> REL is thick space; positive script level suppresses
// nonnegative spacing but leaves the negative ORD -> BIN thin-space entry.
const previous = factory.create('mi');
const current = factory.create('mo');
previous.texClass = TEXCLASS.ORD;
current.texClass = TEXCLASS.REL;
current.setTeXclass(previous);
witness('TeX class matrix', { previous: current.prevClass, spacing: current.texSpacing() },
  { previous: TEXCLASS.ORD, spacing: 'thickmathspace' });
current.prevLevel = 1;
witness('script-level spacing suppression', current.texSpacing(), '');
current.texClass = TEXCLASS.OP;
witness('negative spacing remains in script level', current.texSpacing(), 'thinmathspace');

const unicodeScriptLevel = factory.create('mi', {
  scriptlevel: '\u00a0 0x10 \u3000',
});
const numericSpacing = factory.create('mo');
unicodeScriptLevel.texClass = TEXCLASS.ORD;
numericSpacing.texClass = TEXCLASS.REL;
numericSpacing.getPrevClass(unicodeScriptLevel);
witness('script-level comparison numerically coerces Unicode-whitespace hexadecimal text', {
  storedPrevLevel: numericSpacing.prevLevel,
  spacing: numericSpacing.texSpacing(),
}, { storedPrevLevel: '\u00a0 0x10 \u3000', spacing: '' });

const booleanScriptLevel = factory.create('mi', { scriptlevel: true });
const booleanSpacing = factory.create('mo');
booleanScriptLevel.texClass = TEXCLASS.ORD;
booleanSpacing.texClass = TEXCLASS.REL;
booleanSpacing.getPrevClass(booleanScriptLevel);
witness('boolean script level suppresses positive TeX spacing', {
  storedPrevLevel: booleanSpacing.prevLevel,
  spacing: booleanSpacing.texSpacing(),
}, { storedPrevLevel: true, spacing: '' });

// Source omitted verification is explicitly a no-op. An explicit arity check
// on the same malformed fraction replaces it with a short error tree.
const parent = factory.create('mrow');
const fraction = factory.create('mfrac');
parent.appendChild(fraction);
fraction.verifyTree();
witness('omitted verification options do nothing', {
  childKind: parent.childNodes[0].kind,
  childCount: fraction.childNodes.length,
}, { childKind: 'mfrac', childCount: 0 });
fraction.verifyTree({
  checkArity: true, checkAttributes: false, checkMathvariants: false,
  fullErrors: false, fixMmultiscripts: true, fixMtables: true,
});
witness('explicit arity verification creates short merror', {
  childKind: parent.childNodes[0].kind,
  errorMessage: parent.childNodes[0].attributes.getExplicit('data-mjx-message'),
  title: parent.childNodes[0].attributes.getExplicit('title'),
  text: parent.childNodes[0].childNodes[0].childNodes[0].getText(),
}, {
  childKind: 'merror', errorMessage: 'Wrong number of children for "mfrac" node',
  title: 'Wrong number of children for "mfrac" node', text: 'mfrac',
});

// mError exercises normal nesting, repeated-error suppression, short and full
// branches; these cases assert parent links and exact message/title/text.
function errorCase(short, full) {
  const p = factory.create('mrow');
  const n = factory.create('mi');
  p.appendChild(n);
  const result = n.mError('witness error', { fullErrors: full }, short);
  const errorNode = p.childNodes[0];
  return {
    returnedKind: result && result.kind,
    parentKind: errorNode.kind,
    message: errorNode.attributes.getExplicit('data-mjx-message'),
    title: errorNode.attributes.getExplicit('title'),
    childKinds: errorNode.childNodes.map(child => child.kind),
    inferredChildren: errorNode.childNodes[0].childNodes.map(child => child.kind),
    text: errorNode.childNodes[0].childNodes[0].kind === 'mtext' ? errorNode.childNodes[0].childNodes[0].childNodes[0].getText() : undefined,
    originalParentKind: n.parent && n.parent.kind,
  };
}
witness('ordinary mError branch', errorCase(false, false), {
  returnedKind: 'merror', parentKind: 'merror', message: 'witness error',
  title: undefined, childKinds: ['inferredMrow'], inferredChildren: ['mi'], text: undefined, originalParentKind: 'inferredMrow',
});
witness('short mError branch', errorCase(true, false), {
  returnedKind: 'merror', parentKind: 'merror', message: 'witness error',
  title: 'witness error', childKinds: ['inferredMrow'], inferredChildren: ['mtext'], text: 'mi', originalParentKind: null,
});
witness('full mError branch', errorCase(false, true), {
  returnedKind: 'merror', parentKind: 'merror', message: 'witness error',
  title: undefined, childKinds: ['inferredMrow'], inferredChildren: ['mtext'], text: 'witness error', originalParentKind: null,
});

const identified = factory.create('mi', { id: 'x', mathcolor: 'purple' });
const copied = identified.copy();
const copiedWithId = identified.copy(true);
witness('Source copy id behavior and attributes', {
  copiedId: copied.attributes.getExplicit('id'),
  copiedColor: copied.attributes.getExplicit('mathcolor'),
  keptId: copiedWithId.attributes.getExplicit('id'),
}, { copiedId: undefined, copiedColor: 'purple', keptId: 'x' });

const variantParent = factory.create('mrow');
const emptyVariant = factory.create('mi', { mathvariant: '' });
variantParent.appendChild(emptyVariant);
emptyVariant.verifyTree({
  checkArity: true, checkAttributes: false, checkMathvariants: true,
  fullErrors: false, fixMmultiscripts: true, fixMtables: true,
});
witness('empty mathvariant remains falsy and is not rejected', {
  childKind: variantParent.childNodes[0].kind,
  explicitVariant: emptyVariant.attributes.getExplicit('mathvariant'),
}, { childKind: 'mi', explicitVariant: '' });
const invalidVariant = factory.create('mi', { mathvariant: 5 });
variantParent.appendChild(invalidVariant);
invalidVariant.verifyTree({
  checkArity: true, checkAttributes: false, checkMathvariants: true,
  fullErrors: false, fixMmultiscripts: true, fixMtables: true,
});
witness('truthy nonstring variant error preserves its value in the message', {
  childKind: variantParent.childNodes[1].kind,
  message: variantParent.childNodes[1].attributes.getExplicit('data-mjx-message'),
}, { childKind: 'merror', message: 'Invalid mathvariant: 5' });

const arbitrary = factory.create('mrow');
const sourceInferred = factory.create('inferredMrow', {}, [factory.create('mi'), factory.create('mi')]);
const returnedInferred = arbitrary.appendChild(sourceInferred);
witness('infinite arity appends inferred-row children and returns the source row', {
  returnedSame: returnedInferred === sourceInferred,
  kinds: arbitrary.childNodes.map(child => child.kind),
  parentKinds: arbitrary.childNodes.map(child => child.parent.kind),
}, { returnedSame: true, kinds: ['mi', 'mi'], parentKinds: ['mrow', 'mrow'] });
const errorWithChild = factory.create('merror', {}, [factory.create('mi')]);
witness('negative arity constructor initializes one inferred row', {
  childKinds: errorWithChild.childNodes.map(child => child.kind),
  inferred: errorWithChild.childNodes[0].isInferred,
  nestedKind: errorWithChild.childNodes[0].childNodes[0].kind,
}, { childKinds: ['inferredMrow'], inferred: true, nestedKind: 'mi' });
const finite = factory.create('mfrac');
finite.setInheritedAttributes();
witness('finite arity repair fills missing children with mrows', {
  count: finite.childNodes.length,
  kinds: finite.childNodes.map(child => child.kind),
}, { count: 2, kinds: ['mrow', 'mrow'] });

const result = {
  package: '@mathjax/src', version: packageInfo.version, sources, cases,
};
const output = path.join(root, 'build/math-port/typed-mml-base/source-witness.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(result, null, 2) + '\n');
process.stdout.write(JSON.stringify({ output: path.relative(root, output), ...result }, null, 2) + '\n');








