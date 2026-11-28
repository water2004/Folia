'use strict';

// Locked MathJax 4.1.3 Physics package behavior and registration observation.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const cjsRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const lock = JSON.parse(fs.readFileSync(path.join(root,
  'tools/math-port/locks/physics-source-lock.json'), 'utf8'));
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const hashBuffer = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
for (const file of lock.files) {
  assert.equal(hash(path.join(sourceRoot, file.ts)), file.tsSha256, `Source ${file.ts}`);
  assert.equal(hash(path.join(cjsRoot, file.cjs)), file.cjsSha256, `Source ${file.cjs}`);
}

const load = name => require(path.join(cjsRoot, 'cjs', name));
const {TeX} = load('input/tex.js');
const {MmlFactory} = load('core/MmlTree/MmlFactory.js');
const {AbstractMathItem} = load('core/MathItem.js');
const {MapHandler} = load('input/tex/MapHandler.js');
load('input/tex/physics/PhysicsConfiguration.js');

function plain(value, seen = new WeakSet()) {
  if (value === undefined) return {$: 'undefined'};
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
  if (typeof value === 'number') return Number.isNaN(value) ? {$: 'NaN'} :
    Object.is(value, -0) ? {$: '-0'} : Number.isFinite(value) ? value : {$: value < 0 ? '-Infinity' : 'Infinity'};
  if (typeof value === 'function') return {$: 'function', name: value.name || '', length: value.length};
  if (typeof value !== 'object') return String(value);
  if (seen.has(value)) return {$: 'cycle'};
  seen.add(value);
  if (Array.isArray(value)) return Array.from({length: value.length}, (_, i) =>
    Object.hasOwn(value, i) ? plain(value[i], seen) : {$: 'hole'});
  if (value instanceof Map) return {$: 'map', entries: [...value].map(([k, v]) => [plain(k, seen), plain(v, seen)])};
  const result = {};
  for (const key of Reflect.ownKeys(value)) {
    result[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] = plain(value[key], seen);
  }
  return result;
}
function tree(node) {
  return node == null ? null : {
    kind: node.kind,
    attrs: plain(node.attributes?.getAllAttributes?.() ?? {}),
    properties: plain(node.getAllProperties?.() ?? {}),
    ...(typeof node.getText === 'function' ? {text: node.getText()} : {}),
    children: (node.childNodes || []).map(tree),
  };
}
function capture(fn) {
  try { return {result: fn(), error: null}; }
  catch (error) {
    return {result: null, error: {name: error.name || error.constructor.name,
      id: error.id ?? null, message: String(error.message)}};
  }
}

