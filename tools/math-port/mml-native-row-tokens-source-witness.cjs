'use strict';
// Pinned CJS oracle for the row/token-family methods in this packet.
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
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2)) {
  throw new Error('usage: node mml-native-row-tokens-source-witness.cjs [--repo-root <repository>]');
}
const root = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) :
  (repoRoot(__dirname) || repoRoot(process.cwd()));
assert.ok(root, 'pass --repo-root <repository> or run from the repository');
const packages = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const lock = JSON.parse(fs.readFileSync(path.join(root,
  'tools/math-port/common-layout-reference/profile-source-lock.json'), 'utf8'));
const sourceFiles = [
  'src/cjs/core/MmlTree/MmlNode.js',
  ...['mrow', 'mi', 'mn', 'mtext', 'ms', 'mspace', 'mglyph']
    .map(name => `src/cjs/core/MmlTree/MmlNodes/${name}.js`),
];
const expectedHashes = new Map(lock.dependencies.map(row => [row.file, row.sha256]));
const hashes = {};
for (const source of sourceFiles) {
  const expected = expectedHashes.get(source);
  assert.ok(expected, `profile source lock lacks ${source}`);
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(packages, source))).digest('hex');
  assert.equal(actual, expected, `pinned Source changed: ${source}`);
  hashes[source] = actual;
}
assert.equal(JSON.parse(fs.readFileSync(path.join(packages, 'src/package.json'), 'utf8')).version, '4.1.3');

const { MmlMrow, MmlInferredMrow } = require(path.join(packages, 'src/cjs/core/MmlTree/MmlNodes/mrow.js'));
const { MmlMi } = require(path.join(packages, 'src/cjs/core/MmlTree/MmlNodes/mi.js'));
const { MmlMtext } = require(path.join(packages, 'src/cjs/core/MmlTree/MmlNodes/mtext.js'));
const { MmlMspace } = require(path.join(packages, 'src/cjs/core/MmlTree/MmlNodes/mspace.js'));
const { MmlMglyph } = require(path.join(packages, 'src/cjs/core/MmlTree/MmlNodes/mglyph.js'));

const space = { isSpacelike: true, isEmbellished: false };
const core = { isSpacelike: false, isEmbellished: true, coreMO() { return this; } };
const row = Object.create(MmlMrow.prototype);
row.childNodes = [space, core];
assert.equal(row.isSpacelike, false);
assert.equal(row.isEmbellished, true);
assert.equal(row.core(), core);
assert.equal(row.coreMO(), core);
assert.equal(row.nonSpaceLength(), 1);
assert.equal(row.firstNonSpace(), core);
assert.equal(row.lastNonSpace(), core);

const inferred = Object.create(MmlInferredMrow.prototype);
inferred.childNodes = [{ toString: () => 'mi' }, { toString: () => 'mn' }];
assert.equal(inferred.kind, 'inferredMrow');
assert.equal(inferred.isInferred, true);
assert.equal(inferred.notParent, true);
assert.equal(inferred.toString(), '[mi,mn]');

const mtext = Object.create(MmlMtext.prototype);
mtext.getText = () => '\u2003\t';
mtext.attributes = { hasOneOf: () => false };
const mtextWhitespace = mtext.isSpacelike;
mtext.attributes.hasOneOf = names => names.includes('style');
const mtextBlocked = !mtext.isSpacelike;

const mspace = Object.create(MmlMspace.prototype);
let ownAttrs = new Set();
let values = { width: '-0.2em', linebreak: 'newline' };
mspace.attributes = {
  hasExplicit: name => ownAttrs.has(name),
  hasOneOf: names => names.some(name => ownAttrs.has(name)),
  get: name => values[name],
};
const negativeBreak = mspace.canBreak;
values.width = ' 0.2em';
const positiveBreak = mspace.canBreak;
const newline = mspace.hasNewline;
ownAttrs.add('linebreak');
const explicitLinebreakSpacelike = mspace.isSpacelike;

