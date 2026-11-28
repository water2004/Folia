'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const source = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const {MmlFactory} = require(path.join(source, 'cjs/core/MmlTree/MmlFactory.js'));
const {MmlMi} = require(path.join(source, 'cjs/core/MmlTree/MmlNodes/mi.js'));
const {TextNode} = require(path.join(source, 'cjs/core/MmlTree/MmlNode.js'));
class FirstMi extends MmlMi {}
class SecondMi extends MmlMi {}
const hashes = {};
for (const file of [
  'ts/core/Tree/Factory.ts', 'ts/core/Tree/NodeFactory.ts',
  'ts/core/MmlTree/MmlFactory.ts', 'ts/core/MmlTree/MmlNode.ts',
  'ts/core/MmlTree/MmlNodes/mi.ts',
  'cjs/core/Tree/Factory.js', 'cjs/core/Tree/NodeFactory.js',
  'cjs/core/MmlTree/MmlFactory.js', 'cjs/core/MmlTree/MmlNode.js',
  'cjs/core/MmlTree/MmlNodes/mi.js'
]) {
  const installed = fs.readFileSync(path.join(source, file));
  const bytes = file.startsWith('ts/')
    ? fs.readFileSync(path.join(root, '.reference/MathJax-src', file)) : installed;
  if (file.startsWith('ts/')) assert.equal(
    installed.toString('utf8').replaceAll('\r\n', '\n'),
    bytes.toString('utf8').replaceAll('\r\n', '\n'), file);
  hashes[file] = crypto.createHash('sha256').update(bytes).digest('hex');
}
assert.equal(hashes['ts/core/MmlTree/MmlFactory.ts'], '5e7338cd3eed5a4002243b7371dc5098c6433ff4ba4345cb2d50cf0385b2abdb');
assert.equal(hashes['ts/core/MmlTree/MmlNodes/mi.ts'], '3bbddaeac72d2c0f2e5d8a0440c28188f98d1bf1481a0ec6872e103a50f146c2');
const cases = {};
{
  const factory = new MmlFactory();
  factory.setNodeClass('mi', FirstMi);
  const first = factory.create('mi');
  factory.setNodeClass('mi', SecondMi);
  const second = factory.create('mi');
  factory.deleteNodeClass('mi');
  cases.registryReplacementAndDeletion = {
    firstIsOriginal: first.constructor === FirstMi,
    secondIsReplacement: second.constructor === SecondMi,
    distinctClasses: first.constructor !== second.constructor,
    absentRegistry: factory.getNodeClass('mi') === undefined
  };
}
{
  let observed;
  class ObservedMi extends MmlMi {
    constructor(...args) { super(...args); observed = this.constructor === ObservedMi; }
  }
  const factory = new MmlFactory();
  factory.setNodeClass('mi', ObservedMi);
  const node = factory.create('mi');
  cases.initialization = { observed, identityAfterInit: node.constructor === ObservedMi };
}
{
  const factory = new MmlFactory();
  factory.setNodeClass('mi', FirstMi);
  const savedCreator = factory.MML.mi;
  factory.setNodeClass('mi', SecondMi);
  const node = savedCreator({}, []);
  cases.savedCreator = { originalClass: node.constructor === FirstMi, sameFactory: node.factory === factory };
  factory.deleteNodeClass('mi');
  let failure;
  try { savedCreator({}, []); } catch (error) { failure = { name: error.name, message: error.message }; }
  assert.deepEqual(failure, { name: 'TypeError', message: "Cannot read properties of undefined (reading 'defaults')" });
  cases.savedCreator.defaultsReadLiveRegistry = true;
}
{
  const factory = new MmlFactory();
  const text = factory.create('text');
  cases.emptyText = { originalClass: text.constructor === TextNode, text: text.getText() };
}
for (const value of Object.values(cases)) {
  for (const [key, observation] of Object.entries(value)) {
    if (key !== 'text') assert.equal(observation, true, key);
    else assert.equal(observation, '');
  }
}
const result = { schema: 'mml-constructor-identity-v1', hashes, cases };
const evidence = path.join(root, 'tools/math-port/profiles/evidence/mml-constructor-identity-source.json');
if (process.argv.includes('--check')) {
  assert.deepEqual(result, JSON.parse(fs.readFileSync(evidence, 'utf8')));
  console.log('Pinned MML constructor identity: 4 original observations agree');
} else {
  fs.writeFileSync(evidence, JSON.stringify(result, null, 2) + '\n');
}
