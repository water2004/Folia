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
function repositoryRoot() {
  const arg = process.argv.indexOf('--repo-root');
  if (arg >= 0 && (!process.argv[arg + 1] || process.argv.length !== arg + 2))
    throw new Error('usage: node <witness> [--repo-root <repository>]');
  const root = arg >= 0 ? path.resolve(process.argv[arg + 1]) : findRepositoryRoot();
  assert.ok(root, 'pass --repo-root <repository> or run the witness from within the repository');
  return root;
}
const root = repositoryRoot();
const bundle = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const src = path.join(root, '.reference/MathJax-src');
const fixtureRoot = process.env.FOLIA_COMMON_SOURCE_FIXTURE_ROOT || path.join(root, 'tests/math/fixtures/common-source');
const lock = JSON.parse(fs.readFileSync(path.join(fixtureRoot, 'variant-source-lock.json'), 'utf8'));
const sha256 = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
assert.equal(sha256(scopePath), lock.scopeSha256, 'scope lock changed');
for (const [file, expected] of Object.entries(lock.source)) {
  assert.equal(sha256(path.join(src, 'ts', file)), expected, `Source TS changed: ${file}`);
}
for (const [file, expected] of Object.entries(lock.cjs)) {
  assert.equal(sha256(path.join(bundle, 'src/cjs', file)), expected, `pinned CJS changed: ${file}`);
}
assert.equal(JSON.parse(fs.readFileSync(path.join(bundle, 'src/package.json'))).version,
  '4.1.3', 'MathJax source package version');
assert.equal(JSON.parse(fs.readFileSync(path.join(bundle, 'mathjax-newcm-font/package.json'))).version,
  '4.1.3', 'MathJax NewCM font package version');

const { CommonWrapper } = require(path.join(bundle, 'src/cjs/output/common/Wrapper.js'));
const { MathJaxNewcmFont } = require(path.join(bundle, 'mathjax-newcm-font/cjs/svg.js'));
const font = new MathJaxNewcmFont();

function runExplicitVariant(value) {
  const warnings = [];
  const originalWarn = console.warn;
  console.warn = (...args) => warnings.push(args.map(String).join(' '));
  const receiver = {
    font,
    node: {
      isToken: true,
      attributes: {
        get(name) { assert.equal(name, 'mathvariant'); return value; },
        hasExplicit(name) { return name === 'mathvariant'; },
      },
    },
  };
  try {
    CommonWrapper.prototype.getVariant.call(receiver);
  } finally {
    console.warn = originalWarn;
  }
  return { variant: receiver.variant, warnings };
}

function runFontFamilyPrecedence() {
  const receiver = {
    node: {
      isToken: true,
      attributes: {
        get(name) { assert.equal(name, 'mathvariant'); return undefined; },
        hasExplicit(name) { assert.equal(name, 'mathvariant'); return false; },
        getList() { return { fontfamily: 'MathAttribute' }; },
      },
      getProperty() { return false; },
    },
    removedStyles: { fontFamily: 'InlineMath' },
    explicitVariant(family) {
      this.explicitFont = { family };
      return '-explicitFont';
    },
  };
  CommonWrapper.prototype.getVariant.call(receiver);
  return { variant: receiver.variant, explicitFont: receiver.explicitFont };
}

const actual = {
  invalid: runExplicitVariant('not-a-source-variant'),
  explicitNull: runExplicitVariant(null),
  valid: runExplicitVariant('normal'),
  fontFamilyPrecedence: runFontFamilyPrecedence(),
};
const expected = JSON.parse(fs.readFileSync(path.join(fixtureRoot, 'variant-source-witness.expected.json'), 'utf8'));
assert.deepEqual(actual, expected);
const output = path.join(root, 'build/math-port/common-source/variant.json');
fs.mkdirSync(path.dirname(output), {recursive: true});
fs.writeFileSync(output, JSON.stringify(actual, null, 2) + '\n');
process.stdout.write(`${JSON.stringify(actual, null, 2)}\n`);
