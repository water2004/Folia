// Executable Source witness for the immutable Common wrapper defaults.
// Run from the repository root with: node tools/math-port/drafts/native-raii/common-factory-native-impl/common_factory_source_witness.cjs
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');

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
const commonFile = path.join(root, '.reference/MathJax-src/ts/output/common/Wrapper.ts');
const factoryFile = path.join(root, '.reference/MathJax-src/ts/output/common/WrapperFactory.ts');
const treeFactoryFile = path.join(root, '.reference/MathJax-src/ts/core/Tree/WrapperFactory.ts');
const svgFile = path.join(root, '.reference/MathJax-src/ts/output/svg/Wrappers.ts');
const chtmlFile = path.join(root, '.reference/MathJax-src/ts/output/chtml/Wrappers.ts');
const sourceHashes = {
  'output/common/Wrapper.ts': 'd1db6112db1cb3546658eb8868b5997a4f69e588632ecbe4e08a71949544a828',
  'output/common/WrapperFactory.ts': 'a798803bab094ae1e72cd7f96eb37aca936ac458da145c1ebb22079a0280b6cd',
  'core/Tree/WrapperFactory.ts': '8c59ed6a5e646e0736730070ba87161d7060f1f4a083e92afcc8012345537cea',
  'output/svg/Wrappers.ts': '8a4fd2c7a4f92b1fac22c4e5f79072b8bf24d9942933478c0bdac1ab48fcae23',
  'output/chtml/Wrappers.ts': '4922639dad6f6b8780060b4302ed70f8e87b1fb5c515bcd46a702d83feba424e',
};
const hash = file => require('node:crypto').createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const [relative, expected] of Object.entries(sourceHashes))
  assert.equal(hash(path.join(root, '.reference/MathJax-src/ts', relative)), expected, `pinned Source changed: ${relative}`);
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
for (const name of ['src', 'mathjax-newcm-font'])
  assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot, name, 'package.json'))).version, '4.1.3');
const common = fs.readFileSync(commonFile, 'utf8');
const factory = fs.readFileSync(factoryFile, 'utf8');
const treeFactory = fs.readFileSync(treeFactoryFile, 'utf8');

function literalAfter(source, declaration, opener, closer) {
  const start = source.indexOf(declaration);
  assert.notEqual(start, -1, `Source declaration is missing: ${declaration}`);
  const equals = source.indexOf('=', start + declaration.length);
  assert.notEqual(equals, -1, `Source initializer is missing: ${declaration}`);
  const open = source.indexOf(opener, equals + 1);
  assert.notEqual(open, -1, `Source literal is missing: ${declaration}`);
  let depth = 0;
  let quote = null;
  let escaped = false;
  for (let i = open; i < source.length; ++i) {
    const ch = source[i];
    if (quote) {
      if (escaped) escaped = false;
      else if (ch === '\\') escaped = true;
      else if (ch === quote) quote = null;
      continue;
    }
    if (ch === "'" || ch === '"' || ch === '`') { quote = ch; continue; }
    if (ch === opener) ++depth;
    if (ch === closer && --depth === 0) return source.slice(open, i + 1);
  }
  assert.fail(`Unclosed Source literal: ${declaration}`);
}

function evaluateLiteral(text) {
  return vm.runInNewContext(`(${text})`, Object.create(null), { timeout: 1000 });
}

const removeStyles = evaluateLiteral(literalAfter(
  common, 'public static removeStyles:', '[', ']'));
const skipAttributes = evaluateLiteral(literalAfter(
  common, 'public static skipAttributes:', '{', '}'));
const boldVariants = evaluateLiteral(literalAfter(
  common, 'public static BOLDVARIANTS:', '{', '}'));
const italicVariants = evaluateLiteral(literalAfter(
  common, 'public static ITALICVARIANTS:', '{', '}'));
const defaultNodesText = factory.match(/public static defaultNodes\s*:[\s\S]*?=\s*(\{[\s\S]*?\});/);
assert.ok(defaultNodesText, 'Source CommonWrapperFactory.defaultNodes initializer is present');
const commonFactoryDefaults = evaluateLiteral(defaultNodesText[1]);

