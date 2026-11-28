// Independent observations from the pinned MathJax 4.1.3 CJS classes.
// This script is intentionally outside tests/ auto-globs and never loads native code.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const scopeBytes = fs.readFileSync(scopePath);
const scopeHash = crypto.createHash('sha256').update(scopeBytes).digest('hex');
const expectedScopeHash = '9705d389538b2fc8bf782feeA4372ace639b3727304f2971937bbcfb0042a0d1'.toLowerCase();
assert.equal(scopeHash, expectedScopeHash, 'MathJax scope lock changed');
const expectedCjs = {
  'input/tex/Configuration.js': '86f62a6f6e44fec20156546b7e8d424cac493c7465c2c8ec621c604332a276f3',
  'input/tex/Tags.js': 'e027c7edb6f7207f3b3ce7e7203dcfc763219b1fa37a3b39925ae83c1beef896',
  'input/tex/ColumnParser.js': '9d09d0b921ce50255facfe303d7771309ad71ea473388167535d5acf711feae9',
  'input/tex/ParseOptions.js': 'ff19d70539cb76911c5f1d7673ddfd908183dcf5849c91cb068b19e58da8a9ec',
  'input/tex/TexParser.js': '106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
  'input/tex/StackItem.js': '53baa91d7c27475a91a9b6993235071ab32b04a29736c5920bdda2836ab5a91b',
  'input/tex/StackItemFactory.js': '2722b21be90f76b48325c22a65b37f80dc283e4bc9f74b644dcae81386d001c2',
  'input/tex/base/BaseItems.js': '5310d0429b7b62d405ae9b93c8d21ad17b787e5eb3fd1f01439c62f2094f7d1a',
  'input/tex/ParseUtil.js': 'adafa18cefad39e8408bcc0e4eb7103dceb6d16c9e7ea2eb013f23602c3f8941',
  'input/tex/UnitUtil.js': '22fd8e826575bf9cd612b2f85208a49b07eb3cb6d9ace6c6db36e79d6e50bade',
  'input/tex/TexError.js': 'a5713111ff2ab646740da13ca6273cb5f2a7ceff9d0fb6a2e116185e4994e64f',
  'input/tex.js': 'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
  'core/MathItem.js': 'e6ed5008854e4d2ac6b34ab98c53bec6afb34e460895e8320c90a0d8d700d261',
  'core/MmlTree/MmlNode.js': '86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
  'core/MmlTree/MmlFactory.js': 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'core/Tree/Factory.js': '739db2e5760c29d7bb4f1bc7baa08ff8d727f4d117522aff5ede017f812cffef',
  'util/Options.js': '76f3df8a3e899fba1d0944d5b79818b586c1d9f95cc7021c395fbb10295db660',
  'util/PrioritizedList.js': '787bd21f3634312af666917d8f140145864f98d09ede1bdde3c957d28ec2bc25',
  'util/FunctionList.js': '571f57801b863b2d9f01a66dc72f2836b7fd06a1a48a3fe0057e4ec3d51a7278',
};
const scope = JSON.parse(scopeBytes.toString('utf8'));
const sourceFiles = [
  'ts/input/tex/Configuration.ts',
  'ts/input/tex/Tags.ts',
  'ts/input/tex/ColumnParser.ts',
  'ts/input/tex/ParseOptions.ts',
  'ts/input/tex/TexParser.ts',
  'ts/input/tex/StackItem.ts',
  'ts/input/tex/StackItemFactory.ts',
  'ts/input/tex/base/BaseItems.ts',
  'ts/input/tex/ParseUtil.ts',
  'ts/input/tex/UnitUtil.ts',
  'ts/input/tex/TexError.ts',
  'ts/input/tex.ts',
  'ts/core/MathItem.ts',
  'ts/core/MmlTree/MmlNode.ts',
  'ts/core/MmlTree/MmlFactory.ts',
  'ts/core/Tree/Factory.ts',
  'ts/util/Options.ts',
  'ts/util/PrioritizedList.ts',
  'ts/util/FunctionList.ts',
];
const moduleFiles = sourceFiles.map(source => {
  const lock = scope.files.find(file => file.source === source);
  assert.ok(lock, `scope.json lacks ${source}`);
  const originalPath = path.join(sourceRoot, source);
  const packagePath = path.join(packageRoot, source);
  const original = fs.readFileSync(originalPath);
  const sourceHash = crypto.createHash('sha256').update(original).digest('hex');
  assert.equal(sourceHash, lock.source_sha256, `${source} source hash changed`);
  assert.equal(fs.readFileSync(packagePath, 'utf8').replaceAll('\r\n', '\n'),
    original.toString('utf8').replaceAll('\r\n', '\n'), `${source} installed TS clone changed`);
  const cjs = source.replace(/^ts\//, 'cjs/').replace(/\.ts$/, '.js');
  const cjsBytes = fs.readFileSync(path.join(packageRoot, cjs));
  const cjsName = cjs.replace(/^cjs\//, '');
  const cjsHash = crypto.createHash('sha256').update(cjsBytes).digest('hex');
  assert.equal(cjsHash, expectedCjs[cjsName], `${cjsName} pinned CJS changed`);
  return {
    source,
    source_sha256: sourceHash,
    cjs: cjsName,
    cjs_sha256: cjsHash,
  };
});

const load = name => require(path.join(packageRoot, 'cjs', name));
const { Configuration, ConfigurationHandler, ParserConfiguration } = load('input/tex/Configuration.js');
const { AbstractTags, AllTags, NoTags, Label, TagInfo, TagsFactory } = load('input/tex/Tags.js');
const { TeX } = load('input/tex.js');
const ParseOptions = load('input/tex/ParseOptions.js').default;
const { ColumnParser } = load('input/tex/ColumnParser.js');
const TexParser = load('input/tex/TexParser.js').default;
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const { AbstractMathItem } = load('core/MathItem.js');
const { FunctionList } = load('util/FunctionList.js');

const rows = [];
function plain(value, seen = new WeakSet()) {
  if (value === undefined) return { $: 'undefined' };
  if (value === null) return null;
  if (typeof value === 'number') {
    if (Number.isNaN(value)) return { $: 'NaN' };
    if (Object.is(value, -0)) return { $: '-0' };
    if (!Number.isFinite(value)) return { $: value < 0 ? '-Infinity' : 'Infinity' };
    return value;
  }
  if (typeof value === 'function') return { $: 'function', name: value.name || '' };
  if (typeof value !== 'object') return value;
  if (seen.has(value)) return { $: 'cycle' };
  seen.add(value);
  if (Array.isArray(value)) {
    const items = [];
    for (let i = 0; i < value.length; i++) items.push(Object.hasOwn(value, i) ? plain(value[i], seen) : { $: 'hole' });
    return { $: 'array', items };
  }
  if (value instanceof Map) return { $: 'map', entries: [...value].map(([k, v]) => [plain(k, seen), plain(v, seen)]) };
  if (value instanceof Set) return { $: 'set', values: [...value].map(v => plain(v, seen)) };
  const out = {};
  for (const key of Reflect.ownKeys(value)) {
    if (typeof key === 'symbol') out[`symbol:${key.description || ''}`] = plain(value[key], seen);
    else out[key] = plain(value[key], seen);
  }
  return out;
}
function errorRecord(fn) {
  try { return { result: plain(fn()), error: null }; }
  catch (error) { return { result: null, error: { name: error.name || error.constructor.name,
    id: error.id === undefined ? null : error.id, message: String(error.message) } }; }
}
function record(label, fn) {
  rows.push({ label, ...errorRecord(fn) });
}
function makeJax() {
  const jax = new TeX({ packages: ['base'] });
  jax.setMmlFactory(new MmlFactory());
  return jax;
}
function nodeSummary(node) {
  if (!node) return null;
  const attrs = node.attributes ? node.attributes.getAllAttributes() : {};
  const properties = node.getAllProperties ? node.getAllProperties() : {};
  return { kind: node.kind, attrs: plain(attrs), properties: plain(properties),
    children: (node.childNodes || []).map(nodeSummary) };
}
function makeRealColumnPeers() {
  const jax = makeJax();
  const parser = new TexParser('', {}, jax.parseOptions);
  const item = jax.parseOptions.itemFactory.create('array');
  return { jax, parser, item, columnParser: jax.parseOptions.columnParser };
}

// ParseOptions runtime state uses the real package-created parser graph.
record('parse-options-field-order-sharing-and-math-style-variants', () => {
  const jax = makeJax(), po = jax.parseOptions;
  const fresh = new ParseOptions(makeParserConfiguration(['base']), []);
  return {
    keys: Object.keys(po),
    handlersSharedWithConfiguration: po.handlers === jax.configuration.handlers,
    handlersConstructor: po.handlers.constructor.name,
    optionsOwnRecord: po.options !== jax.options,
    variantMap: [...ParseOptions.getVariant.keys()],
    selectedMathStyle: po.mathStyle === ParseOptions.getVariant.get(po.options.mathStyle),
    fresh: { keys: Object.keys(fresh), options: plain(fresh.options),
      mathStyleFallback: fresh.mathStyle === ParseOptions.getVariant.get('TeX') },
    descriptors: Object.getOwnPropertyDescriptors(Object.getPrototypeOf(po)),
  };
});
record('parse-options-parser-stack-live-array-and-identity', () => {
  const jax = makeJax(), po = jax.parseOptions, first = new TexParser('', {}, po), second = new TexParser('', {}, po);
  const parsers = po.parsers;
  const before = po.parser;
  po.pushParser(first); const afterFirst = po.parser;
  po.pushParser(second); const afterSecond = po.parser;
  const popped = po.popParser(); const afterPop = po.parser;
  return { before: before === undefined, sameArray: parsers === po.parsers,
    first: afterFirst === first, second: afterSecond === second, popReturnsUndefined: popped === undefined,
    afterPopIsFirst: afterPop === first, length: po.parsers.length };
});
record('parse-options-clear-rebinds-ephemeral-fields-and-resets-tags', () => {
  const jax = makeJax(), po = jax.parseOptions, parser = new TexParser('', {}, po);
  const parsers = po.parsers, lists = po.nodeLists, options = po.options, handlers = po.handlers;
  po.pushParser(parser); po.root = po.nodeFactory.create('node', 'mrow'); po.nodeLists = { keep: [po.root] }; po.error = true;
  const root = po.root, tags = po.tags, reset = tags.resetTag.bind(tags);
  let resetCalls = 0; tags.resetTag = function(...args) { resetCalls++; return reset(...args); };
  po.clear();
  return { newParsers: po.parsers !== parsers, newLists: po.nodeLists !== lists, rootWasNode: !!root,
    rootNull: po.root === null, error: po.error, parserUndefined: po.parser === undefined,
    optionsSame: po.options === options, handlersSame: po.handlers === handlers,
    tagsSame: po.tags === tags, resetCalls, fields: Object.keys(po) };
});
record('parse-options-addnode-inlists-and-getlist-live-tree-replacement', () => {
  const jax = makeJax(), po = jax.parseOptions, nf = po.nodeFactory;
  const live = nf.create('node', 'mi', [], {}, nf.create('text', 'x'));
  const dead = nf.create('node', 'mi', [], {}, nf.create('text', 'd'));
  const root = nf.create('node', 'mrow', [live]);
  po.root = root;
  po.nodeLists = {};
  po.addNode('custom', live); po.addNode('custom', dead); po.addNode('mi', live);
  const saved = po.nodeLists.custom;
  const filtered = po.getList('custom');
  const second = po.getList('custom');
  return { inLists: live.getProperty('in-lists'), savedLength: saved.length,
    resultLength: filtered.length, onlyLive: filtered[0] === live, replaced: saved !== filtered,
    secondReusesCurrent: po.nodeLists.custom === second, secondIsNewArray: second !== filtered,
    kindList: po.getList('mi').map(node => node.kind) };
});
record('parse-options-removelist-first-strict-match-preserves-array-identity', () => {
  const jax = makeJax(), po = jax.parseOptions, nf = po.nodeFactory;
  const a = nf.create('node', 'mi'), b = nf.create('node', 'mn');
  const list = [a, a, b]; po.nodeLists = { x: list };
  po.removeFromList('x', [a]);
  const afterOne = list.slice(); po.removeFromList('x', [a, a]);
  return { sameList: po.nodeLists.x === list, afterOne: afterOne.map(n => n.kind),
    final: list.map(n => n.kind), missingNoCreate: (po.removeFromList('missing', [b]), !Object.hasOwn(po.nodeLists, 'missing')) };
});
record('function-list-execute-binds-priority-entry-as-receiver', () => {
  const list = new FunctionList(), argument = { marker: 'source-arg' }, observed = [];
  function callback(...args) {
    observed.push({ receiverIsPriorityEntry: this === list.items[0], receiver: plain(this),
      arity: args.length, argumentIsOriginal: args[0] === argument });
  }
  list.add(callback, 23);
  const returnValue = list.execute(argument);
  return { observed, returnValue, entry: plain(list.items[0]), ownKeys: Object.keys(list.items[0]) };
});

// Configuration and module-level ConfigurationHandler behavior.
record('configuration-defaults-local-versus-create', () => {
  const local = Configuration.local();
  const namedName = `parser-family-4242-registered`;
  const named = Configuration.create(namedName);
  const fresh = Configuration.local();
  return {
    local: { name: local.name, registered: ConfigurationHandler.get(local.name) === local,
      priority: local.priority, parser: local.parser, handler: Object.keys(local.handler),
      handlersFresh: local.handler !== fresh.handler,
      eachDefaultFresh: Object.keys(local.handler).every(key => local.handler[key] !== fresh.handler[key]) },
    named: { name: named.name, registered: ConfigurationHandler.get(namedName) === named,
      priority: named.priority, parser: named.parser },
  };
});
record('configuration-config-property-get-order', () => {
  const trace = [], fn = function initMethod() {};
  const config = new Proxy({}, { get(_target, key) {
    trace.push(String(key));
    if (key === 'priority') return null;
    if (key === 'init') return fn;
    if (key === 'config') return [fn, 0];
    if (key === 'preprocessors' || key === 'postprocessors') return [];
    if (key === 'parser') return '';
    if (key === 'handler') return { macro: [] };
    return undefined;
  } });
  const item = Configuration.local(config);
  return { trace, initMethod: plain(item.initMethod), configMethod: plain(item.configMethod),
    initSame: item.init === fn, configSame: item.config === fn,
    priority: item.priority, parser: item.parser, handler: plain(item.handler) };
});
record('configuration-truthy-property-rereads-and-priority-coalescing', () => {
  const trace = [], init = () => {}, configure = () => {};
  let initReads = 0, configReads = 0;
  const input = {
    get priority() { trace.push('priority'); return undefined; },
    get init() { trace.push(`init${++initReads}`); return init; },
    get config() { trace.push(`config${++configReads}`); return configure; },
    get preprocessors() { trace.push('preprocessors'); return []; },
    get postprocessors() { trace.push('postprocessors'); return []; },
    get parser() { trace.push('parser'); return null; },
    get handler() { trace.push('handler'); return {}; },
    get fallback() { trace.push('fallback'); return {}; },
    get items() { trace.push('items'); return {}; },
    get tags() { trace.push('tags'); return {}; },
    get options() { trace.push('options'); return {}; },
    get nodes() { trace.push('nodes'); return {}; },
  };
  const item = Configuration.local(input);
  return { trace, initReads, configReads, initSame: item.init === init, configSame: item.config === configure,
    priority: item.priority, parser: item.parser };
});
record('configuration-falsy-init-config-and-array-processor-alias', () => {
  const processor = [], exact = [() => {}, 0], c = Configuration.local({ init: 0, config: '', preprocessors: [exact], postprocessors: [] });
  const c2 = Configuration.local({ preprocessors: processor });
  return { init: c.initMethod, config: c.configMethod, preprocessorMapped: c.preprocessors !== c2.preprocessors,
    tuple: plain(c.preprocessors[0]), inputTupleSame: c.preprocessors[0] === exact,
    holes: { length: c2.preprocessors.length, hole0: !Object.hasOwn(c2.preprocessors, 0) } };
});
record('configuration-array-map-captured-length-holes-and-getter-effects', () => {
  const trace = [], processors = new Array(3);
  Object.defineProperty(processors, '0', { configurable: true, enumerable: true, get() {
    trace.push('get:0'); processors[1] = function late() {}; processors.push(function beyondSnapshot() {}); return function first() {};
  } });
  Object.defineProperty(processors, '2', { configurable: true, enumerable: true, get() { trace.push('get:2'); return function third() {}; } });
  const item = Configuration.local({ preprocessors: processors });
  return { trace, inputLength: processors.length, outputLength: item.preprocessors.length,
    output: plain(item.preprocessors), own: Array.from({ length: item.preprocessors.length }, (_, i) => Object.hasOwn(item.preprocessors, i)) };
});
record('configuration-handler-object-assign-enumerability-symbol-and-proto-setter', () => {
  const trace = [], symbol = Symbol('source-symbol'), source = Object.create({ inherited: 'skip' });
  Object.defineProperty(source, 'hidden', { value: 'skip', enumerable: false });
  Object.defineProperty(source, 'macro', { enumerable: true, get() { trace.push('get:macro'); return ['own']; } });
  Object.defineProperty(source, '__proto__', { enumerable: true, value: { injected: true }, configurable: true });
  Object.defineProperty(source, symbol, { enumerable: true, get() { trace.push('get:symbol'); return 'symbol-value'; } });
  const item = Configuration.local({ handler: source });
  return { trace, ownKeys: Reflect.ownKeys(item.handler).map(k => typeof k === 'symbol' ? `symbol:${k.description}` : k),
    proto: plain(Object.getPrototypeOf(item.handler)), macro: item.handler.macro,
    symbol: item.handler[symbol], inherited: item.handler.inherited };
});
record('configuration-create-registers-only-after-success', () => {
  const name = `parser-family-4242-failed-create`;
  const config = { get priority() { return undefined; }, get init() { throw new Error('source-init-get'); } };
  const failed = errorRecord(() => Configuration.create(name, config));
  return { failed, absent: ConfigurationHandler.get(name) === undefined,
    localDoesNotRegister: ConfigurationHandler.get(Configuration.local().name) === undefined };
});
record('configuration-handler-live-map-keys-and-overwrite-order', () => {
  const before = [...ConfigurationHandler.keys()];
  const names = [`parser-family-4242-map-a`, `parser-family-4242-map-b`];
  const a = Configuration.create(names[0]), b = Configuration.create(names[1]);
  const first = ConfigurationHandler.keys();
  const firstKey = first.next().value;
  const added = Configuration.create(`parser-family-4242-map-c`);
  const rest = [...first];
  Configuration.create(names[0], { priority: 4 });
  return { priorCount: before.length, firstKey, restIncludesNew: rest.includes(added.name),
    replacementKeepsOrder: [...ConfigurationHandler.keys()].indexOf(names[0]) < [...ConfigurationHandler.keys()].indexOf(names[1]),
    getIdentity: ConfigurationHandler.get(names[0]) !== a, bIdentity: ConfigurationHandler.get(names[1]) === b };
});
record('configuration-property-reads-short-circuit-and-null-failure', () => {
  const trace = [], input = {
    get priority() { trace.push('priority'); return 0; },
    get init() { trace.push('init-test'); return false; },
    get config() { trace.push('config-test'); return 0; },
    get preprocessors() { trace.push('preprocessors'); return null; },
    get postprocessors() { trace.push('postprocessors'); return false; },
    get parser() { trace.push('parser'); return ''; },
    get handler() { trace.push('handler'); return null; },
    get fallback() { trace.push('fallback'); return false; },
    get items() { trace.push('items'); return 0; },
    get tags() { trace.push('tags'); return ''; },
    get options() { trace.push('options'); return false; },
    get nodes() { trace.push('nodes'); return null; },
  };
  const item = Configuration.local(input);
  const nullFailure = errorRecord(() => Configuration.local(null));
  return { trace, priority: item.priority, parser: item.parser, methods: [item.initMethod, item.configMethod],
    nullFailure };
});
record('configuration-init-config-falsy-short-circuit-read-count', () => {
  const trace = [], input = { get init() { trace.push('init'); return 0; }, get config() { trace.push('config'); return ''; } };
  const item = Configuration.local(input);
  return { trace, init: item.initMethod, config: item.configMethod };
});

record('configuration-init-config-truthy-getter-rereads-current-values', () => {
  const trace = []; let initReads = 0, configReads = 0;
  const firstInit = function firstInit() {}, secondInit = function secondInit() {};
  const firstConfig = function firstConfig() {}, secondConfig = function secondConfig() {};
  const input = {
    get init() { trace.push(`init${++initReads}`); return initReads === 1 ? firstInit : secondInit; },
    get config() { trace.push(`config${++configReads}`); return configReads === 1 ? firstConfig : secondConfig; },
  };
  const result = Configuration.local(input);
  return { trace, initReads, configReads, initStored: result.initMethod[0].name,
    configStored: result.configMethod[0].name };
});
record('configuration-runtime-descriptors-direct-constructor-and-mutable-method-metadata', () => {
  const raw = new Configuration('runtime-direct');
  const actual = Configuration.local({ priority: 12, init: [function sourceInit() {}, 4], config: [function sourceConfig() {}, 6] });
  const before = { priority: actual.priority, init: actual.init.name, initMethod: plain(actual.initMethod), configMethod: plain(actual.configMethod) };
  actual.priority = 23;
  actual.initMethod[0] = function replacementInit() {};
  const changedPair = actual.initMethod;
  actual.initMethod = [function replacedPairInit() {}, 9];
  return { configurationLength: Configuration.length, constructorLength: Configuration.prototype.constructor.length,
    rawOwnDescriptors: Object.getOwnPropertyDescriptors(raw), prototypeDescriptors: Object.getOwnPropertyDescriptors(Configuration.prototype),
    staticDescriptors: Object.getOwnPropertyDescriptors(Configuration), before,
    mutation: { priority: actual.priority, changedPairFirst: changedPair[0].name,
      initMethod: plain(actual.initMethod), init: actual.init.name },
    canDirectConstruct: raw.name === 'runtime-direct', defaults: { parser: raw.parser, priority: raw.priority,
      handler: plain(raw.handler), items: plain(raw.items), tags: plain(raw.tags), options: plain(raw.options), nodes: plain(raw.nodes) } };
});
record('configuration-callable-metadata-and-direct-call-versus-new', () => {
  const functionInfo = fn => ({ name: fn.name, length: fn.length });
  const methodInfo = (owner, names) => Object.fromEntries(names.map(name => {
    const descriptor = Object.getOwnPropertyDescriptor(owner, name);
    const fn = descriptor && (descriptor.value || descriptor.get);
    return [name, { descriptor: { enumerable: descriptor.enumerable, writable: descriptor.writable,
      configurable: descriptor.configurable }, function: fn ? functionInfo(fn) : null }];
  }));
  return { class: functionInfo(Configuration), staticMethods: methodInfo(Configuration, ['makeProcessor', '_create', 'create', 'local']),
    prototype: methodInfo(Configuration.prototype, ['constructor', 'init', 'config']),
    directCall: errorRecord(() => Configuration('not-new')), construct: errorRecord(() => new Configuration('with-new')),
    privateStaticCalls: { makeProcessor: plain(Configuration.makeProcessor(function directProcessor() {}, 29)),
      createInternal: (() => { const result = Configuration._create('internal-direct', {});
        return { name: result.name, registered: ConfigurationHandler.get('internal-direct') === result }; })() } };
});
record('configuration-preprocessor-map-override-get-and-receiver', () => {
  const trace = [], processors = [function sourcePre() {}], replacementFn = function replacementPre() {};
  const mapped = [[replacementFn, 31]];
  Object.defineProperty(processors, 'map', { configurable: true, value(callback) {
    trace.push(`map-this:${this === processors}`);
    trace.push(`map-callback:${callback(processors[0])[0].name}`);
    return mapped;
  } });
  const result = Configuration.local({ preprocessors: processors });
  return { trace, mappedIsReturned: result.preprocessors === mapped,
    resultNames: result.preprocessors.map(pair => pair[0].name) };
});

function makeParserConfiguration(packages = [], parsers = ['tex']) {
  return new ParserConfiguration(packages, parsers);
}
record('parser-configuration-own-fields-and-array-alias', () => {
  const packages = [], parsers = ['tex']; const config = makeParserConfiguration(packages, parsers);
  const descriptorShape = descriptors => Object.fromEntries(Object.entries(descriptors).map(([key, d]) => [key,
    { enumerable: d.enumerable, configurable: d.configurable, writable: 'writable' in d ? d.writable : undefined,
      getter: d.get && d.get.name || null, setter: d.set && d.set.name || null,
      valueType: 'value' in d ? (d.value === null ? 'null' : typeof d.value) : undefined }]));
  return { parsersAlias: config.parsers === parsers, handlers: config.handlers.constructor.name,
    itemKeys: Object.keys(config.items), tagKeys: Object.keys(config.tags), optionKeys: Object.keys(config.options),
    nodesKeys: Object.keys(config.nodes), ownDescriptors: descriptorShape(Object.getOwnPropertyDescriptors(config)),
    prototypeDescriptors: descriptorShape(Object.getOwnPropertyDescriptors(Object.getPrototypeOf(config))),
    configurationList: plain(config.configurations), initList: config.initMethod.constructor.name,
    configList: config.configMethod.constructor.name,
    constructorMetadata: { name: ParserConfiguration.name, length: ParserConfiguration.length,
      prototypeKeys: Reflect.ownKeys(ParserConfiguration.prototype).map(String),
      methods: Object.fromEntries(Reflect.ownKeys(ParserConfiguration.prototype).filter(k => k !== 'constructor').map(name => {
        const descriptor = Object.getOwnPropertyDescriptor(ParserConfiguration.prototype, name), fn = descriptor.value;
        return [String(name), { name: fn.name, length: fn.length, enumerable: descriptor.enumerable }];
      })) } };
});
record('parser-addpackage-snapshot-reverse-and-tuple-index-get-order', () => {
  const trace = [];
  const confA = Configuration.create(`parser-family-4242-append-a`, { priority: 3 });
  const confB = Configuration.create(`parser-family-4242-append-b`, { priority: 4 });
  const tuple = new Array(2); tuple.length = 2;
  Object.setPrototypeOf(tuple, Object.assign(Object.create(Array.prototype), {
    get 0() { trace.push('tuple:0'); return confA.name; },
    get 1() { trace.push('tuple:1'); return 0; },
  }));
  const packages = [tuple, confB.name];
  const parser = makeParserConfiguration(packages);
  packages.reverse();
  return { trace, samePackagesArray: parser.parsers !== packages,
    configOrder: [...parser.configurations].map(entry => [entry.item.name, entry.priority]),
    tupleLength: tuple.length };
});
record('parser-configuration-init-and-config-method-callback-order', () => {
  const trace = [], jax = makeJax();
  const init = function init(c) { trace.push(`init:${this === c}:${c === parserConfig}`); };
  const ready = function ready(c, receivedJax) { trace.push(`config:${this === c}:${c === parserConfig}:${receivedJax === jax}`); };
  const descriptor = Configuration.create(`parser-family-4242-callbacks`, { init, config: ready, preprocessors: [], postprocessors: [] });
  const parserConfig = makeParserConfiguration([descriptor.name]);
  parserConfig.init(); parserConfig.config(jax);
  return { trace, initEntries: parserConfig.initMethod.items.map(entry => [entry.item.name, entry.priority]),
    configEntries: parserConfig.configMethod.items.map(entry => [entry.item.name, entry.priority]) };
});
record('parser-append-processor-direct-index-and-live-object-assign-order', () => {
  const trace = [], parserConfig = makeParserConfiguration([]);
  const initFn = function init() {}, configFn = function configure() {};
  const initPair = Object.create({ 0: initFn, 1: 7 });
  const configPair = Object.create({ 0: configFn, 1: 8 });
  const source = {
    priority: 0, initMethod: initPair, configMethod: configPair,
    handler: {}, fallback: {}, items: {}, tags: {}, options: {}, nodes: {}, preprocessors: [], postprocessors: [],
  };
  const watched = new Proxy(source, { get(target, key, receiver) { trace.push(`get:${String(key)}`); return Reflect.get(target, key, receiver); } });
  const originalHandlersAdd = parserConfig.handlers.add;
  parserConfig.handlers.add = function (...args) { trace.push('handlers.add'); return originalHandlersAdd.apply(this, args); };
  parserConfig.append(watched, 0);
  return { trace, initEntries: parserConfig.initMethod.items.map(entry => [entry.item.name, entry.priority]),
    configEntries: parserConfig.configMethod.items.map(entry => [entry.item.name, entry.priority]),
    effectiveCount: parserConfig.configurations.items.length };
});
record('parser-append-reads-live-configuration-fields-and-method-tuples', () => {
  const parserConfig = makeParserConfiguration([]), trace = [];
  const config = Configuration.local({ init: [function initOriginal() {}, 1], config: [function configOriginal() {}, 2] });
  config.priority = 19;
  config.initMethod[0] = function initPairMutated() {};
  config.configMethod = [function configPairReplaced() {}, 8];
  for (const key of ['priority', 'initMethod', 'configMethod', 'handler', 'fallback', 'items', 'tags', 'options', 'nodes']) {
    const descriptor = Object.getOwnPropertyDescriptor(config, key);
    if (!descriptor || !descriptor.configurable) continue;
    const value = config[key];
    Object.defineProperty(config, key, { configurable: true, enumerable: true, get() { trace.push(`get:${key}`); return value; } });
  }
  parserConfig.append(config, 0);
  return { trace, init: parserConfig.initMethod.items.map(entry => [entry.item.name, entry.priority]),
    configure: parserConfig.configMethod.items.map(entry => [entry.item.name, entry.priority]) };
});
record('parser-append-processor-index-get-order-on-inherited-tuple', () => {
  const trace = [], parserConfig = makeParserConfiguration([]);
  const pair = Object.create({ get 0() { trace.push('pair:0'); return function inheritedInit() {}; },
    get 1() { trace.push('pair:1'); return 17; } });
  const config = Configuration.local({ init: pair, config: pair });
  config.initMethod = pair; config.configMethod = pair;
  parserConfig.append(config);
  return { trace, init: parserConfig.initMethod.items.map(entry => [entry.item.name, entry.priority]),
    config: parserConfig.configMethod.items.map(entry => [entry.item.name, entry.priority]) };
});
record('parser-constructor-config-getters-and-filter-array-map-override', () => {
  const trace = [], pre = [function pre() {}], post = [function post() {}];
  Object.defineProperty(pre, 'map', { value(callback) { trace.push(`pre-map:${this === pre}`); return [callback(pre[0])]; } });
  Object.defineProperty(post, 'map', { value(callback) { trace.push(`post-map:${this === post}`); return [callback(post[0])]; } });
  const descriptor = Configuration.create(`parser-family-4242-filters`, { preprocessors: pre, postprocessors: post });
  const parserConfig = makeParserConfiguration([descriptor.name]);
  const jax = makeJax(), filterTrace = [];
  for (const [name, list] of [['pre', jax.preFilters], ['post', jax.postFilters]]) {
    const add = list.add;
    list.add = function (...args) { filterTrace.push([name, ...args.map(arg => typeof arg === 'function' ? arg.name : arg)]); return add.apply(this, args); };
  }
  parserConfig.config(jax);
  return { trace, filterTrace, storedPre: plain(descriptor.preprocessors), storedPost: plain(descriptor.postprocessors) };
});
record('parser-addfilters-destructures-through-tuple-iterator-and-closes-it', () => {
  const trace = [], pre = function iteratorPre() {}, tuple = [];
  Object.defineProperty(tuple, Symbol.iterator, { configurable: true, get() {
    trace.push(`iterator-get:${this === tuple}`);
    let index = 0;
    const iterator = {
      next() { const current = index++; trace.push(`next:${current}`); return current === 0 ? { value: pre, done: false } :
        current === 1 ? { value: 33, done: false } : { value: 'unused-third', done: false }; },
      return() { trace.push(`return:${this === iterator}`); return { done: true }; },
    };
    return function () { trace.push(`iterator-call:${this === tuple}`); return iterator; };
  } });
  const name = `parser-family-4242-custom-tuple`, descriptor = Configuration.create(name, { preprocessors: [tuple] });
  const parserConfig = makeParserConfiguration([name]), jax = makeJax(), added = [];
  const add = jax.preFilters.add;
  jax.preFilters.add = function (...args) { added.push([args[0].name, args[1]]); return add.apply(this, args); };
  parserConfig.config(jax);
  return { trace, added, storedSameTuple: descriptor.preprocessors[0] === tuple };
});
record('parser-getpackage-parser-target-error-and-missing-warning', () => {
  const parserConfig = makeParserConfiguration([], ['other']);
  const targetName = `parser-family-4242-wrong-parser`;
  Configuration.create(targetName, { parser: 'tex' });
  const wrong = errorRecord(() => parserConfig.addPackage(targetName));
  const warnings = [], priorWarn = console.warn; console.warn = value => warnings.push(value);
  let missing; try { missing = parserConfig.getPackage(`parser-family-4242-missing`); } finally { console.warn = priorWarn; }
  return { wrong, warnings, missing: plain(missing) };
});
record('parser-add-package-real-tex-order-and-configuration-callback-receiver', () => {
  const trace = [], jax = makeJax(), parserConfig = makeParserConfiguration([]);
  const name = `parser-family-4242-runtime-add`;
  const itemCtor = function RuntimeArray() {};
  const tagClass = class RuntimeTag extends NoTags {};
  const config = Configuration.create(name, {
    items: { array: itemCtor }, tags: { runtime: tagClass }, options: { displayAlign: 'left' },
    nodes: { runtimeNode: function RuntimeNode() {} },
    preprocessors: [function runtimeFilter() {}],
    config(c, receivedJax) { trace.push(`descriptor-receiver:${this === config}`); trace.push(`args:${c === parserConfig}:${receivedJax === jax}`); },
  });
  const addCreator = jax.parseOptions.nodeFactory.setCreators;
  jax.parseOptions.nodeFactory.setCreators = function (nodes) { trace.push(`creators:${nodes === config.nodes}`); return addCreator.call(this, nodes); };
  const addClass = jax.parseOptions.itemFactory.setNodeClass;
  jax.parseOptions.itemFactory.setNodeClass = function (...args) { trace.push(`item:${args[0]}:${args[1] === itemCtor}`); return addClass.apply(this, args); };
  const tagsAdd = TagsFactory.addTags;
  TagsFactory.addTags = function (tags) { trace.push(`tags:${tags === config.tags}`); return tagsAdd.call(this, tags); };
  const priorAdd = jax.preFilters.add;
  jax.preFilters.add = function (...args) { trace.push(`filter:${args[0].name}:${args[1]}`); return priorAdd.apply(this, args); };
  const outcome = errorRecord(() => parserConfig.add(name, jax, { displayAlign: 'center' }));
  TagsFactory.addTags = tagsAdd;
  return { outcome, trace, itemInstalled: jax.parseOptions.itemFactory.getNodeClass('array') === itemCtor,
    option: jax.parseOptions.options.displayAlign, configPresent: parserConfig.configurations.items.length };
});
record('parser-configuration-public-fields-are-live-authoritative-properties', () => {
  const parserConfig = makeParserConfiguration([]);
  const originalItems = parserConfig.items, replacementItems = { marker: 'replacement' };
  parserConfig.items = replacementItems;
  const config = Configuration.local({ items: { liveAdded: function LiveItem() {} } });
  parserConfig.append(config);
  const packageName = `parser-family-4242-live-parser-list`;
  Configuration.create(packageName, { parser: 'other' });
  parserConfig.parsers = ['other'];
  const packageCheck = errorRecord(() => parserConfig.addPackage(packageName));
  return { originalItemsUnchanged: Object.keys(originalItems).length === 0,
    replacementItems: Object.keys(replacementItems), itemIdentity: parserConfig.items === replacementItems,
    parsersIdentity: parserConfig.parsers[0] === 'other', packageCheck };
});
record('parser-add-default-user-options-undefined-and-null', () => {
  const name = `parser-family-4242-user-options`, jax = makeJax();
  Configuration.create(name, { preprocessors: [], postprocessors: [] });
  const outcomes = [];
  for (const mode of ['omitted', 'undefined', 'null']) {
    const parserConfig = makeParserConfiguration([]);
    const run = mode === 'omitted' ? () => parserConfig.add(name, jax) :
      mode === 'undefined' ? () => parserConfig.add(name, jax, undefined) : () => parserConfig.add(name, jax, null);
    outcomes.push({ mode, ...errorRecord(run), options: plain(jax.parseOptions.options) });
  }
  return outcomes;
});

// Real Tags/AbstractTags peers use a real TeX, ParseOptions, TexParser and MML factory.
record('tag-record-constructor-default-and-explicit-values', () => ({
  labelDefault: plain(new Label()), labelExplicitUndefined: plain(new Label(undefined, undefined)),
  labelNulls: plain(new Label(null, null)), tagInfoDefault: plain(new TagInfo()),
  tagInfoExplicit: plain(new TagInfo('align', true, false, null, 'id', ['(', '4', ')'], true, 'lab')),
  tagInfoUndefinedDefaults: plain(new TagInfo(undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined)),
}));
record('tags-class-runtime-arity-own-enumerability-and-prototype-inheritance', () => {
  const info = ctor => ({ name: ctor.name, length: ctor.length,
    ownKeys: Reflect.ownKeys(ctor).map(key => String(key)),
    prototypeKeys: Reflect.ownKeys(ctor.prototype).map(key => String(key)) });
  const none = new NoTags(), all = new AllTags(), abstract = new AbstractTags();
  return { classes: [AbstractTags, NoTags, AllTags, Label, TagInfo].map(info),
    instances: { noneIsAbstract: none instanceof AbstractTags, allIsAbstract: all instanceof AbstractTags,
      abstractEnTagOwn: Object.hasOwn(abstract, 'enTag'), enTagDescriptor: Object.getOwnPropertyDescriptor(abstract, 'enTag'),
      currentTagOwn: Object.hasOwn(abstract, 'currentTag'), currentTagDescriptor: Object.getOwnPropertyDescriptor(abstract, 'currentTag') },
    noTagsFinalizeInherited: NoTags.prototype.finalize === AbstractTags.prototype.finalize,
    allTagsFinalizeOverride: AllTags.prototype.finalize !== AbstractTags.prototype.finalize };
});
record('abstract-tags-start-tag-notag-end-history-label', () => {
  const tags = new AbstractTags();
  tags.label = 'outer'; tags.start('align', true, true); tags.tag('7', false); tags.start('split', false, false); tags.label = 'inner'; tags.notag();
  const inner = { env: tags.env, tag: plain(tags.currentTag), label: tags.label, noTag: tags.currentTag.noTag };
  tags.end(); const restored = { env: tags.env, label: tags.label, noTag: tags.currentTag.noTag, history: plain(tags.history) };
  tags.end(); return { inner, restored, finalEnv: tags.env, finalLabel: tags.label, history: plain(tags.history) };
});
record('abstract-tags-gettag-gates-auto-and-force-real-mml', () => {
  const jax = makeJax(), tags = new AbstractTags(); tags.configuration = jax.parseOptions;
  tags.start('equation', true, true);
  const automatic = tags.getTag();
  const afterAuto = { counter: tags.counter, tag: plain(tags.currentTag.tag), tagId: tags.currentTag.tagId, node: nodeSummary(automatic) };
  tags.start('notag-env', true, false); tags.notag(); const denied = tags.getTag();
  tags.start('force-env', false, false); tags.currentTag.tagFormat = ['[', '12', ']']; tags.currentTag.tag = '12';
  const forced = tags.getTag(true);
  return { afterAuto, denied: plain(denied), forced: nodeSummary(forced), counter: tags.counter,
    current: plain(tags.currentTag), activeParsers: jax.parseOptions.parsers.length };
});
record('abstract-tags-format-url-and-encode-uri-errors', () => {
  const tags = new AbstractTags();
  return { url: tags.formatUrl('a b/#?', 'https://x.test/'), unicode: tags.formatUrl('λ𝑥', 'base'),
    loneSurrogate: errorRecord(() => tags.formatUrl('\ud800', 'base')) };
});
record('abstract-tags-format-tag-ref-label-and-cleartag', () => {
  const tags = new AbstractTags(), trace = [];
  const oldFormat = tags.formatTag;
  tags.formatTag = function (tag) { trace.push(`formatTag:${tag}`); return oldFormat.call(this, tag); };
  tags.start('equation', true, false); tags.label = 'first'; tags.tag('8', false);
  const formatted = plain(tags.currentTag.tagFormat);
  const reference = tags.formatRef('8');
  tags.currentTag.tagId = 'stale'; tags.clearTag();
  return { formatted, reference, trace, tag: plain(tags.currentTag.tag), tagId: tags.currentTag.tagId,
    noTag: tags.noTag, label: tags.label };
});
record('abstract-tags-reset-and-equation-lifetime-real-math-item', () => {
  const jax = makeJax(), tags = new AbstractTags(); tags.configuration = jax.parseOptions;
  tags.allCounter = 5; tags.counter = 12; tags.allIds.keep = true; tags.allLabels.keep = new Label('old', 'old-id');
  const math = new AbstractMathItem('x', jax, true); math.inputData.recompile = { counter: 3, state: math.state() };
  tags.startEquation(math); const begun = { counter: tags.counter, refUpdate: tags.refUpdate, ids: plain(tags.ids), labels: plain(tags.labels), currentTag: plain(tags.currentTag) };
  tags.redo = true; tags.currentTag.tag = 'tag'; tags.finishEquation(math);
  const finished = { recompile: plain(math.inputData.recompile), counter: tags.counter, allCounter: tags.allCounter,
    allIds: plain(tags.allIds), allLabels: plain(tags.allLabels) };
  tags.reset(9); return { begun, finished, reset: { counter: tags.counter, allCounter: tags.allCounter,
    allIds: plain(tags.allIds), allLabels: plain(tags.allLabels), label: tags.label, history: plain(tags.history), redo: tags.redo, refUpdate: tags.refUpdate } };
});
record('abstract-tags-finalize-enTag-real-tex-parser-and-mml', () => {
  const jax = makeJax(), tags = new AbstractTags(); tags.configuration = jax.parseOptions;
  tags.currentTag = new TagInfo('', true, false, '4', '', ['(', '4', ')'], false, 'eq:four');
  const node = jax.parseOptions.nodeFactory.create('node', 'mi', [], { mathvariant: 'italic' });
  const result = tags.finalize(node, { display: true });
  return { result: nodeSummary(result), original: nodeSummary(node), labels: plain(tags.labels),
    tagId: tags.currentTag.tagId, parsers: jax.parseOptions.parsers.length };
});
record('abstract-tags-finalize-display-env-tag-short-circuits', () => {
  const node = { marker: 'unchanged' }, tags = new AbstractTags();
  tags.currentTag = new TagInfo('equation', true, false, '1');
  const nondisplay = tags.finalize(node, { display: false });
  const inEnvironment = tags.finalize(node, { display: true });
  tags.currentTag.env = ''; tags.currentTag.tag = null;
  const noTag = tags.finalize(node, { display: true });
  return { nondisplaySame: nondisplay === node, envSame: inEnvironment === node, noTagSame: noTag === node };
});
record('abstract-tags-finalize-live-guard-order-and-owned-entag-dispatch', () => {
  const jax = makeJax(), tags = new AbstractTags(); tags.configuration = jax.parseOptions;
  const trace = [], node = jax.parseOptions.nodeFactory.create('node', 'mi', []);
  Object.defineProperty(tags, 'currentTag', { configurable: true, get() {
    trace.push('currentTag');
    return { get env() { trace.push('tag.env'); return ''; }, get tag() { trace.push('tag.tag'); return null; } };
  } });
  const env = { get display() { trace.push('env.display'); return true; } };
  const skipped = tags.finalize(node, env);
  const tagRecord = new TagInfo('', true, false, '1', '', '1');
  Object.defineProperty(tags, 'currentTag', { configurable: true, get() { trace.push('currentTag-enTag'); return tagRecord; } });
  let receiver = false, args = [];
  tags.enTag = function (...values) { receiver = this === tags; args = values; trace.push('enTag'); return values[0]; };
  tags.makeTag = function () { trace.push('makeTag'); return node; };
  const finalized = tags.finalize(node, { display: true });
  return { trace, skippedSame: skipped === node, finalSame: finalized === node,
    ownEnTagDescriptor: Object.getOwnPropertyDescriptor(tags, 'enTag'), receiver,
    args: args.map(value => value === node ? 'same-node' : plain(value)) };
});
record('no-tags-and-all-tags-overrides', () => {
  const jax = makeJax(), none = new NoTags(), all = new AllTags(); none.configuration = all.configuration = jax.parseOptions;
  none.start('eq', true, true); const noneBefore = none.getTag(); none.currentTag.tag = 'explicit'; const noneExplicit = none.getTag();
  all.start('eq', true, false); all.currentTag.tag = '9'; all.currentTag.tagFormat = '9';
  const node = jax.parseOptions.nodeFactory.create('node', 'mi', []);
  const first = all.finalize(node, { display: true });
  all.end();
  const second = all.finalize(node, { display: true });
  return { noneBefore: plain(noneBefore), noneExplicit: nodeSummary(noneExplicit), allFirst: nodeSummary(first),
    allSecondIsOriginal: second === node, history: plain(all.history), counter: all.counter };
});
record('tags-factory-default-fallback-addtags-key-selection', () => {
  const oldDefault = TagsFactory.getDefault().constructor;
  const name = `parser-family-4242-tag-class`;
  TagsFactory.add(name, NoTags);
  TagsFactory.setDefault(name);
  const fallback = TagsFactory.getDefault();
  const inherited = { inherited: NoTags }, tags = Object.assign(Object.create(inherited), { own: AllTags });
  Object.defineProperty(tags, 'hidden', { value: NoTags, enumerable: false });
  TagsFactory.addTags(tags);
  const selectedOwn = TagsFactory.create('own');
  TagsFactory.add(name, AllTags); const selected = TagsFactory.create(name);
  const unknown = TagsFactory.create(`parser-family-4242-not-added`);
  TagsFactory.setDefault('none');
  return { oldDefault: oldDefault.name, registeredKeys: ['own', 'inherited', 'hidden'].map(key => ({ key, className: TagsFactory.create(key).constructor.name })),
    defaultWasFallback: fallback.constructor === NoTags, selectedOwn: selectedOwn.constructor === AllTags,
    selected: selected.constructor === AllTags,
    unknownUsesDefault: unknown.constructor === NoTags };
});
record('tags-factory-options-record-and-default-selection-are-independent', () => {
  const optionsRecord = TagsFactory.OPTIONS, initialTagsOption = optionsRecord.tags;
  const oldDefaultInstance = TagsFactory.getDefault();
  TagsFactory.setDefault('all');
  const afterSet = TagsFactory.getDefault();
  optionsRecord.tags = 'none';
  const afterOptionMutation = TagsFactory.getDefault();
  TagsFactory.setDefault('none');
  return { optionsIdentityStable: TagsFactory.OPTIONS === optionsRecord, initialTagsOption,
    afterSet: afterSet.constructor.name, optionsMutationDoesNotSetDefault: afterOptionMutation.constructor.name,
    oldInstance: oldDefaultInstance.constructor.name, optionsOwnDescriptors: Object.getOwnPropertyDescriptors(optionsRecord) };
});

// ColumnParser always receives the real parser and actual ArrayItem from TeX's factories.
record('column-constructor-default-handler-keys-and-maxcolumns', () => {
  const { columnParser } = makeRealColumnPeers();
  const methods = ['constructor', 'process', 'processColumn', 'setColumnAlign', 'setColumnWidths',
    'setColumnSpacing', 'setColumnLines', 'setPadding', 'getColumn', 'getDimen', 'getAlign',
    'getBraces', 'macroColumn', 'addRule', 'addAt', 'addBang', 'repeat', 'brace'];
  return { max: columnParser.MAXCOLUMNS, handlerKeys: Object.keys(columnParser.columnHandler),
    handlerTypes: Object.fromEntries(Object.entries(columnParser.columnHandler).map(([key, value]) => [key, typeof value])),
    runtimeMethods: Object.fromEntries(methods.map(name => { const fn = ColumnParser.prototype[name];
      return [name, { length: fn.length, name: fn.name,
        enumerable: Object.getOwnPropertyDescriptor(ColumnParser.prototype, name).enumerable }]; })),
    columnParserClass: { name: ColumnParser.name, length: ColumnParser.length,
      ownKeys: Reflect.ownKeys(ColumnParser).map(key => String(key)) } };
});
const templates = [
  ['basic-alignments', 'lcr'], ['column-dimensions', 'p{1em}m{2ex}b{3pt}'],
  ['explicit-alignment-fallback', 'w{1em}{r}W{2em}{c}'], ['rules-and-spacing', '|c::r|'],
  ['prefix-and-suffix', '>{\\bf}c<{\\it}'], ['at-and-bang', '@{x}c!{y}r'],
  ['repeat', '*{2}{lr}'], ['braced-expansion', '{lc}'], ['macro-columns', 'P{1em}M{2em}B{3em}'],
  ['space-newline', ' l\n c '], ['astral-handler-error', '😀'], ['unknown-handler-error', 'x'],
  ['missing-argument', 'p'], ['bad-dimension', 'p{bad}'], ['missing-close-brace', 'p{1em'],
  ['repeat-bad-number', '*{02}{c}'], ['nested-repeat', '*{2}{c*{2}{r}}'],
];
for (const [label, template] of templates) record(`column-process-${label}`, () => {
  const { jax, parser, item, columnParser } = makeRealColumnPeers();
  const before = { cstart: item.cstart, cend: item.cend, cextra: item.cextra, ralign: item.ralign };
  const run = errorRecord(() => columnParser.process(parser, template, item));
  return { template, outcome: run, arraydef: plain(item.arraydef), frame: plain(item.frame),
    cstart: plain(item.cstart), cend: plain(item.cend), cextra: plain(item.cextra), ralign: plain(item.ralign),
    aliasPreserved: before.cstart === item.cstart && before.cend === item.cend && before.cextra === item.cextra && before.ralign === item.ralign,
    parserString: parser.string, parserI: parser.i, parsers: jax.parseOptions.parsers.length };
});
record('column-process-handler-and-finalizer-order-live-lookups', () => {
  const { parser, item, columnParser } = makeRealColumnPeers(), trace = [];
  const original = columnParser.columnHandler;
  columnParser.columnHandler = new Proxy(original, {
    get(target, key, receiver) { trace.push(`get:${String(key)}`); return Reflect.get(target, key, receiver); },
    getOwnPropertyDescriptor(target, key) { trace.push(`desc:${String(key)}`); return Reflect.getOwnPropertyDescriptor(target, key); },
  });
  const hooks = ['setColumnAlign', 'setColumnWidths', 'setColumnSpacing', 'setColumnLines', 'setPadding'];
  for (const method of hooks) {
    const prior = columnParser[method].bind(columnParser);
    columnParser[method] = function (...args) { trace.push(method); return prior(...args); };
  }
  columnParser.process(parser, 'lc', item);
  return { trace, arraydef: plain(item.arraydef) };
});
record('column-processColumn-own-handler-only-and-function-receiver', () => {
  const { parser, item, columnParser } = makeRealColumnPeers();
  let received = null, state = null;
  const inherited = function () { throw new Error('inherited handler must not run'); };
  const handlers = Object.create({ z: inherited });
  Object.defineProperty(handlers, 'z', { configurable: true, enumerable: true, get() {
    return function (actualState) { received = this === handlers; state = actualState; actualState.calign[actualState.j++] = 'custom'; };
  } });
  columnParser.columnHandler = handlers;
  const ownRun = errorRecord(() => columnParser.process(parser, 'z', item));
  const inheritedRun = errorRecord(() => columnParser.process(parser, 'x', item));
  return { ownRun, inheritedRun, receiverIsHandler: received, stateFields: state && {
    parserIsReal: state.parser === parser, template: state.template, i: state.i, j: state.j,
    c: state.c, calign: plain(state.calign), cstartAliases: state.cstart === item.cstart,
    ralignAliases: state.ralign === item.ralign }, arraydef: plain(item.arraydef) };
});
record('column-processColumn-rereads-replaced-handler-object', () => {
  const { parser, item, columnParser } = makeRealColumnPeers(), trace = [];
  const first = { x: true };
  const second = { x(state) { trace.push(`call-receiver:${this === second}`); state.calign[state.j++] = 'from-second'; } };
  let reads = 0;
  Object.defineProperty(columnParser, 'columnHandler', { configurable: true, get() { trace.push(`handler${++reads}`); return reads === 1 ? first : second; } });
  const outcome = errorRecord(() => columnParser.process(parser, 'x', item));
  return { outcome, reads, trace, align: plain(item.arraydef.columnalign) };
});
record('column-repeat-number-boundaries-and-partial-state', () => {
  const results = [];
  for (const num of ['02', '+2', '2.0', '2x', '-1', '0', '2']) {
    const { parser, item, columnParser } = makeRealColumnPeers();
    const outcome = errorRecord(() => columnParser.process(parser, `*{${num}}{c}`, item));
    results.push({ num, outcome, arraydef: plain(item.arraydef), parserI: parser.i });
  }
  return results;
});
record('column-process-partial-array-writes-before-invalid-column', () => {
  const { parser, item, columnParser } = makeRealColumnPeers();
  columnParser.columnHandler = {
    a(state) { state.cstart[0] = 'prefix'; state.j++; },
    x() { throw new Error('intentional handler failure'); },
  };
  return { outcome: errorRecord(() => columnParser.process(parser, 'ax', item)),
    arraydef: plain(item.arraydef), cstart: plain(item.cstart),
    cend: plain(item.cend), cextra: plain(item.cextra), ralign: plain(item.ralign) };
});
record('column-process-captured-maxcolumns-boundary', () => {
  const { parser, item, columnParser } = makeRealColumnPeers(); columnParser.MAXCOLUMNS = 1;
  return { exactlyLimit: errorRecord(() => columnParser.process(parser, 'll', item)),
    overLimit: errorRecord(() => columnParser.process(parser, 'lll', item)), max: columnParser.MAXCOLUMNS };
});
record('column-public-brace-align-dimen-direct-via-real-state', () => {
  const { parser, item, columnParser } = makeRealColumnPeers(); let state;
  const original = columnParser.processColumn;
  columnParser.processColumn = function (value, c) { state = value; return original.call(this, value, c); };
  columnParser.process(parser, 'c', item);
  state.template = '  {a{b}\\}c}tail'; state.i = 0; state.c = 'p';
  const braces = columnParser.getBraces(state); const afterBraces = { i: state.i, rest: state.template.slice(state.i) };
  state.template = '{bad}'; state.i = 0;
  const align = columnParser.getAlign(state);
  state.template = '{2em}'; state.i = 0; state.c = 'p';
  const dim = columnParser.getDimen(state);
  state.template = '{bad}'; state.i = 0;
  const badDim = errorRecord(() => columnParser.getDimen(state));
  return { braces, afterBraces, align, dim, badDim };
});
record('column-macro-getbraces-error-boundaries', () => {
  const { parser, item, columnParser } = makeRealColumnPeers(); let state;
  const original = columnParser.processColumn;
  columnParser.processColumn = function (value, c) { state = value; return original.call(this, value, c); };
  columnParser.process(parser, 'c', item);
  const run = template => { state.template = template; state.i = 0; state.c = '@'; return errorRecord(() => columnParser.getBraces(state)); };
  return { empty: run(''), trailingSpace: run('  '), loneSlash: run('\\'), loneHighSurrogate: run('\ud800'), nestedEscaped: run('{x\\}y}') };
});

const output = {
  schema: 1,
  source: 'actual pinned MathJax 4.1.3 CJS; no native runtime or fake upstream parser peers',
  sourceCommit: scope.upstream.commit,
  scope_sha256: scopeHash,
  modules: moduleFiles,
  observations: rows,
};
const outPath = path.join(root, 'tools/math-port/profiles/evidence/parser-family-original-source.json');
const serialized = JSON.stringify(output, null, 2) + '\n';
if (process.argv.includes('--check')) {
  assert.equal(fs.readFileSync(outPath, 'utf8').replaceAll('\r\n', '\n'), serialized);
} else {
  fs.writeFileSync(outPath, serialized);
}
console.log(JSON.stringify({ observations: rows.length, module_files: moduleFiles.length,
  output: path.relative(root, outPath), scope_sha256: scopeHash }));
