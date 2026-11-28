'use strict';
// Additive pinned-CJS witness for empty TexParser/ParseArg/xArrow behavior.
// This executes only MathJax's actual locked CJS classes; it never loads native code.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const pkg = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const scopeBytes = fs.readFileSync(path.join(root, 'tools/math-port/profiles/scope.json'));
const scope = JSON.parse(scopeBytes.toString('utf8'));
const scopeHash = crypto.createHash('sha256').update(scopeBytes).digest('hex');
assert.equal(scopeHash, '9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1', 'scope lock');
const sourceFiles = [
  'ts/input/tex/TexParser.ts', 'ts/input/tex/Stack.ts', 'ts/input/tex/ParseUtil.ts',
  'ts/input/tex/Configuration.ts',
  'ts/input/tex/ams/AmsMethods.ts', 'ts/input/tex/ams/AmsMappings.ts',
  'ts/input/tex.ts', 'ts/core/MmlTree/MmlFactory.ts', 'ts/core/MmlTree/MmlNode.ts',
  'ts/util/FunctionList.ts', 'ts/core/InputJax.ts', 'ts/input/tex/textmacros/TextMacrosConfiguration.ts'
];
const cjsLocks = {
  'input/tex/TexParser.js': '106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
  'input/tex/Configuration.js': '86f62a6f6e44fec20156546b7e8d424cac493c7465c2c8ec621c604332a276f3',
  'input/tex/Stack.js': '78743f9dfcf1ebf9beb890629d908111be5c1e47f49c16d5ffa0877c5935c385',
  'input/tex/ParseUtil.js': 'adafa18cefad39e8408bcc0e4eb7103dceb6d16c9e7ea2eb013f23602c3f8941',
  'input/tex/ams/AmsMethods.js': '1b24b7bfa46c4e8764756ef66972cd788512fbd33bd80eeed18f88a2db72de51',
  'input/tex/ams/AmsMappings.js': '83337b1aafc4e7431050696abc27a3392c26e5626d70d22595d0b250bc5f2af9',
  'input/tex.js': 'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
  'core/MmlTree/MmlFactory.js': 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'core/MmlTree/MmlNode.js': '86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
  'util/FunctionList.js': '571f57801b863b2d9f01a66dc72f2836b7fd06a1a48a3fe0057e4ec3d51a7278',
  'core/InputJax.js': 'e2e330b4ba637c93bc3d2411e59129d681f865be7ad76297e8ab79d8f0f01f7a',
  'input/tex/ams/AmsConfiguration.js': 'ff8e1aa5936fb48e68da3cc5ff8745f1365d5dbd2416c1a908c6a06842989f79',
  'input/tex/textmacros/TextMacrosConfiguration.js': '5803611b187a6256cae6289cfc2f61fafa7f93395432d3a7b365f8089334b5cf'
};
const modules = sourceFiles.map(source => {
  const lock = scope.files.find(file => file.source === source);
  assert.ok(lock, `scope.json lacks ${source}`);
  const original = fs.readFileSync(path.join(sourceRoot, source));
  assert.equal(crypto.createHash('sha256').update(original).digest('hex'), lock.source_sha256, `${source} lock`);
  const installed = fs.readFileSync(path.join(pkg, source));
  assert.equal(installed.toString('utf8').replaceAll('\r\n', '\n'), original.toString('utf8').replaceAll('\r\n', '\n'), `${source} installed TS`);
  const cjs = source.replace(/^ts\//, 'cjs/').replace(/\.ts$/, '.js');
  const cjsBytes = fs.readFileSync(path.join(pkg, cjs));
  const cjsHash = crypto.createHash('sha256').update(cjsBytes).digest('hex');
  assert.equal(cjsHash, cjsLocks[cjs.replace(/^cjs\//, '')], `${cjs} CJS lock`);
  return { source, source_sha256: lock.source_sha256, cjs: cjs.replace(/^cjs\//, ''), cjs_sha256: cjsHash };
});
const amsConfigurationHash = crypto.createHash('sha256').update(fs.readFileSync(path.join(pkg, 'cjs/input/tex/ams/AmsConfiguration.js'))).digest('hex');
assert.equal(amsConfigurationHash, cjsLocks['input/tex/ams/AmsConfiguration.js'], 'AmsConfiguration.js CJS lock');
const load = file => require(path.join(pkg, 'cjs', file));
const { TeX } = load('input/tex.js');
const TexParser = load('input/tex/TexParser.js').default;
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const { AbstractMathItem } = load('core/MathItem.js');
const { MapHandler } = load('input/tex/MapHandler.js');
// This is the real package module: loading it registers its actual mappings.
load('input/tex/ams/AmsConfiguration.js');
load('input/tex/textmacros/TextMacrosConfiguration.js');

function plain(value, seen = new WeakSet()) {
  if (value === undefined) return { $: 'undefined' };
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
  if (typeof value === 'number') return Number.isNaN(value) ? { $: 'NaN' } : Object.is(value, -0) ? { $: '-0' } : value;
  if (typeof value === 'function') return { $: 'function', name: value.name || '', length: value.length };
  if (typeof value !== 'object') return String(value);
  if (seen.has(value)) return { $: 'cycle' };
  seen.add(value);
  if (Array.isArray(value)) return Array.from({ length: value.length }, (_, i) => Object.hasOwn(value, i) ? plain(value[i], seen) : { $: 'hole' });
  const out = {};
  for (const key of Reflect.ownKeys(value)) out[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] = plain(value[key], seen);
  return out;
}
function tree(n) {
  if (!n) return null;
  const out = { kind: n.kind, inferred: !!n.isInferred, token: !!n.isToken };
  if (n.isToken) out.text = n.getText();
  out.attrs = plain(n.attributes?.getAllAttributes?.() || {});
  out.properties = plain(n.getAllProperties?.() || {});
  out.children = Array.from({ length: n.childNodes?.length || 0 }, (_, i) =>
    Object.hasOwn(n.childNodes, i) ? tree(n.childNodes[i]) : { $: 'hole' });
  return out;
}
function stackItem(item) {
  if (!item) return null;
  return {
    kind: item.kind,
    first: Array.isArray(item.First) ? item.First.map(tree) : plain(item.First),
    nodes: Array.isArray(item.nodes) ? item.nodes.map(tree) : undefined,
    toString: String(item)
  };
}
function parserObservation(label, source, argName = 'probe') {
  const jax = new TeX({ packages: ['base', 'ams'] });
  jax.setMmlFactory(new MmlFactory());
  const parser = new TexParser(source, {}, jax.parseOptions);
  const before = {
    input: parser.string, index: parser.i, currentCS: parser.currentCS,
    stackHeight: parser.stack.height, top: stackItem(parser.stack.Top()), env: plain(parser.stack.env)
  };
  let parsed, error;
  try { parsed = parser.mml(); }
  catch (e) { error = { name: e.name || e.constructor.name, id: e.id ?? null, message: String(e.message) }; }
  return {
    label, source, afterConstructorBeforeMml: before,
    afterMml: {
      index: parser.i, currentCS: parser.currentCS, stackHeight: parser.stack.height,
      top: stackItem(parser.stack.Top()), env: plain(parser.stack.env),
      root: parsed ? tree(parsed) : null
    },
    error: error || null
  };
}
function xarrowObservation(label, input) {
  const jax = new TeX({ packages: ['base', 'ams'] });
  jax.setMmlFactory(new MmlFactory());
  const math = new AbstractMathItem(input, jax, true);
  let root, parser, error;
  const argumentCalls = [];
  const originalParseArg = TexParser.prototype.ParseArg;
  TexParser.prototype.ParseArg = function (name) {
    const call = { name, source: this.string, index: this.i, topBefore: stackItem(this.stack.Top()) };
    let result;
    try {
      result = originalParseArg.call(this, name);
      call.result = result ? tree(result) : null;
      return result;
    } catch (e) {
      call.error = { name: e.name || e.constructor.name, id: e.id ?? null, message: String(e.message) };
      throw e;
    } finally {
      call.indexAfter = this.i;
      call.topAfter = stackItem(this.stack.Top());
      argumentCalls.push(call);
    }
  };
  try {
    parser = new TexParser(input, { display: true, isInner: false }, jax.parseOptions);
    root = parser.mml();
  } catch (e) {
    error = { name: e.name || e.constructor.name, id: e.id ?? null, message: String(e.message) };
  } finally {
    TexParser.prototype.ParseArg = originalParseArg;
  }
  const beforeFilters = root ? tree(root) : null;
  let afterFilters = null, filterError = null;
  if (root) {
    jax.parseOptions.root = root;
    try {
      jax.postFilters.execute({ math, document: null, data: jax.parseOptions });
      afterFilters = tree(jax.parseOptions.root);
    } catch (e) {
      filterError = { name: e.name || e.constructor.name, id: e.id ?? null, message: String(e.message) };
    }
  }
  const maps = ['AMSmath-macros', 'AMSmath-mathchar0mo'].map(name => {
    const map = MapHandler.getMap(name);
    return { name, found: !!map, hasXArrow: !!map?.map?.xrightarrow, hasXLeftArrow: !!map?.map?.xleftarrow };
  });
  return {
    label, input, argumentCalls, beforeFilters, afterFilters, error: error || null, filterError,
    parser: parser ? { index: parser.i, string: parser.string, stackHeight: parser.stack.height, top: stackItem(parser.stack.Top()), env: plain(parser.stack.env) } : null,
    maps
  };
}

const rows = {
  parser: [parserObservation('empty-string', ''), parserObservation('empty-brace-argument-source', '{}')],
  textmacrosPackage: (() => {
    const { ParserConfiguration } = load('input/tex/Configuration.js');
    const appendCalls = [];
    const originalAppend = ParserConfiguration.prototype.append;
    ParserConfiguration.prototype.append = function (configuration, priority) {
      appendCalls.push({
        name: configuration.name,
        priority: priority === undefined ? { $: 'undefined' } : plain(priority),
        receiver: this.constructor.name
      });
      return originalAppend.call(this, configuration, priority);
    };
    let jax;
    try { jax = new TeX({ packages: ['base', 'textmacros'] }); }
    finally { ParserConfiguration.prototype.append = originalAppend; }
    jax.setMmlFactory(new MmlFactory());
    const packageData = jax.parseOptions.packageData.get('textmacros');
    const callbacks = Array.from(jax.preFilters, record => ({ name: record.item.name || '', length: record.item.length, priority: record.priority }));
    const before = {
      label: 'registered-before-execute', prefilterCount: callbacks.length, callbacks,
      configurationCount: jax.configuration.configurations.items.length,
      configurations: jax.configuration.configurations.items.map(record => ({
        name: record.item.name, priority: plain(record.priority)
      })),
      appendCalls,
      packageDataKeys: packageData ? Reflect.ownKeys(packageData).map(key => typeof key === 'symbol' ? `symbol:${key.description || ''}` : key) : null,
      hasInnerParseOptions: !!packageData?.parseOptions,
      innerFactoryBeforeExecute: packageData?.parseOptions?.nodeFactory?.mmlFactory === jax.mmlFactory,
      outerFactory: jax.mmlFactory ? 'set' : 'null'
    };
    let error = null;
    try {
      const math = new AbstractMathItem('x', jax, true);
      jax.preFilters.execute({ math, document: null, data: jax.parseOptions });
    } catch (e) {
      error = { name: e.name || e.constructor.name, id: e.id ?? null, message: String(e.message) };
    }
    const afterData = jax.parseOptions.packageData.get('textmacros');
    return [{
      ...before,
      afterExecute: {
        error,
        innerFactorySameAsOuter: afterData?.parseOptions?.nodeFactory?.mmlFactory === jax.mmlFactory,
        innerRoot: afterData?.parseOptions?.root === undefined ? { $: 'undefined' } : plain(afterData.parseOptions.root),
        innerNodeLists: Object.keys(afterData?.parseOptions?.nodeLists || {}),
        innerParserStack: afterData?.parseOptions?.parsers?.length ?? null
      }
    }];
  })(),
  xarrow: [
    xarrowObservation('empty-string', String.raw`\xrightarrow`),
    xarrowObservation('empty-brace-argument', String.raw`\xrightarrow{}`),
    xarrowObservation('empty-brackets-empty-brace', String.raw`\xrightarrow[]{} `),
    xarrowObservation('upper-label', String.raw`\xrightarrow{A}`),
    xarrowObservation('lower-label', String.raw`\xrightarrow[a]{}`),
    xarrowObservation('upper-lower-label', String.raw`\xrightarrow[a]{A}`),
    xarrowObservation('left-arrow-empty-brace', String.raw`\xleftarrow{}`)
  ]
};
const output = {
  schema: 1,
  provenance: { upstream: 'MathJax 4.1.3 pinned CJS', scope_sha256: scopeHash, modules },
  observations: rows
};
const destination = path.join(root, 'build/math-port/tex-parser-empty-xarrow-source.json');
fs.mkdirSync(path.dirname(destination), { recursive: true });
const serialized = JSON.stringify(output, null, 2) + '\n';
if (process.argv.includes('--check')) assert.equal(fs.readFileSync(destination, 'utf8').replaceAll('\r\n', '\n'), serialized);
else fs.writeFileSync(destination, serialized);
console.log(JSON.stringify({ parser: rows.parser.length, xarrow: rows.xarrow.length, destination: path.relative(root, destination) }));
