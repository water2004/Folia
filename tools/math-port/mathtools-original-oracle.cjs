'use strict';

// Pinned MathJax 4.1.3 Mathtools Source observations. This is development-only.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const cjsRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const sources = [
  'ts/input/tex/mathtools/MathtoolsConfiguration.ts',
  'ts/input/tex/mathtools/MathtoolsItems.ts',
  'ts/input/tex/mathtools/MathtoolsMappings.ts',
  'ts/input/tex/mathtools/MathtoolsMethods.ts',
  'ts/input/tex/mathtools/MathtoolsTags.ts',
  'ts/input/tex/mathtools/MathtoolsUtil.ts',
];
const files = sources.map(ts => {
  const cjs = ts.replace(/^ts\//, 'cjs/').replace(/\.ts$/, '.js');
  return {ts, tsSha256: hash(fs.readFileSync(path.join(sourceRoot, ts))),
    cjs, cjsSha256: hash(fs.readFileSync(path.join(cjsRoot, cjs)))};
});
const load = name => require(path.join(cjsRoot, 'cjs', name));
const {TeX} = load('input/tex.js');
const {MmlFactory} = load('core/MmlTree/MmlFactory.js');
const {AbstractMathItem} = load('core/MathItem.js');
const {MapHandler} = load('input/tex/MapHandler.js');
load('input/tex/ams/AmsConfiguration.js');
const configurationExports = load('input/tex/mathtools/MathtoolsConfiguration.js');
const itemExports = load('input/tex/mathtools/MathtoolsItems.js');
const mappingExports = load('input/tex/mathtools/MathtoolsMappings.js');
const methodExports = load('input/tex/mathtools/MathtoolsMethods.js');
const tagExports = load('input/tex/mathtools/MathtoolsTags.js');
const utilExports = load('input/tex/mathtools/MathtoolsUtil.js');

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
  for (const key of Reflect.ownKeys(value))
    result[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] = plain(value[key], seen);
  return result;
}
function tree(node) {
  return node == null ? null : {kind: node.kind,
    attrs: plain(node.attributes?.getAllAttributes?.() ?? {}),
    properties: plain(node.getAllProperties?.() ?? {}),
    ...(typeof node.getText === 'function' ? {text: node.getText()} : {}),
    children: (node.childNodes || []).map(tree)};
}
function classShape(ctor) {
  return {constructor: plain(ctor), prototype: Object.getOwnPropertyNames(ctor.prototype).map(name => {
    const descriptor = Object.getOwnPropertyDescriptor(ctor.prototype, name);
    return {name, value: typeof descriptor.value === 'function' ? plain(descriptor.value) : plain(descriptor.value),
      get: descriptor.get ? plain(descriptor.get) : null,
      set: descriptor.set ? plain(descriptor.set) : null};
  })};
}
function observe(name, input, options = {}) {
  let jax = null, result = null, error = null;
  try {
    jax = new TeX({...options, packages: ['base', 'ams', 'mathtools']});
    jax.setMmlFactory(new MmlFactory());
    result = tree(jax.compile(new AbstractMathItem(input, jax, true), {}));
  } catch (caught) {
    error = {name: caught.name || caught.constructor.name,
      id: caught.id ?? null, message: String(caught.message)};
  }
  return {name, input, options, result, error,
    packageData: jax ? plain(jax.parseOptions.packageData) : null,
    activeOptions: jax ? plain(jax.parseOptions.options.mathtools) : null,
    activeTags: jax ? plain(jax.parseOptions.options.tags) : null};
}

