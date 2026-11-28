// TeX input-jax observations from the pinned MathJax 4.1.3 CJS implementation.
// This generator does not load native code or redefine Source expectations.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const scope = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/profiles/scope.json'), 'utf8'));
const source = 'ts/input/tex.ts';
const sourceLock = scope.files.find(file => file.source === source);
if (!sourceLock) throw new Error('scope lock lacks ' + source);
const sourceBytes = fs.readFileSync(path.join(sourceRoot, source));
const sourceHash = crypto.createHash('sha256').update(sourceBytes).digest('hex');
if (sourceHash !== sourceLock.source_sha256) throw new Error('pinned TeX Source hash changed');
if (fs.readFileSync(path.join(packageRoot, source), 'utf8').replaceAll('\r\n', '\n') !==
    sourceBytes.toString('utf8').replaceAll('\r\n', '\n')) throw new Error('installed TeX TS differs from pinned Source');
const cjsPath = path.join(packageRoot, 'cjs/input/tex.js');
const cjsBytes = fs.readFileSync(cjsPath);
const cjsHash = crypto.createHash('sha256').update(cjsBytes).digest('hex');
const expectedCjsHash = 'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada';
if (cjsHash !== expectedCjsHash) throw new Error('pinned TeX CJS hash changed');

const { TeX } = require(cjsPath);
const { MmlFactory } = require(path.join(packageRoot, 'cjs/core/MmlTree/MmlFactory.js'));
const { AbstractMathItem } = require(path.join(packageRoot, 'cjs/core/MathItem.js'));
const TexError = require(path.join(packageRoot, 'cjs/input/tex/TexError.js')).default;
const rows = [];
function plain(value, seen = new WeakSet()) {
  if (value === undefined) return { $: 'undefined' };
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
  if (typeof value === 'number') return Number.isNaN(value) ? { $: 'NaN' } : Object.is(value, -0) ? { $: '-0' } : value;
  if (typeof value === 'function') return { $: 'function', name: value.name, length: value.length };
  if (typeof value !== 'object') return String(value);
  if (seen.has(value)) return { $: 'cycle' };
  seen.add(value);
  if (Array.isArray(value)) return Array.from({ length: value.length }, (_, i) => Object.hasOwn(value, i) ? plain(value[i], seen) : { $: 'hole' });
  if (value instanceof Map) return [...value].map(([k, v]) => [plain(k, seen), plain(v, seen)]);
  if (value instanceof Set) return [...value].map(v => plain(v, seen));
  const result = {};
  for (const key of Reflect.ownKeys(value)) result[typeof key === 'symbol' ? `symbol:${key.description}` : key] = plain(value[key], seen);
  return result;
}
function errorRecord(fn) {
  try { return { result: plain(fn()), error: null }; }
  catch (error) { return { result: null, error: { name: error.name || error.constructor.name,
    id: error.id === undefined ? null : error.id, message: String(error.message) } }; }
}
function record(label, fn) { rows.push({ label, ...errorRecord(fn) }); }
function makeJax(options = { packages: ['base'] }) {
  const jax = new TeX(options);
  jax.setMmlFactory(new MmlFactory());
  return jax;
}
function tree(node) {
  if (!node) return null;
  return { kind: node.kind, attributes: plain(node.attributes && node.attributes.getAllAttributes()),
    properties: plain(node.getAllProperties && node.getAllProperties()),
    children: (node.childNodes || []).map(tree) };
}
function math(source, jax = makeJax()) { return new AbstractMathItem(source, jax, true); }

record('tex-class-function-descriptors-and-inheritance', () => ({
  constructor: { name: TeX.name, length: TeX.length, ownKeys: Reflect.ownKeys(TeX).map(String),
    nameDescriptor: Object.getOwnPropertyDescriptor(TeX, 'name'),
    lengthDescriptor: Object.getOwnPropertyDescriptor(TeX, 'length') },
  prototypeKeys: Reflect.ownKeys(TeX.prototype).map(String),
  methods: ['setMmlFactory', 'parseOptions', 'reset', 'compile', 'findMath', 'formatError'].map(name => ({
    name, descriptor: Object.getOwnPropertyDescriptor(TeX.prototype, name),
    function: Object.getOwnPropertyDescriptor(TeX.prototype, name)?.get ? plain(Object.getOwnPropertyDescriptor(TeX.prototype, name).get) : plain(TeX.prototype[name]) })),
  staticMethods: ['configure', 'tags'].map(name => ({ name, function: plain(TeX[name]), descriptor: Object.getOwnPropertyDescriptor(TeX, name) })),
  nameValue: TeX.NAME,
  optionKeys: Object.keys(TeX.OPTIONS),
  options: plain(TeX.OPTIONS),
}));

record('tex-constructor-source-initialization-and-live-parse-options', () => {
  const jax = makeJax();
  const getter = Object.getOwnPropertyDescriptor(TeX.prototype, 'parseOptions').get;
  return { ownKeys: Object.keys(jax), name: jax.name, getterIdentity: getter.call(jax) === jax.parseOptions,
    parseOptions: Object.keys(jax.parseOptions), configurationParsers: jax.configuration.parsers,
    options: plain(jax.options), tagsConfigurationIsParseOptions: jax.parseOptions.tags.configuration === jax.parseOptions,
    optionsRecordDistinct: jax.options !== jax.parseOptions.options,
    postFilterTuples: jax.postFilters.items.map(([fn, priority]) => [fn.name, priority]) };
});

