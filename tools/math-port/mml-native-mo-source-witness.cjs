'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/math-port/mo-pattern-reference.cjs'))) return current;
  }
  return null;
}
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2)) {
  throw new Error('usage: node mml-native-mo-source-witness.cjs [--repo-root <repository>]');
}
const root = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) : findRepositoryRoot();
assert.ok(root, 'pass --repo-root <repository> or run this script from within the repository');
const { root: referenceRoot, M } = require(path.join(root, 'tools/math-port/mo-pattern-reference.cjs'));
assert.equal(referenceRoot, root);
const { MmlFactory } = require(path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlFactory.js'));
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const sourceLock = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/locks/p12b-source-lock.json'), 'utf8'));
assert.equal(sourceLock.version, '4.1.3');
assert.equal(sourceLock.sha256, '2082cd372a4c21fa46c67ab3972b51bcc950f711dd3369cbb2bd7ced62c2f605');
assert.equal(sha(fs.readFileSync(path.join(root, '.reference/MathJax-src', sourceLock.source))), sourceLock.sha256);
assert.equal(require(path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')).version, '4.1.3');

class ProbeMo extends M {
  operatorDefinition(text) { return this.getOperatorDef(text); }
  updateOperatorTable(text) { return this.checkOperatorTable(text); }
  remapPrimeText(text) { return this.checkPrimes(text); }
  updateScripts(text) { return this.checkPseudoScripts(text); }
}
function makeMo(factory, text, attributes = {}) {
  const child = factory.create('text').setText(text);
  return factory.create('mo', attributes, [child]);
}
const factory = new MmlFactory();
factory.setNodeClass('mo', ProbeMo);

const infix = makeMo(factory, '+');
assert.deepEqual(infix.getForms(), ['infix', 'prefix', 'postfix']);
const infixDefinition = infix.operatorDefinition('+');
assert.deepEqual(infixDefinition, [4, 4, 2, null]);

const getterOnly = makeMo(factory, '+');
assert.equal(getterOnly.texClass, 2);
assert.equal(getterOnly.lspace, 5 / 18);
assert.equal(getterOnly.rspace, 5 / 18);
const explicitFormGetter = makeMo(factory, '+', { form: 'prefix' });
assert.deepEqual(explicitFormGetter.getForms(), ['infix', 'prefix', 'postfix']);

infix.updateOperatorTable('+');
assert.equal(infix._texClass, 2);
assert.equal(infix.lspace, 4 / 18);
assert.equal(infix.rspace, 4 / 18);

const prefix = makeMo(factory, '+', { form: 'prefix' });
assert.deepEqual(prefix.getForms(), ['infix', 'prefix', 'postfix']);
assert.deepEqual(prefix.operatorDefinition('+'), [0, 0, 2, null]);

const primes = makeMo(factory, "''");
primes.remapPrimeText("''");
assert.equal(primes.getProperty('primes'), '′′');

const pseudo = makeMo(factory, "'²");
pseudo.updateScripts("'²");
assert.equal(pseudo.getProperty('pseudoscript'), true);
assert.equal(pseudo.attributes.getInherited('lspace'), 0);
assert.equal(pseudo.attributes.getInherited('rspace'), 0);

const superscriptPseudo = makeMo(factory, "'²");
const superscript = factory.create('msup', {}, [factory.create('text').setText('x'), superscriptPseudo]);
superscriptPseudo.updateScripts("'²");
assert.equal(superscript.isKind('msubsup'), true);
assert.equal(superscript.isKind('msub'), false);
assert.equal(superscriptPseudo.getProperty('pseudoscript'), false);
assert.equal(superscriptPseudo.attributes.getInherited('lspace'), 'thickmathspace');

const output = {
  source: { file: sourceLock.source, sha256: sourceLock.sha256, package: '4.1.3' },
  witness: {
    default_forms: infix.getForms(),
    infix_plus: infixDefinition,
    getter_only: { texClass: getterOnly.texClass, lspace: getterOnly.lspace, rspace: getterOnly.rspace },
    check_operator_table: { texClass: infix._texClass, lspace: infix.lspace, rspace: infix.rspace },
    explicit_prefix_plus: prefix.operatorDefinition('+'),
    remapped_primes: primes.getProperty('primes'),
    pseudo_script: { value: pseudo.getProperty('pseudoscript'), lspace: pseudo.attributes.getInherited('lspace'), rspace: pseudo.attributes.getInherited('rspace') }
  },
    assertions: 23
};
assert.equal(output.witness.infix_plus[2], 2);
const outputFile = path.join(root, 'build/math-port/typed-mml-mo/source-witness.json');
fs.mkdirSync(path.dirname(outputFile), { recursive: true });
fs.writeFileSync(outputFile, JSON.stringify(output, null, 2) + '\n');
process.stdout.write(JSON.stringify({ output: path.relative(root, outputFile), ...output }, null, 2) + '\n');