// Every method family and map is exercised, including malformed arguments and
// both ordinary and package-order-sensitive arrow behavior.
const cases = [
  ['mt-matrix', String.raw`\begin{matrix*}[r]a&b\\c&d\end{matrix*}`],
  ['mt-smallmatrix', String.raw`\begin{psmallmatrix*}a&b\\c&d\end{psmallmatrix*}`],
  ['mt-multlined', String.raw`\begin{multlined}[t][4em]a\\b\end{multlined}`],
  ['mt-shove', String.raw`\begin{align}a\shoveleft{+b}\\c\shoveright{+d}\end{align}`],
  ['mt-spreadlines', String.raw`\begin{spreadlines}{2pt}\begin{matrix}a\\b\end{matrix}\end{spreadlines}`],
  ['mt-cases', String.raw`\begin{dcases}x&x>0\\-x&x<0\end{dcases}`],
  ['mt-mathlap', String.raw`\mathllap{x}+\mathrlap{y}+\mathclap{z}`],
  ['mt-cramped', String.raw`\cramped{\frac{a}{b}}`],
  ['mt-textlap', String.raw`\textllap{left}\textclap{center}\textrlap{right}`],
  ['mt-makebox', String.raw`\mathmakebox[2em][c]{x}`],
  ['mt-mbox', String.raw`\mathmbox{x}`],
  ['mt-brackets', String.raw`\overbracket{x}\underbracket{y}`],
  ['mt-refeq', String.raw`\refeq{eq:x}`],
  ['mt-aboxed', String.raw`\begin{align}\Aboxed{a=b}\end{align}`],
  ['mt-aboxed-declare', String.raw`\MakeAboxedCommand{\mybox}\begin{align}\mybox{a=b}\end{align}`],
  ['mt-arrow-between-lines', String.raw`\begin{multline}a\\\ArrowBetweenLines\\b\end{multline}`],
  ['mt-vdotswithin', String.raw`\begin{matrix}a&b\\\vdotswithin{a}&c\end{matrix}`],
  ['mt-shortvdotswithin', String.raw`\begin{matrix}a&b\\\shortvdotswithin{a}&c\end{matrix}`],
  ['mt-flush-space', String.raw`\MTFlushSpaceAbove{2pt}\MTFlushSpaceBelow{1pt}`],
  ['mt-paired-delimiter', String.raw`\DeclarePairedDelimiter{\norm}{\lVert}{\rVert}\norm{x}`],
  ['mt-paired-delimiter-x', String.raw`\DeclarePairedDelimiterX{\set}[1]{\lbrace}{\rbrace}{#1}\set{x}`],
  ['mt-paired-delimiter-xpp', String.raw`\DeclarePairedDelimiterXPP{\abs}[1]{\lvert}{#1}{\rvert}{#1}\abs{x}`],
  ['mt-paired-delimiter-typo', String.raw`\DeclarePairedDelimiters{\myabs}{|}{|}\myabs{x}`],
  ['mt-colon', String.raw`a\vcentercolon b\ordinarycolon c\MTThinColon d`],
  ['mt-relations', String.raw`a\coloneqq b\Coloneqq c\eqqcolon d\Colonapprox e\colonsim f\dblcolon g\dashcolon h`],
  ['mt-narrow-arrows', String.raw`\nuparrow\ndownarrow`],
  ['mt-splitfrac', String.raw`\splitfrac{a+b}{c+d}+\splitdfrac{e}{f}`],
  ['mt-xmathstrut', String.raw`\xmathstrut{(}`],
  ['mt-prescript', String.raw`\prescript{a}{b}{X}`],
  ['mt-tagform', String.raw`\newtagform{brackets}{[}{]}\usetagform{brackets}\begin{equation}x\end{equation}`],
  ['mt-adjustlimits', String.raw`\adjustlimits\sum_{a}^{b}`],
  ['mt-mathtoolsset', String.raw`\mathtoolsset{centercolon=true,thincolon-dx=.1em}a\coloneqq b`],
  ['mt-xmapsto', String.raw`\xmapsto{f}`],
  ['mt-xmapsto-after-ams', String.raw`\xmapsto[below]{above}`],
  ['mt-cases-env', String.raw`\begin{cases*}x&positive\\-x&negative\end{cases*}`],
  ['mt-cramped-array', String.raw`\begin{crampedsubarray}{c}a\\b\end{crampedsubarray}`],
  ['mt-gathered', String.raw`\begin{lgathered}a\\b\end{lgathered}`],
  ['mt-delimiter', String.raw`\left\lparen x\right\rparen`],
  ['mt-invalid-width', String.raw`\begin{multlined}[t][bogus]x\end{multlined}`],
  ['mt-invalid-tag-form', String.raw`\newtagform{bad}{[}{]}`],
  ['mt-undefined-paired', String.raw`\DeclarePairedDelimiter{\frac}{(}{)}x`],
  ['mt-unknown', String.raw`\mathtoolsUnknown{x}`],
];
const observations = cases.map(([name, input, options]) => observe(name, input, options));
const mapNames = ['mathtools-macros', 'mathtools-legacycolonsymbols',
  'mathtools-environments', 'mathtools-delimiters', 'mathtools-characters'];
const maps = Object.fromEntries(mapNames.map(name => {
  const map = MapHandler.getMap(name);
  return [name, map ? plain(map.map) : {$: 'missing-map'}];
}));
const result = {schema: 1, source: 'MathJax 4.1.3 fb987178c2d279a99b0db5ea02e751691703955e',
  files, exports: {
    configuration: plain(configurationExports.MathtoolsConfiguration),
    itemClass: classShape(itemExports.MultlinedItem),
    mappingExports: Object.keys(mappingExports).sort(),
    methods: plain(methodExports.MathtoolsMethods),
    tagMethods: plain(tagExports),
    utilities: plain(utilExports.MathtoolsUtil),
  }, cases: observations, maps};
const destination = path.join(root, 'build/math-port/mathtools-original.json');
fs.mkdirSync(path.dirname(destination), {recursive: true});
const bytes = Buffer.from(`${JSON.stringify(result, null, 2)}\n`);
fs.writeFileSync(destination, bytes);
const lockPath = path.join(root, 'tools/math-port/locks/mathtools-source-lock.json');
const prior = fs.existsSync(lockPath) ? JSON.parse(fs.readFileSync(lockPath, 'utf8')) : {};
const lock = {...prior, schema: 1, source: result.source,
  scopeSha256: '9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1',
  oracleOutputSha256: hash(bytes), files};
fs.writeFileSync(lockPath, `${JSON.stringify(lock, null, 2)}\n`);
process.stdout.write(`${JSON.stringify({cases: observations.length, maps: Object.keys(maps).length,
  oracleSha256: lock.oracleOutputSha256, output: path.relative(root, destination)})}\n`);