assert.deepEqual(Array.from(removeStyles), [
  'fontSize', 'fontFamily', 'fontWeight', 'fontStyle', 'fontVariant', 'font',
]);
assert.deepEqual(Object.assign({}, skipAttributes), {
  fontfamily: true, fontsize: true, fontweight: true, fontstyle: true,
  color: true, background: true, class: true, href: true, style: true, xmlns: true,
});
assert.equal(boldVariants.bold['sans-serif-italic'], 'sans-serif-bold-italic');
assert.equal(boldVariants.normal['bold-script'], 'script');
assert.equal(italicVariants.italic['bold-sans-serif'], 'sans-serif-bold-italic');
assert.equal(italicVariants.normal['sans-serif-bold-italic'], 'bold-sans-serif');
assert.deepEqual(Object.assign({}, commonFactoryDefaults), {});

const initializeStart = common.indexOf('constructor(factory: WF, node: MmlNode, parent: WW = null)');
const wrapStart = common.indexOf('public wrap<TT = WW>', initializeStart);
assert.ok(initializeStart >= 0 && wrapStart > initializeStart,
          'CommonWrapper constructor and wrap method are present');
const initializeBody = common.slice(initializeStart, wrapStart);
const initializationSteps = [
  'this.getStyles();', 'this.getStyleData();', 'this.getVariant();',
  'this.getScale();', 'this.getSpace();', 'this.childNodes = node.childNodes.map',
];
let lastInitializationPosition = -1;
for (const step of initializationSteps) {
  const position = initializeBody.indexOf(step);
  assert.ok(position > lastInitializationPosition, `Source initialization order includes ${step}`);
  lastInitializationPosition = position;
}
const wrapBody = common.slice(wrapStart, common.indexOf('/********', wrapStart));
const wrapSteps = ['this.factory.wrap(node, parent || this)', 'if (parent)',
                   'parent.childNodes.push(wrapped)', 'this.jax.nodeMap.set(node, wrapped)'];
let lastWrapPosition = -1;
for (const step of wrapSteps) {
  const position = wrapBody.indexOf(step);
  assert.ok(position > lastWrapPosition, `Source wrapper side-effect order includes ${step}`);
  lastWrapPosition = position;
}
assert.match(treeFactory, /return this\.create\(node\.kind, node, \.\.\.args\)/,
             'Tree factory dispatches from the source node kind');

function profileKinds(file, prefix) {
  const source = fs.readFileSync(file, 'utf8');
  const block = source.match(new RegExp(`export const ${prefix}Wrappers[\\s\\S]*?= \\{([\\s\\S]*?)\\n\\};`));
  assert.ok(block, `${prefix} profile wrapper table exists`);
  return Array.from(block[1].matchAll(/^\s*\[?(\w+)\.(?:kind|kind)\]?:\s*\w+/gm), m => m[1]);
}
const svgNames = profileKinds(svgFile, 'Svg');
const chtmlNames = profileKinds(chtmlFile, 'Chtml');
assert.equal(svgNames.length, 38);
assert.equal(chtmlNames.length, 36);
assert.ok(svgNames.includes('SvgMerror'));
assert.ok(svgNames.includes('SvgWrapper'));
assert.ok(chtmlNames.includes('ChtmlMenclose'));
assert.ok(chtmlNames.includes('ChtmlWrapper'));

const evidence = {source: {commit: 'fb987178c2d279a99b0db5ea02e751691703955', packageVersion: '4.1.3', hashes: sourceHashes},
  defaults: {removeStyles, skipAttributes, boldVariants, italicVariants, commonFactoryDefaults},
  order: {initializationSteps, wrapSteps}, profiles: {svg: svgNames, chtml: chtmlNames}};
const output = path.join(root, 'build/math-port/common-source/factory.json');
fs.mkdirSync(path.dirname(output), {recursive: true});
fs.writeFileSync(output, JSON.stringify(evidence, null, 2) + '\n');
console.log(`PASS: Source defaults, wrapper ordering, and empty generic registry; SVG ${svgNames.length} and CHTML ${chtmlNames.length}; wrote ${path.relative(root, output)}`);