const cases = [
  ['physics-quantity-round', String.raw`\qty(x)`, {}],
  ['physics-quantity-braced', String.raw`\qty{x}`, {}],
  ['physics-quantity-star', String.raw`\qty*{x}`, {}],
  ['physics-quantity-big', String.raw`\qty\big(x)`, {}],
  ['physics-quantity-bad-big', String.raw`\qty\foo(x)`, {}],
  ['physics-quantity-nested-open', String.raw`\qty(\qty[x])`, {}],
  ['physics-pqty', String.raw`\pqty{x}`, {}],
  ['physics-Bqty', String.raw`\Bqty{x}`, {}],
  ['physics-norm-order', String.raw`\order{x}`, {}],
  ['physics-eval-parentheses', String.raw`\eval(x)`, {}],
  ['physics-eval-braced-star', String.raw`\eval*{x}`, {}],
  ['physics-commutator', String.raw`\comm{A}{B}`, {}],
  ['physics-commutator-star-big', String.raw`\comm*\Big[A][B]`, {}],
  ['physics-vector-bold', String.raw`\vb{abc}`, {}],
  ['physics-vector-bold-star', String.raw`\vb*{a\alpha 1}`, {}],
  ['physics-vector-arrow', String.raw`\va{v}`, {}],
  ['physics-gradient-brace', String.raw`\gradient{f}`, {}],
  ['physics-curl-round', String.raw`\curl(v)`, {}],
  ['physics-laplacian-square', String.raw`\laplacian[f]`, {}],
  ['physics-expression-power', String.raw`\sin^2[x](x)`, {}],
  ['physics-expression-no-power', String.raw`\det(A)`, {}],
  ['physics-named-function', String.raw`\sine{x}`, {}],
  ['physics-operator-app-no-fence', String.raw`\grad x`, {}],
  ['physics-qq-star', String.raw`\qq*{if}`, {}],
  ['physics-qword', String.raw`x\qgiven y`, {}],
  ['physics-differential-no-argument', String.raw`\dd`, {}],
  ['physics-differential-brace', String.raw`\dd{x}`, {}],
  ['physics-differential-round', String.raw`\dd(x)`, {}],
  ['physics-diffd-default', String.raw`\diffd x`, {}],
  ['physics-diffd-italic', String.raw`\diffd x`, {physics: {italicdiff: true}}],
  ['physics-vnabla-default', String.raw`\vnabla`, {}],
  ['physics-vnabla-arrowdel', String.raw`\vnabla`, {physics: {arrowdel: true}}],
  ['physics-derivative-one-arg', String.raw`\dv{x}`, {}],
  ['physics-derivative-two-args', String.raw`\dv{f}{x}`, {}],
  ['physics-derivative-partial-three', String.raw`\pdv{f}{x}{y}`, {}],
  ['physics-derivative-star', String.raw`\dv*{f}{x}(x)`, {}],
  ['physics-bra-ket-pair', String.raw`\bra{a}\ket{b}`, {}],
  ['physics-bra-with-ket-command', String.raw`\bra{a}\ket{b}`, {}],
  ['physics-bra-only', String.raw`\Bra{a}`, {}],
  ['physics-innerproduct', String.raw`\ip{a}{b}`, {}],
  ['physics-ketbra-implicit', String.raw`\dyad{a}`, {}],
  ['physics-expectation', String.raw`\ev{A}`, {}],
  ['physics-expectation-state', String.raw`\ev{A}{\psi}`, {}],
  ['physics-matrix-element', String.raw`\mel{a}{A}{b}`, {}],
  ['physics-matrix-quantity-braces', String.raw`\mqty{a&b\\c&d}`, {}],
  ['physics-matrix-quantity-parentheses', String.raw`\mqty(a&b\\c&d)`, {}],
  ['physics-matrix-quantity-star', String.raw`\mqty*(a&b)`, {}],
  ['physics-small-matrix', String.raw`\smqty{a&b\\c&d}`, {}],
  ['physics-identity-matrix', String.raw`\imat{3}`, {}],
  ['physics-identity-matrix-invalid', String.raw`\imat{03}`, {}],
  ['physics-xmatrix-scalar', String.raw`\xmat{a}{1}{1}`, {}],
  ['physics-xmatrix-indexed', String.raw`\xmat*{a}{2}{3}`, {}],
  ['physics-xmatrix-invalid-dimension', String.raw`\xmat{a}{2}{03}`, {}],
  ['physics-pauli-x', String.raw`\pmat{x}`, {}],
  ['physics-pauli-unknown', String.raw`\pmat{q}`, {}],
  ['physics-diagonal', String.raw`\dmat{a,b,c}`, {}],
  ['physics-antidiagonal', String.raw`\admat{a,b,c}`, {}],
  ['physics-smallmatrix-environment', String.raw`\begin{smallmatrix}a&b\\c&d\end{smallmatrix}`, {}],
  ['physics-auto-close-paren', String.raw`\qty(x)`, {}],
  ['physics-auto-close-nested', String.raw`\qty(\qty[x])`, {}],
  ['physics-auto-close-extra', String.raw`\qty(x))`, {}],
  ['physics-auto-close-missing', String.raw`\qty(x`, {}],
  ['physics-unknown-command', String.raw`\physicsUnknown{x}`, {}],
];

const observations = cases.map(([name, input, options]) => {
  let jax = null;
  let result = null;
  let error = null;
  try {
    jax = new TeX({...options, packages: ['base', 'physics']});
    jax.setMmlFactory(new MmlFactory());
    const item = new AbstractMathItem(input, jax, true);
    result = tree(jax.compile(item, {}));
  } catch (caught) {
    error = {name: caught.name || caught.constructor.name, id: caught.id ?? null,
      message: String(caught.message)};
  }
  return {name, input, options, result, error,
    packageData: jax ? plain(jax.parseOptions.packageData) : null};
});

const mapNames = ['Physics-automatic-bracing-macros', 'Physics-vector-mo',
  'Physics-vector-mi', 'Physics-vector-macros', 'Physics-expressions-macros',
  'Physics-quick-quad-macros', 'Physics-derivative-macros',
  'Physics-bra-ket-macros', 'Physics-matrix-macros', 'Physics-aux-envs',
  'Physics-characters'];
const maps = {};
for (const name of mapNames) {
  const map = MapHandler.getMap(name);
  maps[name] = map ? plain(map.map) : {$: 'missing-map'};
}
const output = {schema: 1, source: lock.source, files: lock.files,
  cases: observations, maps};
const destination = path.join(root, 'build/math-port/physics-original.json');
fs.mkdirSync(path.dirname(destination), {recursive: true});
const bytes = Buffer.from(`${JSON.stringify(output, null, 2)}\n`);
assert.equal(hashBuffer(bytes), lock.oracleOutputSha256, 'Physics Source oracle output changed');
fs.writeFileSync(destination, bytes);
process.stdout.write(`${JSON.stringify({cases: observations.length, maps: Object.keys(maps).length,
  output: path.relative(root, destination)})}\n`);
