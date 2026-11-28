// Independent observations from pinned MathJax 4.1.3 ParseUtil CJS and real TeX/MML peers.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const scopeBytes = fs.readFileSync(path.join(root, 'tools/math-port/profiles/scope.json'));
const scopeHash = crypto.createHash('sha256').update(scopeBytes).digest('hex');
assert.equal(scopeHash, '9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1');
const scope = JSON.parse(scopeBytes.toString('utf8'));
const locks = {
  'ts/input/tex/ParseUtil.ts': 'ADAFA18CEFAD39E8408BCC0E4EB7103DCEB6D16C9E7EA2EB013F23602C3F8941',
  'input/tex/ParseUtil.js': 'ADAFA18CEFAD39E8408BCC0E4EB7103DCEB6D16C9E7EA2EB013F23602C3F8941',
  'input/tex/TexParser.js': '106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
  'input/tex.js': 'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
  'input/tex/ParseOptions.js': 'ff19d70539cb76911c5f1d7673ddfd908183dcf5849c91cb068b19e58da8a9ec',
  'input/tex/Stack.js': '78743f9dfcf1ebf9beb890629d908111be5c1e47f49c16d5ffa0877c5935c385',
  'input/tex/StackItem.js': '53baa91d7c27475a91a9b6993235071ab32b04a29736c5920bdda2836ab5a91b',
  'input/tex/UnitUtil.js': '22fd8e826575bf9cd612b2f85208a49b07eb3cb6d9ace6c6db36e79d6e50bade',
  'input/tex/TexError.js': 'a5713111ff2ab646740da13ca6273cb5f2a7ceff9d0fb6a2e116185e4994e64f',
  'input/tex/base/BaseItems.js': '5310D0429B7B62D405AE9B93C8D21AD17B787E5EB3FD1F01439C62F2094F7D1A',
  'input/tex/NodeUtil.js': '4A55841FD22167D25F505E8F41ADEA072715EF024E00EA5AC6DF3F199F2CB809',
  'core/MmlTree/MmlFactory.js': 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'core/MmlTree/MmlNode.js': '86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
  'core/Tree/Factory.js': '739db2e5760c29d7bb4f1bc7baa08ff8d727f4d117522aff5ede017f812cffef',
  'util/Entities.js': 'A616936550AA115AD994C5F1F7D3BFB50CB818C2876BEAAE6B60A6E58A647D07',
};
const modules = [];
for (const [key, expected] of Object.entries(locks)) {
  const cjsRelative = key.startsWith('ts/') ? key.slice(3).replace(/\.ts$/, '.js') : key;
  const bytes = fs.readFileSync(path.join(packageRoot, 'cjs', cjsRelative));
  const hash = crypto.createHash('sha256').update(bytes).digest('hex');
  if (expected !== 'TODO') assert.equal(hash, expected.toLowerCase(), `${key} pinned CJS changed`);
  if (key.startsWith('ts/')) {
    const lock = scope.files.find(file => file.source === key);
    assert.ok(lock, `${key} absent from scope`);
    const original = fs.readFileSync(path.join(root, '.reference/MathJax-src', key));
    assert.equal(crypto.createHash('sha256').update(original).digest('hex'), lock.source_sha256);
    assert.equal(fs.readFileSync(path.join(packageRoot, key), 'utf8').replaceAll('\r\n', '\n'), original.toString('utf8').replaceAll('\r\n', '\n'));
    modules.push({ source: key, source_sha256: lock.source_sha256, cjs: cjsRelative, cjs_sha256: hash });
  } else modules.push({ cjs: key, cjs_sha256: hash });
}
const load = name => require(path.join(packageRoot, 'cjs', name));
const { ParseUtil, KeyValueDef, KeyValueTypes } = load('input/tex/ParseUtil.js');
const TexParser = load('input/tex/TexParser.js').default;
const { TeX } = load('input/tex.js');
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const rows = [];
function plain(v, seen = new WeakSet()) {
  if (v === undefined) return { $: 'undefined' };
  if (v === null) return null;
  if (typeof v === 'number') return Number.isNaN(v) ? { $: 'NaN' } : Object.is(v, -0) ? { $: '-0' } : v;
  if (typeof v === 'function') return { $: 'function', name: v.name || '', length: v.length };
  if (typeof v !== 'object') return v;
  if (seen.has(v)) return { $: 'cycle' };
  seen.add(v);
  if (Array.isArray(v)) return { $: 'array', items: Array.from({ length: v.length }, (_, i) => Object.hasOwn(v, i) ? plain(v[i], seen) : { $: 'hole' }) };
  const o = {};
  for (const k of Reflect.ownKeys(v)) o[typeof k === 'symbol' ? `symbol:${k.description || ''}` : k] = plain(v[k], seen);
  return o;
}
function error(fn) { try { return { result: plain(fn()), error: null }; } catch (e) { return { result: null, error: { name: e.name || e.constructor.name, id: e.id ?? null, message: String(e.message) } }; } }
function record(label, fn) { rows.push({ label, ...error(fn) }); }
function jax() { const x = new TeX({ packages: ['base'] }); x.setMmlFactory(new MmlFactory()); return x; }
function parser(x = jax()) { return new TexParser('', {}, x.parseOptions); }
function node(n) { return !n ? null : { kind: n.kind, attrs: plain(n.attributes?.getAllAttributes?.() || {}), props: plain(n.getAllProperties?.() || {}), children: (n.childNodes || []).map(node) }; }

