'use strict';

// Locked MathJax 4.1.3 Bussproofs commands, maps and MML/error observations.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const cjsRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const lockPath = path.join(root, 'tools/math-port/locks/bussproofs-source-lock.json');
const lock = JSON.parse(fs.readFileSync(lockPath, 'utf8'));
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const file of lock.files) {
  assert.equal(hash(path.join(sourceRoot, file.ts)), file.tsSha256, `Source ${file.ts}`);
  assert.equal(hash(path.join(cjsRoot, file.cjs)), file.cjsSha256, `Source ${file.cjs}`);
}
const load = name => require(path.join(cjsRoot, 'cjs', name));
const {mathjax} = load('mathjax.js');
const {TeX} = load('input/tex.js');
const {MmlFactory} = load('core/MmlTree/MmlFactory.js');
const {SVG} = load('output/svg.js');
const {liteAdaptor} = load('adaptors/liteAdaptor.js');
const {RegisterHTMLHandler} = load('handlers/html.js');
const {MathJaxNewcmFont} = require(path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/mathjax-newcm-font/cjs/svg.js'));
const {MathJaxBboldxFontExtension} = require(path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/mathjax-bboldx-font-extension/cjs/svg.js'));
const {MathJaxBbmFontExtension} = require(path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/mathjax-bbm-font-extension/cjs/svg.js'));
const {MathJaxDsfontFontExtension} = require(path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/mathjax-dsfont-font-extension/cjs/svg.js'));
const {AbstractMathItem} = load('core/MathItem.js');
const {MapHandler} = load('input/tex/MapHandler.js');
const {BussproofsConfiguration} = load('input/tex/bussproofs/BussproofsConfiguration.js');
const {ProofTreeItem} = load('input/tex/bussproofs/BussproofsItems.js');
const adaptor = liteAdaptor({fontSize: 16});
RegisterHTMLHandler(adaptor);
const input = new TeX({packages: ['base', 'bussproofs']});
const outputJax = new SVG({fontData: MathJaxNewcmFont, fontCache: 'none', exFactor: 0.5});
outputJax.addExtension(MathJaxBboldxFontExtension);
outputJax.addExtension(MathJaxBbmFontExtension);
outputJax.addExtension(MathJaxDsfontFontExtension);
const document = mathjax.document('', {InputJax: input, OutputJax: outputJax});

function plain(value, seen = new WeakSet()) {
  if (value === undefined) return {$: 'undefined'};
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
  if (typeof value === 'number') return Number.isNaN(value) ? {$: 'NaN'} :
    Object.is(value, -0) ? {$: '-0'} : Number.isFinite(value) ? value : {$: value < 0 ? '-Infinity' : '-Infinity'};
  if (typeof value === 'function') return {$: 'function', name: value.name || '', length: value.length};
  if (typeof value !== 'object') return String(value);
  if (seen.has(value)) return {$: 'cycle'};
  seen.add(value);
  if (Array.isArray(value)) return Array.from({length: value.length}, (_, i) =>
    Object.hasOwn(value, i) ? plain(value[i], seen) : {$: 'hole'});
  if (value instanceof Map) return {$: 'map', entries: [...value].map(([k, v]) => [plain(k, seen), plain(v, seen)])};
  const result = {};
  for (const key of Reflect.ownKeys(value)) result[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] = plain(value[key], seen);
  return result;
}
function tree(node) {
  return node == null ? null : {kind: node.kind,
    attrs: plain(node.attributes?.getAllAttributes?.() ?? {}),
    properties: plain(node.getAllProperties?.() ?? {}),
    ...(typeof node.getText === 'function' ? {text: node.getText()} : {}),
    children: (node.childNodes || []).map(tree)};
}
function descriptors(object) {
  return Reflect.ownKeys(object).map(key => {
    const descriptor = Object.getOwnPropertyDescriptor(object, key);
    const name = typeof key === 'symbol' ? `symbol:${key.description || ''}` : key;
    return {name, enumerable: !!descriptor.enumerable, configurable: !!descriptor.configurable,
      ...(Object.hasOwn(descriptor, 'writable') ? {writable: !!descriptor.writable} : {}),
      ...(Object.hasOwn(descriptor, 'value') ? {value: plain(descriptor.value)} : {}),
      ...(descriptor.get ? {get: plain(descriptor.get)} : {}),
      ...(descriptor.set ? {set: plain(descriptor.set)} : {})};
  });
}
function sourceSurface() {
  const jax = new TeX({packages: ['base', 'bussproofs']});
  const item = new ProofTreeItem(jax.parseOptions.itemFactory);
  return {configuration: plain(BussproofsConfiguration),
    itemClass: {constructor: plain(ProofTreeItem), static: descriptors(ProofTreeItem),
      prototype: descriptors(ProofTreeItem.prototype), instanceFields: [
        {name: 'leftLabel', value: plain(item.leftLabel)},
        {name: 'rigthLabel', value: plain(item.rigthLabel)}]}};
}
function capture(input) {
  let jax = null;
  try {
    jax = new TeX({packages: ['base', 'bussproofs']});
    jax.setMmlFactory(new MmlFactory());
    const item = new AbstractMathItem(input, jax, true);
    return {result: tree(jax.compile(item, document)), error: null,
      packageData: plain(jax.parseOptions.packageData)};
  } catch (error) {
    return {result: null, error: {name: error.name || error.constructor.name,
      id: error.id ?? null, message: String(error.message)},
      packageData: jax ? plain(jax.parseOptions.packageData) : null};
  }
}
const cases = [
  ['proof-axiom-short', String.raw`\begin{prooftree}\AxiomC{A}\end{prooftree}`],
  ['proof-axiom-alias', String.raw`\begin{prooftree}\AXC{A}\end{prooftree}`],
  ['proof-unary', String.raw`\begin{prooftree}\AxiomC{A}\UnaryInfC{B}\end{prooftree}`],
  ['proof-binary', String.raw`\begin{prooftree}\AxiomC{A}\AxiomC{B}\BinaryInfC{C}\end{prooftree}`],
  ['proof-ternary', String.raw`\begin{prooftree}\AxiomC{A}\AxiomC{B}\AxiomC{C}\TrinaryInfC{D}\end{prooftree}`],
  ['proof-quaternary', String.raw`\begin{prooftree}\AxiomC{A}\AxiomC{B}\AxiomC{C}\AxiomC{D}\QuaternaryInfC{E}\end{prooftree}`],
  ['proof-quinary', String.raw`\begin{prooftree}\AxiomC{A}\AxiomC{B}\AxiomC{C}\AxiomC{D}\AxiomC{E}\QuinaryInfC{F}\end{prooftree}`],
  ['proof-unary-alias', String.raw`\begin{prooftree}\AXC{A}\UIC{B}\end{prooftree}`],
  ['proof-binary-alias', String.raw`\begin{prooftree}\AXC{A}\AXC{B}\BIC{C}\end{prooftree}`],
  ['proof-ternary-alias', String.raw`\begin{prooftree}\AXC{A}\AXC{B}\AXC{C}\TIC{D}\end{prooftree}`],
  ['proof-labels-left-right', String.raw`\begin{prooftree}\AxiomC{A}\LeftLabel{l}\RightLabel{r}\UnaryInfC{B}\end{prooftree}`],
  ['proof-label-left-alias', String.raw`\begin{prooftree}\AXC{A}\LL{L}\UIC{B}\end{prooftree}`],
  ['proof-label-right-alias', String.raw`\begin{prooftree}\AXC{A}\RL{R}\UIC{B}\end{prooftree}`],
  ['proof-no-line', String.raw`\begin{prooftree}\AxiomC{A}\noLine\UnaryInfC{B}\end{prooftree}`],
  ['proof-single-line', String.raw`\begin{prooftree}\AxiomC{A}\singleLine\UnaryInfC{B}\end{prooftree}`],
  ['proof-dashed-line', String.raw`\begin{prooftree}\AxiomC{A}\dashedLine\UnaryInfC{B}\end{prooftree}`],
  ['proof-persistent-dashed', String.raw`\begin{prooftree}\alwaysDashedLine\AxiomC{A}\UnaryInfC{B}\UnaryInfC{C}\end{prooftree}`],
  ['proof-root-at-top', String.raw`\begin{prooftree}\rootAtTop\AxiomC{A}\UnaryInfC{B}\end{prooftree}`],
  ['proof-root-at-bottom', String.raw`\begin{prooftree}\rootAtBottom\AxiomC{A}\UnaryInfC{B}\end{prooftree}`],
  ['proof-always-root-at-top', String.raw`\begin{prooftree}\alwaysRootAtTop\AxiomC{A}\UnaryInfC{B}\end{prooftree}`],
  ['proof-fcenter-axiom', String.raw`\begin{prooftree}\Axiom$A\fCenter B$\end{prooftree}`],
  ['proof-fcenter-extra-separator', String.raw`\begin{prooftree}\Axiom$A\fCenter B\fCenter C$\end{prooftree}`],
  ['proof-fcenter-inference', String.raw`\begin{prooftree}\Axiom$A\fCenter B$\UnaryInf$B\fCenter C$\end{prooftree}`],
  ['proof-recursive-binary', String.raw`\begin{prooftree}\AxiomC{A}\AxiomC{B}\BinaryInfC{A\land B}\AxiomC{C}\BinaryInfC{(A\land B)\land C}\end{prooftree}`],
  ['proof-leftover-formula', String.raw`\begin{prooftree}\AxiomC{A}x\end{prooftree}`],
  ['proof-command-outside', String.raw`\AxiomC{A}`],
  ['proof-inference-too-few', String.raw`\begin{prooftree}\AxiomC{A}\BinaryInfC{B}\end{prooftree}`],
  ['proof-unknown-command', String.raw`\begin{prooftree}\BadProofCommand{A}\end{prooftree}`],
  ['proof-missing-end', String.raw`\begin{prooftree}\AxiomC{A}`],
  ['proof-fcenter-missing', String.raw`\begin{prooftree}\Axiom$A=B$\end{prooftree}`],
  ['proof-fcenter-no-math-shift', String.raw`\begin{prooftree}\Axiom A\fCenter B\end{prooftree}`],
];
const observations = cases.map(([name, input]) => ({name, input, ...capture(input)}));
const maps = {};
for (const name of ['Bussproofs-macros', 'Bussproofs-environments']) {
  const map = MapHandler.getMap(name); maps[name] = map ? plain(map.map) : {$: 'missing-map'};
}
const output = {schema: 1, source: lock.source, files: lock.files,
  surface: sourceSurface(), cases: observations, maps};
const destination = path.join(root, 'build/math-port/bussproofs-original.json');
fs.mkdirSync(path.dirname(destination), {recursive: true});
const bytes = Buffer.from(`${JSON.stringify(output, null, 2)}\n`);
const actual = crypto.createHash('sha256').update(bytes).digest('hex');
if (process.env.BSPR_UPDATE_LOCK === '1') {
  lock.oracleOutputSha256 = actual;
  fs.writeFileSync(lockPath, `${JSON.stringify(lock, null, 2)}\n`);
} else if (lock.oracleOutputSha256) assert.equal(actual, lock.oracleOutputSha256, 'Bussproofs Source oracle output changed');
fs.writeFileSync(destination, bytes);
process.stdout.write(`${JSON.stringify({cases: observations.length, maps: Object.keys(maps).length,
  output: path.relative(root, destination), sha256: actual})}\n`);
