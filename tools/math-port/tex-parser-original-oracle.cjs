// Independent observations from pinned MathJax 4.1.3 TexParser CJS and real TeX/MML peers.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const scopeBytes = fs.readFileSync(scopePath);
const scopeHash = crypto.createHash('sha256').update(scopeBytes).digest('hex');
const expectedScope = '9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1';
assert.equal(scopeHash, expectedScope);
const locks = {
  'ts/input/tex/TexParser.ts': '106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
  'input/tex/TexParser.js': '106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
  'input/tex.js': 'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
  'input/tex/Stack.js': '78743f9dfcf1ebf9beb890629d908111be5c1e47f49c16d5ffa0877c5935c385',
  'input/tex/MapHandler.js': 'd82e3034776d2392689aad86500b5e60dbe0aa5540853b48f308841879f3cf67',
  'input/tex/ParseOptions.js': 'ff19d70539cb76911c5f1d7673ddfd908183dcf5849c91cb068b19e58da8a9ec',
  'input/tex/StackItem.js': '53baa91d7c27475a91a9b6993235071ab32b04a29736c5920bdda2836ab5a91b',
  'input/tex/StackItemFactory.js': '2722b21be90f76b48325c22a65b37f80dc283e4bc9f74b644dcae81386d001c2',
  'input/tex/UnitUtil.js': '22fd8e826575bf9cd612b2f85208a49b07eb3cb6d9ace6c6db36e79d6e50bade',
  'input/tex/TexError.js': 'a5713111ff2ab646740da13ca6273cb5f2a7ceff9d0fb6a2e116185e4994e64f',
  'core/MmlTree/MmlFactory.js': 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'core/MmlTree/MmlNode.js': '86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
  'core/Tree/Factory.js': '739db2e5760c29d7bb4f1bc7baa08ff8d727f4d117522aff5ede017f812cffef',
};
const scope = JSON.parse(scopeBytes.toString('utf8'));
const modules = [];
for (const [key, expected] of Object.entries(locks)) {
  const cjsRelative = key.startsWith('ts/') ? key.slice(3).replace(/\.ts$/, '.js') : key;
  const cjsPath = path.join(packageRoot, 'cjs', cjsRelative);
  const bytes = fs.readFileSync(cjsPath);
  const hash = crypto.createHash('sha256').update(bytes).digest('hex');
  assert.equal(hash, expected, `${key} pinned CJS changed`);
  if (key.startsWith('ts/')) {
    const lock = scope.files.find(file => file.source === key);
    assert.ok(lock, `${key} absent from scope`);
    const original = fs.readFileSync(path.join(root, '.reference/MathJax-src', key));
    assert.equal(crypto.createHash('sha256').update(original).digest('hex'), lock.source_sha256);
    assert.equal(fs.readFileSync(path.join(packageRoot, key), 'utf8').replaceAll('\r\n', '\n'),
      original.toString('utf8').replaceAll('\r\n', '\n'));
    modules.push({ source: key, source_sha256: lock.source_sha256, cjs: cjsRelative, cjs_sha256: hash });
  } else {
    modules.push({ cjs: key, cjs_sha256: hash });
  }
}
const load = name => require(path.join(packageRoot, 'cjs', name));
const TexParser = load('input/tex/TexParser.js').default;
const { TeX } = load('input/tex.js');
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const rows = [];
function plain(value, seen = new WeakSet()) {
  if (value === undefined) return { $: 'undefined' };
  if (value === null) return null;
  if (typeof value === 'number') return Number.isNaN(value) ? { $: 'NaN' } : Object.is(value, -0) ? { $: '-0' } : value;
  if (typeof value === 'function') return { $: 'function', name: value.name || '', length: value.length };
  if (typeof value !== 'object') return value;
  if (seen.has(value)) return { $: 'cycle' };
  seen.add(value);
  if (Array.isArray(value)) return { $: 'array', items: Array.from({ length: value.length }, (_, i) => Object.hasOwn(value, i) ? plain(value[i], seen) : { $: 'hole' }) };
  const out = {};
  for (const key of Reflect.ownKeys(value)) out[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] = plain(value[key], seen);
  return out;
}
function error(fn) {
  try { return { result: plain(fn()), error: null }; }
  catch (e) { return { result: null, error: { name: e.name || e.constructor.name, id: e.id ?? null, message: String(e.message) } }; }
}
function record(label, fn) { rows.push({ label, ...error(fn) }); }
function jax() { const value = new TeX({ packages: ['base'] }); value.setMmlFactory(new MmlFactory()); return value; }
function parser(source = '', env = {}, configuration = jax().parseOptions) { return new TexParser(source, env, configuration); }
function nodeSummary(n) {
  if (!n) return null;
  return { kind: n.kind, attrs: plain(n.attributes?.getAllAttributes?.() || {}),
    properties: plain(n.getAllProperties?.() || {}), children: (n.childNodes || []).map(nodeSummary) };
}
function setupText(p, text, index = 0) { p.string = text; p.i = index; p.currentCS = '\\probe'; }
record('texparser-emitted-prototype-method-metadata', () => {
  const descriptors = Object.getOwnPropertyDescriptors(TexParser.prototype);
  return Object.fromEntries(Reflect.ownKeys(descriptors).map(key => [String(key), {
    enumerable: descriptors[key].enumerable, configurable: descriptors[key].configurable,
    get: descriptors[key].get ? { name: descriptors[key].get.name, length: descriptors[key].get.length } : null,
    set: descriptors[key].set ? { name: descriptors[key].set.name, length: descriptors[key].set.length } : null,
    value: typeof descriptors[key].value === 'function' ? { name: descriptors[key].value.name, length: descriptors[key].value.length } : plain(descriptors[key].value),
  }]));
});
record('constructor-real-parse-empty-token-macro-and-mml', () => {
  const outputs = [];
  for (const source of ['', 'x', 'x+1', '\\frac{1}{2}', '😀', 'x^2']) {
    const configuration = jax().parseOptions;
    const p = new TexParser(source, {}, configuration);
    const before = { string: p.string, i: p.i, macroCount: p.macroCount, currentCS: p.currentCS,
      topKind: p.stack.Top().kind, parserCount: configuration.parsers.length };
    const result = p.mml();
    outputs.push({ source, before, result: nodeSummary(result), parserCountAfterMml: configuration.parsers.length });
  }
  return outputs;
});
record('mml-reads-stack-top-independently-for-kind-and-first', () => {
  const p = parser('x');
  const stack = p.stack;
  const original = stack.Top;
  const reads = [];
  stack.Top = function (...args) {
    reads.push({ receiverIsStack: this === stack, argc: args.length });
    return original.apply(this, args);
  };
  const result = p.mml();
  return { reads, result: nodeSummary(result) };
});
record('constructor-env-isInner-own-test-delete-and-stack-copy', () => {
  const configuration = jax().parseOptions;
  const env = Object.create({ inherited: 'skip', isInner: false });
  env.font = 'bold'; env.isInner = false;
  const p = new TexParser('', env, configuration);
  const result = { envAfter: plain(env), stackEnv: plain(p.stack.env), stackInner: p.stack.Top().getProperty('isInner'),
    ownParserFields: Object.keys(p), descriptors: Object.getOwnPropertyDescriptors(p) };
  p.mml(); return result;
});
record('constructor-malformed-env-null-before-parser-push', () => {
  const configuration = jax().parseOptions;
  return { outcome: error(() => new TexParser('', null, configuration)), parserCount: configuration.parsers.length };
});
record('parser-getters-setter-and-default-string-replacement', () => {
  const p = parser();
  const initial = { optionsIdentity: p.options === p.configuration.options,
    itemFactoryIdentity: p.itemFactory === p.configuration.itemFactory,
    tagsIdentity: p.tags === p.configuration.tags, string: p.string };
  p.string = '𝑥 y'; p.i = 0; const codePoint = p.getCodePoint(); p.i += codePoint.length;
  return { initial, codePoint, afterOneCodePointIndex: p.i, nextIsSpace: p.nextIsSpace(), next: p.GetNext(), index: p.i };
});
record('parser-getnext-spaces-ascii-unicode-and-end', () => {
  const outputs = [];
  for (const text of [' \t\nX', '\u00a0x', '😀x', '']) {
    const p = parser(); setupText(p, text);
    outputs.push({ text, initialSpace: p.nextIsSpace(), next: p.GetNext(), index: p.i, afterSpace: p.nextIsSpace(), code: p.getCodePoint() });
  }
  return outputs;
});
record('parser-getcs-control-sequence-ascii-astral-and-empty', () => {
  const outputs = [];
  for (const text of ['alpha X', 'Abc', '😀X', '\\', '']) {
    const p = parser(); setupText(p, text, text.startsWith('\\') ? 1 : 0);
    outputs.push({ text, result: p.GetCS(), index: p.i });
  }
  return outputs;
});
record('parser-getargument-single-control-group-nesting-and-errors', () => {
  const outputs = [];
  for (const text of ['xR', '\\alpha tail', '{a{b}c}z', '{a\\}b}', '', '}', '{open']) {
    const p = parser(); setupText(p, text);
    outputs.push({ text, result: error(() => p.GetArgument('probe')), index: p.i });
  }
  const optional = parser(); setupText(optional, '   ');
  outputs.push({ text: 'optional-empty', result: error(() => optional.GetArgument('probe', true)), index: optional.i });
  return outputs;
});
record('parser-getbrackets-matching-escapes-braces-default-and-errors', () => {
  const outputs = [];
  for (const [text, match] of [['[a[b]c]z', true], ['[a[b]c]z', false], ['[{]}]z', true], ['[a\\]b]', true], ['}', false], ['[open', false]]) {
    const p = parser(); setupText(p, text);
    outputs.push({ text, match, result: error(() => p.GetBrackets('probe', 'DEFAULT', match)), index: p.i });
  }
  return outputs;
});
record('parser-getdelimiter-base-delimiters-and-errors', () => {
  const outputs = [];
  for (const [text, braceOK] of ['(', '\\{', '{(', '\\unknown', 'x', ''].map(x => [x, true])) {
    const p = parser(); setupText(p, text);
    outputs.push({ text, result: error(() => p.GetDelimiter('probe', braceOK)), index: p.i });
  }
  const p = parser(); setupText(p, '\\langle'); outputs.push({ text: '\\langle', result: error(() => p.GetDelimiter('probe')), index: p.i });
  return outputs;
});
record('parser-getdimen-braced-and-unbraced-source-consumption', () => {
  const outputs = [];
  for (const text of ['{1.25em}x', '2.5pt plus 1em', '{bad}', 'bad', '{1em', '']) {
    const p = parser(); setupText(p, text);
    outputs.push({ text, result: error(() => p.GetDimen('probe')), index: p.i, rest: p.string.slice(p.i) });
  }
  return outputs;
});
record('parser-getupto-braces-commands-token-and-error-state', () => {
  const outputs = [];
  for (const text of ['  a{b ;c;rest', 'a\\; rest', 'a}token', 'no ;']) {
    const p = parser(); setupText(p, text);
    outputs.push({ text, result: error(() => p.GetUpTo('probe', ';')), index: p.i });
  }
  return outputs;
});
record('parser-parsearg-parseupto-real-nested-parser-and-latex', () => {
  const outputs = [];
  for (const [text, method] of [['{x+1}rest', 'ParseArg'], ['x+1}tail', 'ParseUpTo']]) {
    const p = parser(); setupText(p, text);
    const result = error(() => p[method]('probe'));
    outputs.push({ text, method, result, index: p.i, nestedParserCount: p.configuration.parsers.length });
  }
  return outputs;
});
record('parser-delimiter-arg-and-star-state', () => {
  const outputs = [];
  for (const text of ['{(}', '  ', '\\{', 'x']) {
    const p = parser(); setupText(p, text);
    outputs.push({ text, delimiter: error(() => p.GetDelimiterArg('probe')), indexAfterDelimiter: p.i });
  }
  for (const text of ['*x', ' x', '']) {
    const p = parser(); setupText(p, text); outputs.push({ text, star: p.GetStar(), index: p.i });
  }
  return outputs;
});
record('parser-create-real-node-token-mathvariant-and-factory-errors', () => {
  const p = parser();
  const token = p.create('node', 'mi', [], { mathvariant: '-tex-caligraphic' });
  const ordinary = p.create('node', 'mi', [], { mathvariant: 'normal' });
  return { token: nodeSummary(token), ignoreVariant: token.getProperty('ignore-variant'), ordinary: nodeSummary(ordinary),
    invalidFactory: error(() => p.create('unknown-node-kind', 'mi')) };
});
record('parser-push-and-pushall-actual-inferred-mml-and-stackitem', () => {
  const p = parser(), nf = p.configuration.nodeFactory;
  const inferred = nf.create('node', 'mrow', [nf.create('node', 'mi')]);
  const before = p.stack.stack.length; p.Push(inferred); const afterInferred = p.stack.stack.length;
  const stop = p.itemFactory.create('stop'); p.i = 4; p.Push(stop);
  const afterItem = { startI: stop.startI, stopI: stop.stopI, startStr: stop.startStr, height: p.stack.stack.length };
  const row = nf.create('node', 'mrow', [nf.create('node', 'mi'), nf.create('node', 'mn')]);
  p.PushAll(row.childNodes); const afterAll = p.stack.stack.length;
  return { before, afterInferred, afterItem, afterAll, top: p.stack.Top().kind };
});
record('parser-parse-lookup-contains-tostring-live-subhandlers', () => {
  const j = jax(), p = new TexParser('', {}, j.parseOptions);
  const before = { containsX: p.contains('character', 'x'), lookupA: plain(p.lookup('character', 'a')),
    containsUnknown: p.contains('character', '😀'), string: p.toString() };
  p.configuration.handlers.set('temporary', p.configuration.handlers.get('character'));
  const after = { containsX: p.contains('temporary', 'x'), lookupX: plain(p.lookup('temporary', 'x')) };
  return { before, after };
});

const output = { schema: 1, source: 'actual pinned MathJax CJS classes, real TeX/ParseOptions/Stack/MML peers; no native code',
  sourceCommit: scope.upstream.commit, scope_sha256: scopeHash, modules, observations: rows };
const out = path.join(root, 'tools/math-port/profiles/evidence/tex-parser-original-source.json');
const serialized = JSON.stringify(output, null, 2) + '\n';
if (process.argv.includes('--check')) assert.equal(fs.readFileSync(out, 'utf8').replaceAll('\r\n', '\n'), serialized);
else fs.writeFileSync(out, serialized);
console.log(JSON.stringify({ observations: rows.length, module_files: modules.length, output: path.relative(root, out) }));
