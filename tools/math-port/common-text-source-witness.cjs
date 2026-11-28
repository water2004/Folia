'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
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
const fixtureRoot = process.env.FOLIA_COMMON_SOURCE_FIXTURE_ROOT || path.join(root, 'tests/math/fixtures/common-source');
const lock = JSON.parse(fs.readFileSync(path.join(fixtureRoot, 'text-source-lock.json'), 'utf8'));
const scopeFile = path.join(root, 'tools/math-port/profiles/scope.json');
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
assert.equal(digest(scopeFile), lock.scopeSha256, 'scope.json lock changed');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const srcRoot = path.join(packageRoot, 'src/cjs');
for (const [file, expected] of Object.entries(lock.source))
  assert.equal(digest(path.join(root, '.reference/MathJax-src/ts', file)), expected, `Source ${file}`);
for (const [file, expected] of Object.entries(lock.cjs))
  assert.equal(digest(path.join(srcRoot, file)), expected, `CJS ${file}`);
for (const [name, version] of Object.entries(lock.packages)) { const packageName = name === 'newcm' ? 'mathjax-newcm-font' : name; assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot, packageName, 'package.json'))).version, version); }
const load = file => require(path.join(srcRoot, `${file}.js`));
const {mathjax} = load('mathjax');
const {MathML} = load('input/mathml');
const {SVG} = load('output/svg');
const {liteAdaptor} = load('adaptors/liteAdaptor');
const {RegisterHTMLHandler} = load('handlers/html');
const {MathJaxNewcmFont} = require(path.join(packageRoot, 'mathjax-newcm-font/cjs/svg.js'));
const adaptor = liteAdaptor({fontSize: 16});
RegisterHTMLHandler(adaptor);
const input = new MathML();
const output = new SVG({fontData: MathJaxNewcmFont, fontCache: 'none', exFactor: .5,
  mtextFont: 'Arial', displayOverflow: 'linebreak', linebreaks: {inline: true, width: '4em'}});
const document = mathjax.document('', {InputJax: input, OutputJax: output});
const fields = ['w','h','d','scale','rscale','ic','oc','sk','dx'];
const plain = value => value === undefined ? {'$':'undefined'} :
  Object.is(value, -0) ? {'$':'-0'} : typeof value === 'number' && !Number.isFinite(value) ? {'$':String(value)} : value;
let active;
const original = output.processMath;
output.processMath = function(wrapper, node) {
  const result = original.apply(this, arguments);
  if (active) active.wrappers = Array.from(this.nodeMap.values(), item => ({
    kind: item.node.kind, text: typeof item.node.getText === 'function' ? item.node.getText() : null,
    variant: plain(item.variant), bbox: Object.fromEntries(fields.map(key => [key, plain(item.getBBox()[key])])),
    ...(item.node.kind === 'mtext' ? {breakPoints: item.breakPoints,
      breakWidths: Array.from({length: item.breakCount}, (_, i) => plain(item.getBreakWidth(i))),
      lineBoxes: Array.from({length: item.breakCount + 1}, (_, i) => { const box = item.getLineBBox(i); return {w: plain(box.w), h: plain(box.h), d: plain(box.d), L: plain(box.L), R: plain(box.R)}; })} : {}),
    ...(item.node.kind === 'math' ? {wrapWidth: plain(item.getWrapWidth(0))} : {})
  }));
  return result;
};
const cases = [
  ['unicode-mi', '<math><mi>𝑨</mi></math>'],
  ['mn-leading-minus', '<math><mn>-2</mn></math>'],
  ['scaled-mi', '<math><mstyle mathsize="150%"><mi>x</mi></mstyle></math>'],
  ['ms-quotes', '<math><ms>hello</ms></math>'],
  ['mspace-metrics', '<math><mrow><mi>x</mi><mspace width="1em" height=".5em" depth=".2em"/></mrow></math>'],
  ['mglyph-index', '<math><mglyph index="65"/></math>'],
  ['explicit-font-mi', '<math><mi fontfamily="Arial">x</mi></math>'],
  ['mtext-widths', '<math><mtext>alpha beta gamma</mtext></math>'],
  ['display-mtext-linebreak', '<math display="block" overflow="linebreak" maxwidth="4em"><mrow><mtext>alpha beta gamma delta epsilon</mtext><mo>+</mo><mi>x</mi></mrow></math>']
];
(async () => {
  const records = [];
  for (const [name, mathml] of cases) {
    active = {wrappers: []};
    let error = null;
    try { await document.convertPromise(mathml, {em: 16, ex: 8, containerWidth: 64}); }
    catch (caught) { error = {name: caught.name, message: String(caught.message)}; }
    records.push({name, mathml, wrappers: active.wrappers, error});
  }
  const witness = {schema: 1, source: {package: '@mathjax/src 4.1.3', scopeSha256: lock.scopeSha256,
    ts: lock.source, cjs: lock.cjs}, records};
  const expectedPath = path.join(fixtureRoot, 'text-source-witness.expected.json');
  assert.deepEqual(witness, JSON.parse(fs.readFileSync(expectedPath, 'utf8')), 'pinned Source observations changed');
  const outputPath = path.join(root, 'build/math-port/common-source/text.json');
  fs.mkdirSync(path.dirname(outputPath), {recursive: true});
  fs.writeFileSync(outputPath, JSON.stringify(witness, null, 2) + '\n');
  process.stdout.write(`Source witness ${records.length} cases; hashes pinned; evidence ${path.relative(root, outputPath)}\n`);
})().catch(error => { process.stderr.write(error.stack + '\n'); process.exitCode = 1; });