assert.equal(MmlMi.singleCharacter.test('x'), true);
assert.equal(MmlMi.singleCharacter.test('x\u0301'), true);
assert.equal(MmlMi.singleCharacter.test('𝑥'), true);
assert.equal(MmlMi.singleCharacter.test('xy'), false);
assert.equal(MmlMi.singleCharacter.test('x\n'), false);
assert.equal(MmlMi.singleCharacter.test('x\r\n'), false);
assert.equal(MmlMi.operatorName.test('sin2'), true);
assert.equal(MmlMi.operatorName.test('sin_2'), false);
assert.equal(MmlMi.operatorName.test('sin\n'), false);
assert.equal(MmlMi.operatorName.test('sin\r\n'), false);
function inheritedMiFixture(text) {
  const node = Object.create(MmlMi.prototype);
  node.childNodes = [];
  node.attributes = {
    getAllDefaults: () => ({ mathvariant: true }),
    setInherited(name, value) { this[name] = value; },
    getExplicit: () => undefined,
  };
  node.getText = () => text;
  node.setProperty = () => {};
  return node;
}
const inheritedSymbol = inheritedMiFixture('x');
MmlMi.prototype.setInheritedAttributes.call(inheritedSymbol, {});
const inheritedMulti = inheritedMiFixture('xy');
MmlMi.prototype.setInheritedAttributes.call(inheritedMulti, {});
const inheritedFalse = inheritedMiFixture('x');
MmlMi.prototype.setInheritedAttributes.call(inheritedFalse, {
  mathvariant: ['parent', false],
});
const mi = Object.create(MmlMi.prototype);
mi.getPrevClass = previous => { mi.previous = previous; };
mi.getText = () => 'sin';
mi.attributes = { get: name => name === 'mathvariant' ? 'normal' : undefined };
mi.getProperty = () => undefined;
mi.setTexClass = value => { mi.texClass = value; };
mi.setProperty = (name, value) => { mi[name] = value; };
const miReturn = MmlMi.prototype.setTeXclass.call(mi, null);
function autoOPFixture(autoOP) {
  const node = Object.create(MmlMi.prototype);
  node.sourceAutoOP = autoOP;
  node.texClass = 'ORD';
  node.getPrevClass = () => {};
  node.getText = () => 'sin';
  node.attributes = { get: name => name === 'mathvariant' ? 'normal' : undefined };
  node.getProperty = name => name === 'autoOP' ? autoOP : undefined;
  node.setProperty = (name, value) => { node[name] = value; };
  MmlMi.prototype.setTeXclass.call(node, null);
  return node;
}
const falseAutoOP = autoOPFixture(false);
const trueAutoOP = autoOPFixture(true);

const glyph = Object.create(MmlMglyph.prototype);
glyph.attributes = { getList: (...names) => Object.fromEntries(names.map(name => [name, ''])) };
let glyphError;
glyph.mError = (message, options, shortError) => { glyphError = { message, shortError }; return glyph; };
MmlMglyph.prototype.verifyAttributes.call(glyph, {});

const witness = {
  schema: 1,
  source: { version: '4.1.3', files: hashes },
  row: { spacelike: row.isSpacelike, embellished: row.isEmbellished,
    coreIdentity: row.core() === core, coreMOIdentity: row.coreMO() === core,
    nonSpaceLength: row.nonSpaceLength(), first: row.firstNonSpace() === core,
    last: row.lastNonSpace() === core, inferred: inferred.toString() },
  tokenPredicates: { mtextWhitespace, mtextBlocked, negativeBreak, positiveBreak,
    newline, explicitLinebreakSpacelike, mi: { autoOperator: mi.autoOP,
      texClass: mi.texClass, returnsSelf: miReturn === mi,
      inheritedSingle: inheritedSymbol.attributes.mathvariant,
      inheritedMulti: inheritedMulti.attributes.mathvariant,
      inheritedFalse: inheritedFalse.attributes.mathvariant,
      autoOP: { absent: { sourceValue: 'undefined', class: mi.texClass, assigned: mi.autoOP },
        false: { sourceValue: false, class: falseAutoOP.texClass, assigned: falseAutoOP.autoOP },
        true: { sourceValue: true, class: trueAutoOP.texClass, assigned: trueAutoOP.autoOP } } },
    singleCharacter: ['x', 'x\u0301', '𝑥', 'xy'].map(value => MmlMi.singleCharacter.test(value)),
    singleCharacterTerminalLines: ['x\n', 'x\r\n'].map(value => MmlMi.singleCharacter.test(value)),
    operatorName: ['sin2', 'sin_2', 'sin\n', 'sin\r\n'].map(value => MmlMi.operatorName.test(value)),
    mglyphError: glyphError },
};
assert.equal(mtextWhitespace, true);
assert.equal(mtextBlocked, true);
assert.equal(negativeBreak, false);
assert.equal(positiveBreak, true);
assert.equal(newline, true);
assert.equal(explicitLinebreakSpacelike, false);
assert.equal(mi.autoOP, true);
assert.equal(miReturn, mi);
assert.equal(falseAutoOP.texClass, 'ORD');
assert.equal(falseAutoOP.autoOP, undefined);
assert.equal(falseAutoOP.sourceAutoOP, false);
assert.equal(trueAutoOP.texClass, 'ORD');
assert.equal(trueAutoOP.autoOP, undefined);
assert.equal(trueAutoOP.sourceAutoOP, true);
assert.equal(inheritedSymbol.attributes.mathvariant, 'italic');
assert.equal(inheritedMulti.attributes.mathvariant, undefined);
assert.equal(inheritedFalse.attributes.mathvariant, false);
assert.equal(glyphError.shortError, true);

const output = path.join(root, 'build/math-port/typed-mml-row-tokens/source-witness.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(witness, null, 2) + '\n');
console.log(JSON.stringify({ output: path.relative(root, output), sourceVersion: witness.source.version,
  row: witness.row, tokenPredicates: witness.tokenPredicates }));