record('tex-configure-actual-parser-configuration', () => {
  const config = TeX.configure(['base']);
  return { constructor: config.constructor.name, packages: config.packages, parsers: config.parsers,
    initialized: config.initialized, packageNames: config.configurations.map(item => item.name) };
});

record('tex-static-tags-adds-configuration-and-selects-default', () => {
  const jax = makeJax();
  const before = jax.parseOptions.tags;
  const config = jax.configuration;
  TeX.tags(jax.parseOptions, config);
  return { oldAndNewDiffer: before !== jax.parseOptions.tags, tagClass: jax.parseOptions.tags.constructor.name,
    configOwnTagKeys: Object.keys(config.tags), tagsConfigurationIsParseOptions: jax.parseOptions.tags.configuration === jax.parseOptions };
});

record('tex-reset-omitted-undefined-null-and-value', () => {
  const results = [];
  for (const [label, invoke] of [
    ['omitted', jax => jax.reset()], ['undefined', jax => jax.reset(undefined)],
    ['null', jax => jax.reset(null)], ['number', jax => jax.reset(7)],
  ]) {
    const jax = makeJax(); let calls = 0; const original = jax.parseOptions.tags.reset;
    jax.parseOptions.tags.reset = function (...args) { calls++; return original.apply(this, args); };
    const reads = []; const getter = Object.getOwnPropertyDescriptor(TeX.prototype, 'parseOptions').get;
    Object.defineProperty(jax, 'parseOptions', { configurable: true, get() { reads.push('parseOptions'); return getter.call(this); } });
    const outcome = errorRecord(() => invoke(jax));
    const parseOptionsReads = [...reads];
    results.push({ label, ...outcome, calls, counter: jax.parseOptions.tags.counter,
      currentTag: plain(jax.parseOptions.tags.currentTag), parseRoot: jax.parseOptions.root,
      parserLength: jax.parseOptions.parsers.length, nodeListKeys: Object.keys(jax.parseOptions.nodeLists),
      parseOptionsReads });
  }
  return results;
});

record('tex-findmath-delegates-live-findtex-and-ignores-second-argument', () => {
  const jax = makeJax(); const strings = ['inline \\(x\\)', 'display $$y$$'];
  const actual = jax.findMath(strings, { ignored: true });
  const receiver = { marker: 'findTeX receiver' }; let captured;
  jax.findTeX = { findMath(value) { captured = { receiverIsFindTeX: this === jax.findTeX, sameInput: value === strings }; return ['delegated']; } };
  const delegated = jax.findMath(strings, 'also ignored');
  return { actual: actual.map(value => plain(value)), receiver, captured, delegated };
});

record('tex-formaterror-regexp-line-boundary-and-real-mml', () => {
  const jax = makeJax();
  const error = new TexError('TestID', 'first line\nsecond line\nthird line');
  return { result: tree(jax.formatError(error)), error: { id: error.id, message: error.message } };
});

record('tex-setmmlfactory-base-and-nodefactory-share-one-object', () => {
  const jax = makeJax(); const factory = new MmlFactory();
  jax.setMmlFactory(factory);
  return { baseFactoryIdentity: jax.mmlFactory === factory,
    parseFactoryIdentity: jax.parseOptions.nodeFactory.mmlFactory === factory };
});

record('tex-compile-real-parser-mml-and-filter-state', () => {
  const jax = makeJax(); const item = math('x+1', jax);
  const result = jax.compile(item, {});
  return { result: tree(result), sameAsRoot: result === jax.parseOptions.root,
    latex: jax.latex, parseError: jax.parseOptions.error, parserCount: jax.parseOptions.parsers.length,
    itemInputIsJax: item.inputJax === jax, tagCounter: jax.parseOptions.tags.counter };
});

record('tex-compile-source-order-and-live-parse-options-getter', () => {
  const jax = makeJax(); const parseOptions = jax.parseOptions; const trace = [];
  const getter = Object.getOwnPropertyDescriptor(TeX.prototype, 'parseOptions').get;
  Object.defineProperty(jax, 'parseOptions', { configurable: true, get() { trace.push('parseOptions'); return getter.call(this); } });
  const wrap = (object, name, label) => {
    const original = object[name];
    object[name] = function (...args) { trace.push(label); return original.apply(this, args); };
  };
  wrap(parseOptions, 'clear', 'clear');
  wrap(parseOptions.tags, 'startEquation', 'startEquation');
  wrap(parseOptions.tags, 'finishEquation', 'finishEquation');
  wrap(jax, 'executeFilters', 'executeFilters');
  wrap(parseOptions.nodeFactory, 'create', 'nodeFactory.create');
  const item = math('x', jax); const result = jax.compile(item, {});
  return { trace, resultKind: result.kind, rootSame: result === parseOptions.root,
    parseError: parseOptions.error, stackParserCount: parseOptions.parsers.length };
});

record('tex-compile-formaterror-callback-receiver-and-return', () => {
  const calls = []; let jax;
  jax = makeJax({ packages: ['base'], formatError(receiver, err) {
    calls.push({ receiverIsJax: receiver === jax, id: err.id, message: err.message });
    return jax.parseOptions.nodeFactory.create('node', 'merror');
  } });
  const item = math('\\notACommand', jax);
  const result = jax.compile(item, {});
  return { calls, result: tree(result), parseError: jax.parseOptions.error };
});

const output = { source: { path: source, sha256: sourceHash }, cjs: { path: 'cjs/input/tex.js', sha256: cjsHash }, rows };
fs.writeFileSync(path.join(__dirname, 'tex-jax-original.json'), JSON.stringify(output, null, 2) + '\n');
process.stdout.write(`wrote ${rows.length} actual TeX input-jax observations\n`);