record('key-value-builtins-and-oneof-real-callbacks', () => ({
  keys: Object.keys(KeyValueTypes),
  builtins: Object.fromEntries(Object.entries(KeyValueTypes).map(([k, d]) => [k, { name: d.name, verifyTrue: d.verify('true'), convertTrue: d.convert('true'), verifyNumber: d.verify('1.25'), convertNumber: d.convert('1.25') }])),
  oneof: (() => { const d = KeyValueDef.oneof('up', 'down'); return { name: d.name, yes: d.verify('up'), no: d.verify('left'), converted: d.convert('down') }; })(),
  descriptors: Object.getOwnPropertyDescriptors(KeyValueTypes),
}));
record('cols-numbers-and-empty', () => [ParseUtil.cols(), ParseUtil.cols(0, 1, -0.5, 2.25)]);
record('fenced-fixedfence-mathpalette-real-node-trees', () => {
  const x = jax(), c = x.parseOptions, p = parser(x);
  const m = p.create('node', 'mrow', [p.create('node', 'mi', [], {}, p.create('text', 'x'))]);
  return { fenced: node(ParseUtil.fenced(c, '(', m, ')', 'big', 'red')), fixed: node(ParseUtil.fixedFence(c, '[', m, ']')), palette: node(ParseUtil.mathPalette(c, '{', 'l')) };
});
record('fixinitialmo-real-arrays-holes-and-texatoms', () => {
  const c = jax().parseOptions, p = parser(), nf = c.nodeFactory;
  const mo = p.create('node', 'mo', [], {}, p.create('text', '+'));
  const space = p.create('node', 'mspace');
  const atom = p.create('node', 'TeXAtom', [p.create('node', 'mrow', [])], { texClass: 3 });
  const rows = [];
  for (const a of [[mo], [space, mo], [atom, mo], [null, mo], []]) { ParseUtil.fixInitialMO(c, a); rows.push(a.map(n => n?.kind ?? null)); }
  return rows;
});
record('fixinitialmo-captured-length-with-live-array-reads', () => {
  const c = jax().parseOptions, NodeUtil = load('input/tex/NodeUtil.js').default;
  const p = parser(), space = p.create('node', 'mspace'), mo = p.create('node', 'mo');
  const values = [space, mo], original = NodeUtil.isType;
  let changed = false;
  NodeUtil.isType = function (node, type) {
    if (!changed && node === space && type === 'mspace') { changed = true; values[1] = p.create('node', 'mi'); }
    return original.call(this, node, type);
  };
  try { ParseUtil.fixInitialMO(c, values); }
  finally { NodeUtil.isType = original; }
  return { changed, final: values.map(n => n.kind) };
});
record('internaltext-whitespace-and-definitions', () => {
  const p = parser(); return [' hello\nworld ', '', 'x  y'].map(s => node(ParseUtil.internalText(p, s, { mathvariant: 'bold' })));
});
record('internalmath-default-and-box-delimiters', () => {
  const p = parser(); return ['', 'plain text', 'a $x$ b', 'x\\(y\\)z', 'x\\ref{tag}', 'x\\eqref {tag}', 'a\\U{1F600}', '\\$x\\$', '\\{x\\}', '\\\\x', 'a { b }', 'bad $x', '\\mbox{ab}'].map(s => ({ text: s, result: error(() => ParseUtil.internalMath(p, s)), macroCount: p.macroCount }));
});
record('internalmath-level-font-and-live-callback', () => {
  const p = parser(); const original = p.configuration.options.internalMath;
  const calls = []; p.configuration.options.internalMath = function(...args) { calls.push({ receiver: this === p.configuration.options, args: args.map((x, i) => i === 0 ? x === p : plain(x)) }); return ['callback']; };
  const custom = error(() => ParseUtil.internalMath(p, ' a   b ', 0, 'italic'));
  p.configuration.options.internalMath = original;
  return { custom, calls, wrapped: ParseUtil.internalMath(p, 'plain', 0, 'bold').map(node) };
});
record('underover-movable-limits-and-stack-shape', () => {
  const p = parser(), base = p.create('node', 'mo', [], { movablelimits: true }, p.create('text', '∑'));
  const script = p.create('node', 'mi', [], {}, p.create('text', 'i'));
  return { ordinary: node(ParseUtil.underOver(p, base, script, 'over', false)), stacked: node(ParseUtil.underOver(p, base, script, 'under', true)), baseProperties: plain(base.getAllProperties()) };
});
record('checkmovablelimits-source-property-and-form', () => {
  const p = parser(); const nodes = [p.create('node', 'mi'), p.create('node', 'mo', [], {}, p.create('text', '∑'))];
  return nodes.map(n => { ParseUtil.checkMovableLimits(n); return { kind: n.kind, props: plain(n.getAllProperties()) }; });
});
record('setarrayalign-parser-and-no-parser-branches', () => {
  const p = parser(), a = p.itemFactory.create('array');
  const results = [];
  for (const align of ['t', 'b', 'c', 'lcr', '']) { const item = p.itemFactory.create('array'); results.push({ align, returnedSame: ParseUtil.setArrayAlign(item, align) === item, value: plain(item.arraydef.align) }); }
  p.string = 'rest'; p.i = 2; ParseUtil.setArrayAlign(a, 'c|c', p);
  return { results, parserString: p.string, parserIndex: p.i, parserArray: plain(a.arraydef) };
});
record('macro-substitute-addargs-limit-and-errors', () => {
  const p = parser();
  const outputs = ['#1', '##', '\\#1', '#0', '#2', 'x#'].map(s => ({ s, outcome: error(() => ParseUtil.substituteArgs(p, ['\\alpha', 'b'], s)) }));
  const a = ParseUtil.addArgs(p, '\\alpha', 'beta');
  const before = p.configuration.options.maxBuffer; p.configuration.options.maxBuffer = 2;
  const tooLong = error(() => ParseUtil.addArgs(p, 'ab', 'c')); p.configuration.options.maxBuffer = before;
  return { outputs, macroSpace: a, tooLong };
});
record('macrocount-error-ids-and-equation-nesting', () => {
  const p = parser(), outputs = [];
  p.configuration.options.maxMacros = 1;
  outputs.push(error(() => ParseUtil.checkMaxMacros(p)), error(() => ParseUtil.checkMaxMacros(p, false)), { count: p.macroCount });
  const top = p.stack.Top(); outputs.push(error(() => ParseUtil.checkEqnEnv(p)), { topKind: top.kind, first: plain(top.First) });
  return outputs;
});
record('copy-node-real-tree-and-node-lists', () => {
  const p = parser(), nf = p.configuration.nodeFactory;
  const n = p.create('node', 'mrow', [p.create('node', 'mi', [], {}, p.create('text', 'x'))]);
  n.setProperty('in-lists', 'foo,bar');
  const copy = ParseUtil.copyNode(n, p);
  return { original: node(n), copy: node(copy), same: copy === n, foo: p.configuration.getList('foo').map(node), mrow: p.configuration.getList('mrow').map(node) };
});
record('copy-node-addnode-order-and-falsey-in-lists', () => {
  const p = parser(), n = p.create('node', 'mrow', [p.create('node', 'mi')]);
  n.setProperty('in-lists', 'first,,last');
  n.childNodes[0].setProperty('in-lists', 0);
  const original = p.configuration.addNode, calls = [];
  p.configuration.addNode = function (kind, value) { calls.push({ receiver: this === p.configuration, kind, isNode: value === n || value === n.childNodes[0] }); return original.call(this, kind, value); };
  const copy = ParseUtil.copyNode(n, p);
  p.configuration.addNode = original;
  return { calls, copy: node(copy) };
});
record('mmlfilter-fontdef-and-getfontdef-liveenv', () => {
  const p = parser(); p.stack.env.font = 'script';
  return { filter: ParseUtil.mmlFilterAttribute(p, 'x', 'v'), font: plain(ParseUtil.getFontDef(p)), absent: (() => { delete p.stack.env.font; return plain(ParseUtil.getFontDef(p)); })() };
});
record('keyval-options-types-filter-errors-and-l3keys', () => {
  const allowed = { bool: KeyValueTypes.boolean, n: KeyValueTypes.number, i: KeyValueTypes.integer, s: KeyValueTypes.string, d: KeyValueTypes.dimen, dir: KeyValueDef.oneof('up', 'down') };
  const inputs = [
    ['bool=true,n=-1.5,i=12,s={hello, world},d=2em,dir=up,extra=x', allowed, false, false],
    ['bool=maybe', allowed, false, false], ['dir=left', allowed, false, false], ['extra=x', allowed, true, false],
    ['x={{ a,b }}', null, false, false], ['x={{ a,b }}', null, false, true], ['a={x},b=2', null, false, false],
    ['x=1}', null, false, false], ['x={1', null, false, false],
  ];
  return inputs.map(([s, a, e, l]) => ({ s, result: error(() => ParseUtil.keyvalOptions(s, a, e, l)) }));
});
record('keyval-options-current-getter-and-method-receiver', () => {
  const calls = [], type = new KeyValueDef('custom', function (v) { calls.push({ op: 'verify', receiver: this === type, value: v }); return true; }, function (v) { calls.push({ op: 'convert', receiver: this === type, value: v }); return `!${v}`; });
  const allowed = Object.defineProperty({}, 'x', { enumerable: true, get() { calls.push({ op: 'allowed-get' }); return type; } });
  return { result: plain(ParseUtil.keyvalOptions('x=7', allowed)), calls };
});
record('latin-greek-normalization-character-samples', () => ['A', 'α', 'Ω', 'é', '𝑥', '中', '', '😀'].map(s => [s, ParseUtil.isLatinOrGreekChar(s)]));

const output = { schema: 1, source: 'actual pinned MathJax CJS ParseUtil, KeyValueDef, TeX, ParseOptions, MML peers; no native code', sourceCommit: scope.upstream.commit, scope_sha256: scopeHash, modules, observations: rows };
const out = path.join(root, 'tools/math-port/profiles/evidence/parse-util-original-source.json');
const serialized = JSON.stringify(output, null, 2) + '\n';
if (process.argv.includes('--check')) assert.equal(fs.readFileSync(out, 'utf8').replaceAll('\r\n', '\n'), serialized);
else fs.writeFileSync(out, serialized);
console.log(JSON.stringify({ observations: rows.length, module_files: modules.length, output: path.relative(root, out) }));
