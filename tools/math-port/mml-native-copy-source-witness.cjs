'use strict';

const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

function repositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2))
  throw new Error('usage: node mml-native-copy-source-witness.cjs [--repo-root <repository>]');
const repo = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) : repositoryRoot();
assert.ok(repo, 'pass --repo-root <repository> or run this script from within the repository');
const sourceRoot = path.join(repo, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const expectedHashes = {
  'cjs/core/MmlTree/MmlNode.js': '86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
  'cjs/core/MmlTree/MmlFactory.js': 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'cjs/core/MmlTree/MmlNodes/mi.js': '47058d7ddb5b24ed530936baffe834a06c0557ec7dbdc03312d3ffa0aca17241',
  'cjs/core/MmlTree/Attributes.js': 'efa567c0c573f13f136f1d37b0af6bdc1a675f70cc13a9ae1cd7334321f30749',
  'cjs/core/Tree/NodeFactory.js': '148c9a3a2e469fc464036b88deb7067cd779989a43356812d31365a84ec75c54',
};
const hashes = {};
for (const [relative, expected] of Object.entries(expectedHashes)) {
  const bytes = fs.readFileSync(path.join(sourceRoot, relative));
  const actual = crypto.createHash('sha256').update(bytes).digest('hex');
  assert.equal(actual, expected, `Pinned Source changed: ${relative}`);
  hashes[relative] = actual;
}
const pkg = require(path.join(sourceRoot, 'package.json'));
assert.equal(pkg.version, '4.1.3');

const { MmlFactory } = require(path.join(sourceRoot, 'cjs/core/MmlTree/MmlFactory.js'));
const { TEXCLASS } = require(path.join(sourceRoot, 'cjs/core/MmlTree/MmlNode.js'));
const factory = new MmlFactory();
const source = factory.create('mi', { id: 'source-id', mathvariant: 'bold', 'data-mark': 'source' });
source.appendChild(factory.create('text').setText('glyph text'));
source.texClass = TEXCLASS.REL;
source.prevClass = TEXCLASS.OP;
source.prevLevel = 2;
source.setProperty('copy-marker', 'source');

const copy = source.copy();
assert.equal(copy.kind, 'mi');
assert.equal(copy.getText(), 'glyph text');
assert.equal(copy.childNodes.length, 1);
assert.equal(copy.childNodes[0].kind, 'text');
assert.equal(copy.childNodes[0].getText(), 'glyph text');
assert.equal(copy.texClass, TEXCLASS.ORD); // MmlMi constructor value; Source copy does not copy source.texClass.
assert.equal(copy.prevClass, null);
assert.equal(copy.prevLevel, null);
assert.equal(copy.attributes.getExplicit('id'), undefined);
assert.equal(copy.attributes.getExplicit('mathvariant'), 'bold');
assert.equal(copy.attributes.getExplicit('data-mark'), 'source');
assert.equal(copy.getProperty('copy-marker'), 'source');
const copyBeforeMutation = {
  kind: copy.kind,
  text: copy.getText(),
  childKinds: copy.childNodes.map((child) => child.kind),
  childTexts: copy.childNodes.map((child) => child.getText()),
  texClass: copy.texClass,
  prevClass: copy.prevClass,
  prevLevel: copy.prevLevel,
  hasId: copy.attributes.hasExplicit('id'),
  mathvariant: copy.attributes.getExplicit('mathvariant'),
  dataMark: copy.attributes.getExplicit('data-mark'),
  copyMarker: copy.getProperty('copy-marker'),
};

copy.attributes.set('data-mark', 'copy-only');
copy.setProperty('copy-marker', 'copy-only');
assert.equal(source.attributes.getExplicit('data-mark'), 'source');
assert.equal(source.getProperty('copy-marker'), 'source');
const sourceAfterCopyMutation = {
  dataMark: source.attributes.getExplicit('data-mark'),
  copyMarker: source.getProperty('copy-marker'),
};

const evidence = {
  package: pkg.name,
  version: pkg.version,
  hashes,
  copyBeforeMutation,
  sourceAfterCopyMutation,
};
const output = path.join(repo, 'build/math-port/typed-mml-copy/source-witness.json');
fs.mkdirSync(path.dirname(output), {recursive: true});
fs.writeFileSync(output, JSON.stringify(evidence, null, 2) + '\n');
process.stdout.write(JSON.stringify({output: path.relative(repo, output), ...evidence}, null, 2) + '\n');
