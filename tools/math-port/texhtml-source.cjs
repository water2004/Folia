'use strict';

// Independent Source witness for the pinned MathJax texhtml package.
// Run from C:\Windows with Node's heap bounded. This file is intentionally
// self-contained and does not use any ignored oracle, fixture, or native output.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const lockPath = path.join(root, 'docs/math-port/evidence/texhtml-xml-source-lock.json');
const lock = JSON.parse(fs.readFileSync(lockPath, 'utf8'));
const digest = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
assert.equal(digest(fs.readFileSync(lockPath)), '8b42afbe5fc15903cbaf12efe18c8b7b75f28a97265012fb763b8566b5aa8ab2', 'texhtml source lock changed');
for (const item of lock.sources) {
  const source = path.join(sourceRoot, item.file);
  assert.equal(digest(fs.readFileSync(source)), item.sha256, `${item.file}: pinned TS mismatch`);
  const installed = path.join(packageRoot, item.file);
  assert.equal(fs.readFileSync(installed, 'utf8').replaceAll('\r\n', '\n'),
    fs.readFileSync(source, 'utf8').replaceAll('\r\n', '\n'), `${item.file}: installed TS mismatch`);
}

const load = name => require(path.join(packageRoot, 'cjs', name));
const { TeX } = load('input/tex.js');
const TexParser = load('input/tex/TexParser.js').default;
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const { LiteAdaptor } = load('adaptors/liteAdaptor.js');
const { ConfigurationHandler } = load('input/tex/Configuration.js');
const { MapHandler } = load('input/tex/MapHandler.js');
const { NodeFactory } = load('input/tex/NodeFactory.js');
const { HTMLDomStrings } = load('handlers/html/HTMLDomStrings.js');
const { HTMLDocument } = load('handlers/html/HTMLDocument.js');
const { SerializedMmlVisitor } = load('core/MmlTree/SerializedMmlVisitor.js');
const { HtmlNodeMethods } = load('input/tex/texhtml/TexHtmlConfiguration.js');
for (const name of ['base/BaseConfiguration', 'setoptions/SetOptionsConfiguration',
  'html/HtmlConfiguration', 'texhtml/TexHtmlConfiguration']) load(`input/tex/${name}.js`);

function plain(value, seen = new WeakSet()) {
  if (value === undefined) return { $: 'undefined' };
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
  if (typeof value === 'number') return Number.isFinite(value) ? value : String(value);
  if (typeof value === 'function') return { $: 'function', name: value.name || '', length: value.length };
  if (typeof value !== 'object') return String(value);
  if (seen.has(value)) return { $: 'cycle' };
  seen.add(value);
  if (Array.isArray(value)) return Array.from({ length: value.length }, (_, i) =>
    Object.hasOwn(value, i) ? plain(value[i], seen) : { $: 'hole' });
  if (value instanceof Map) return { $: 'map', entries: [...value].map(([k, v]) => [plain(k, seen), plain(v, seen)]) };
  const out = {};
  for (const key of Reflect.ownKeys(value)) out[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] = plain(value[key], seen);
  return out;
}
function dom(node, adaptor) {
  if (!node) return null;
  const kind = adaptor.kind(node);
  if (kind === '#text') return { kind, text: adaptor.value(node) };
  if (kind === '#comment') return { kind, raw: node.value, text: adaptor.value(node) };
  if (!kind || kind === '#document') return { kind: kind || '#document', children: adaptor.childNodes(node).map(n => dom(n, adaptor)), textContent: adaptor.textContent(node) };
  const attrs = adaptor.allAttributes(node).map(a => [a.name, a.value]);
  return { kind, attrs, children: adaptor.childNodes(node).map(n => dom(n, adaptor)),
    outerHTML: adaptor.outerHTML(node), textContent: adaptor.textContent(node) };
}
function mml(node, adaptor) {
  if (!node) return null;
  const out = { kind: node.kind,
    attrs: plain(node.attributes?.getAllAttributes?.() || {}),
    properties: plain(node.getAllProperties?.() || {}),
    children: (node.childNodes || []).map(n => mml(n, adaptor)) };
  if (typeof node.getText === 'function') out.text = node.getText();
  if (node.kind === 'html') {
    const html = node.getHTML();
    out.html = { tree: dom(html, adaptor), serialized: node.getSerializedHTML(), textContent: node.textContent() };
  }
  return out;
}
function packageDataState(value) {
  if (!value) return value === undefined ? { $: 'undefined' } : value;
  const adaptor = value.adaptor;
  return { adaptor: adaptor ? { class: adaptor.constructor.name, present: true } : null };
}

// Original complete pinned testsuite inputs, plus boundary and failure cases.
const inputs = [
  ['original-html', 'x + <tex-html><b>bold</b></tex-html> + y', true],
  ['original-multiple', 'x + <tex-html><b>bold</b></tex-html> + <tex-html><i>italic</i></tex-html>y', true],
  ['original-nested', 'x + <tex-html><b>bold</b> and <tex-html>html</tex-html></tex-html> + y', true],
  ['original-empty', 'x + <tex-html></tex-html> + y', true],
  ['original-no-tag', 'x < y', true],
  ['original-missing-comment-error', 'x + <tex-html n="1"><b>bold</b></tex-html> + y', true],
  ['original-disabled', 'x + <tex-html><b>a</b></tex-html> + y', false],
  ['disabled-leading-candidate', '<tex-html><b>not active</b></tex-html>', false],
  ['enabled-leading', '<tex-html><b>yes</b></tex-html>', true],
  ['exact-tag-case', '<TEX-HTML><b>case remains ordinary</b></TEX-HTML>', true],
  ['tag-prefix', '<tex-html-extra><b>not exact</b></tex-html-extra>', true],
  ['trim-leading-trailing', '<tex-html> \n <b>trim</b> \t </tex-html>', true],
  ['multiple-roots', '<tex-html><b>A</b><!--c--><i>B</i>text</tex-html>', true],
  ['empty-whitespace', '<tex-html> \n\t </tex-html>', true],
  ['entity-attrs-text', '<tex-html><a title="A &amp; B" data-x="&quot;">one &amp; two</a></tex-html>', true],
  ['comments-and-entities', '<tex-html><!-- kept --><span title="&lt;">&amp;&#x1F600;</span></tex-html>', true],
  ['numbered-nested-marker', '<tex-html n="2"><span>outer<tex-html><b>inner</b></tex-html></span><!2></tex-html>', true],
  ['unmatched-numbered-marker', '<tex-html n="1"><b>open</b></tex-html>', true],
  ['numbered-marker-exact', '<tex-html n="1"><b>one</b><!1></tex-html>', true],
  ['missing-end', '<tex-html><b>open</b>', true],
  ['malformed-html-recovery', '<tex-html><b title="unterminated>broken</b></tex-html>', true],
  ['setoptions-toggle', String.raw`\setOptions{allowTexHTML=true}<tex-html><b>on</b></tex-html>\setOptions{allowTexHTML=false}<tex-html><i>off</i></tex-html>`, true],
];

function runDirect(name, tex, enabled, sharedJax = null) {
  const packages = enabled ? ['base', 'html', 'texhtml', 'setoptions'] : ['base', 'html', 'texhtml', 'setoptions'];
  const jax = sharedJax || new TeX({ packages, allowTexHTML: enabled });
  const adaptor = new LiteAdaptor();
  if (!sharedJax) jax.setMmlFactory(new MmlFactory());
  jax.parseOptions.packageData.set('texhtml', { adaptor });
  let parser = null, result = null, error = null;
  try {
    parser = new TexParser(tex, {}, jax.parseOptions);
    result = mml(parser.mml(), adaptor);
  } catch (e) { error = { name: e.name || e.constructor.name, id: e.id ?? null, message: String(e.message) }; }
  return { name, tex, allowTexHTML: jax.options.allowTexHTML,
    parser: parser ? { i: parser.i, string: parser.string, macroCount: parser.macroCount,
      allowTexHTML: parser.options.allowTexHTML } : null, result, error };
}

const cases = inputs.map(([name, tex, enabled]) => runDirect(name, tex, enabled));
// Error followed by a valid parse on the same configured TeX instance.
{
  const jax = new TeX({ packages: ['base', 'html', 'texhtml'], allowTexHTML: true });
  jax.setMmlFactory(new MmlFactory());
  const adaptor = new LiteAdaptor();
  jax.parseOptions.packageData.set('texhtml', { adaptor });
  const before = runDirect('same-jax-missing-marker', '<tex-html n="1"><b>x</b></tex-html>', true, jax);
  const after = runDirect('same-jax-after-error', '<tex-html><b>recovered</b></tex-html>', true, jax);
  cases.push({ name: 'same-jax-recovery', before, after, sameJax: true });
}

// Capture the real registration, parser callback, option, and lifecycle hooks.
const configuration = ConfigurationHandler.get('texhtml');
const charMap = MapHandler.getMap('tex-html');
const configObservation = {
  configuration: configuration ? { name: configuration.name, options: plain(configuration.options),
    handler: plain(configuration.handler), preprocessors: plain(configuration.preprocessors),
    postprocessors: plain(configuration.postprocessors) } : null,
  characterMap: charMap ? { name: charMap.name, constructor: charMap.constructor.name,
    entries: plain(charMap.map) } : null,
  methods: { TexHTML: plain(HtmlNodeMethods.TexHTML) },
  includeTexHtml: plain(HTMLDomStrings.OPTIONS.includeHtmlTags['tex-html']),
};

// Observe runtime creator requests and the package's DOM-adaptor data lifetime.
const creator = (() => {
  const jax = new TeX({ packages: ['base', 'html', 'texhtml'], allowTexHTML: true });
  jax.setMmlFactory(new MmlFactory());
  const adaptor = new LiteAdaptor();
  jax.parseOptions.packageData.set('texhtml', { adaptor });
  const requests = [];
  jax.parseOptions.nodeFactory.set('node', (factory, kind, children, def, text) => {
    requests.push({ kind, children: children?.map(n => n.kind) || [], definition: plain(def), text: text ?? null });
    return NodeFactory.createNode(factory, kind, children, def, text);
  });
  let parser = null, tree = null, error = null;
  try { parser = new TexParser('<tex-html><b data-x="1">text</b></tex-html>', {}, jax.parseOptions); tree = mml(parser.mml(), adaptor); }
  catch (e) { error = { name: e.name, id: e.id ?? null, message: String(e.message) }; }
  const dataAfterDirectParse = packageDataState(jax.parseOptions.packageData.get('texhtml'));
  return { requests, tree, error, dataAfterDirectParse, note: 'Direct TexParser does not execute HTMLDocument preprocess/postprocessors.' };
})();

// Exercise actual pre/post processing through one MathJax MathItem/document
// pipeline; record packageData while each configured processor runs.
const wrapTexFilters = (jax, observed, configuration) => {
  const texHtmlFilters = new Set([...configuration.preprocessors, ...configuration.postprocessors].map(entry => entry[0]));
  const wrap = (list, phase) => {
    const entries = [...list];
    for (const entry of entries) {
      const original = entry.item;
      const wrapped = function (...args) {
        const parseOptions = args[0].data;
        observed.push({ phase: `${phase}-before`, data: packageDataState(parseOptions.packageData.get('texhtml')) });
        const value = original.apply(this, args);
        observed.push({ phase: `${phase}-after`, data: packageDataState(parseOptions.packageData.get('texhtml')) });
        return value;
      };
      wrapped.original = original;
      entry.item = wrapped;
    }
    return entries.map(e => ({ priority: e.priority, texHtmlConfigurationFunction: texHtmlFilters.has(e.item.original) }));
  };
  return { pre: wrap(jax.preFilters, 'pre'), post: wrap(jax.postFilters, 'post') };
};

const snapshotPath = path.join(sourceRoot, 'testsuite/tests/input/tex/__snapshots__/TexHtml.test.ts.snap');
const snapshotText = fs.readFileSync(snapshotPath, 'utf8').replaceAll('\r\n', '\n');
const snapshotValue = key => {
  const marker = `exports[\`${key} 1\`] = \`\n`;
  const start = snapshotText.indexOf(marker);
  assert.notEqual(start, -1, `missing original snapshot: ${key}`);
  const end = snapshotText.indexOf('\n`;', start + marker.length);
  assert.notEqual(end, -1, `unterminated original snapshot: ${key}`);
  return snapshotText.slice(start + marker.length, end).trim().replace(/^"|"$/g, '');
};
const renderPipelineCase = (name, tex) => {
  const adaptor = new LiteAdaptor();
  const jax = new TeX({ packages: ['base', 'texhtml'], inlineMath: { '[+]': [['$', '$']] }, allowTexHTML: true,
    formatError(_jax, error) { throw error; } });
  const html = `<html><head></head><body>$${'$'}${tex}$${'$'}</body></html>`;
  const document = new HTMLDocument(adaptor.parse(html, 'text/html'), adaptor, { InputJax: jax });
  const events = [];
  const registeredFilters = wrapTexFilters(jax, events, ConfigurationHandler.get('texhtml'));
  document.findMath().compile();
  const item = Array.from(document.math)[0];
  const serialized = new SerializedMmlVisitor().visitTree(item.root);
  const snapKey = ({
    'original-html': 'Texhtml Html', 'original-multiple': 'Texhtml Html multiple',
    'original-nested': 'Texhtml Html nested', 'original-empty': 'Texhtml Html empty tag',
    'original-no-tag': 'Texhtml Html no tag',
  })[name];
  const expectedSnapshot = snapshotValue(snapKey);
  const observed = {
    name, sourceTest: 'render2mml via setupTexRender', inputTex: tex, inputHtml: html,
    mathItemsAfterFind: [...document.math].map(math => ({ math: math.math,
      start: { n: math.start.n, delim: math.start.delim }, end: { n: math.end.n, delim: math.end.delim } })),
    actualTex: item.math, mml: mml(item.root, adaptor), serializedMml: serialized,
    originalSnapshot: { key: snapKey, expected: expectedSnapshot, matches: serialized === expectedSnapshot },
    registeredFilters, filterEvents: events,
    packageDataAfterCompile: packageDataState(jax.parseOptions.packageData.get('texhtml')),
    parsedInputDom: dom(adaptor.body(document.document), adaptor),
  };
  return observed;
};
const originalPipelineCases = inputs.filter(([name]) => name.startsWith('original-') && !['original-missing-comment-error', 'original-disabled'].includes(name))
  .map(([name, tex]) => renderPipelineCase(name, tex));
assert.equal(originalPipelineCases.length, 5, 'all five original render2mml tests must be observed');
for (const item of originalPipelineCases) assert.equal(item.originalSnapshot.matches, true, `${item.name}: original snapshot mismatch`);

const lifecycle = (() => {
  const jax = new TeX({ packages: ['base', 'html', 'texhtml'], allowTexHTML: true });
  jax.setMmlFactory(new MmlFactory());
  const observed = [];
  const lifecycleAdaptor = new LiteAdaptor();
  const document = new HTMLDocument(lifecycleAdaptor.parse('<html><head></head><body>$$<tex-html><b>pipeline</b></tex-html>$$</body></html>', 'text/html'), lifecycleAdaptor, { InputJax: jax });
  const registeredFilters = wrapTexFilters(jax, observed, configuration);
  document.findMath().compile();
  return { registeredFilters, observed, result: mml([...document.math][0].root, lifecycleAdaptor),
    packageDataAfter: packageDataState(jax.parseOptions.packageData.get('texhtml')) };
})();

// Runtime HTMLDomStrings behavior including filter decisions, text/node maps,
// comments, text, included tags and state cleanup after find().
const domStrings = (() => {
  const adaptor = new LiteAdaptor();
  const source = adaptor.node('div', {}, [
    adaptor.text('A'), adaptor.node('br', {}, []), adaptor.text('B'), adaptor.node('wbr', {}, []),
    adaptor.node('script', {}, [adaptor.text('skip')]),
    adaptor.node('span', { class: 'mathjax_ignore' }, [adaptor.text('ignore'),
      adaptor.node('span', { class: 'mathjax_process' }, [adaptor.text('process')])]),
    adaptor.node('pre', {}, [adaptor.text('pre-skip')]),
    adaptor.node('code', {}, [adaptor.text('code-skip')]),
    adaptor.node('div', { 'data-MJX': '1' }, [adaptor.text('mjx-skip')]),
    adaptor.node('span', {}, [adaptor.text('tail'), adaptor.comment('<!--comment-->')]),
  ]);
  const strings = new HTMLDomStrings();
  strings.adaptor = adaptor;
  const found = strings.find(source);
  const texHtmlRoot = adaptor.node('div', {}, [adaptor.text('before'),
    adaptor.node('tex-html', {}, [adaptor.node('b', {}, [adaptor.text('outer'),
      adaptor.node('tex-html', {}, [adaptor.text('inner')])])]), adaptor.text('after')]);
  const texHtmlFound = strings.find(texHtmlRoot);
  return { source: dom(source, adaptor), strings: found[0], nodeLengths: found[1].map(list => list.map(([node, length]) => ({ kind: adaptor.kind(node), ...(adaptor.kind(node) === '#comment' ? { raw: node.value } : {}), value: ['#text', '#comment'].includes(adaptor.kind(node)) ? adaptor.value(node) : adaptor.outerHTML(node), length }))),
    configuredTexHtml: { strings: texHtmlFound[0], nodeLengths: texHtmlFound[1].map(list => list.map(([node, length]) => ({ kind: adaptor.kind(node), length, value: ['#text', '#comment'].includes(adaptor.kind(node)) ? adaptor.value(node) : adaptor.outerHTML(node) }))) },
    stateAfterFind: { strings: strings.strings, string: strings.string, snodes: strings.snodes, nodes: strings.nodes, stack: strings.stack } };
})();

const loadedFiles = [];
for (const filename of Object.keys(require.cache)) {
  const relative = path.relative(path.join(packageRoot, 'cjs'), filename);
  if (relative.startsWith('..') || path.isAbsolute(relative)) continue;
  loadedFiles.push({ file: `cjs/${relative.split(path.sep).join('/')}`, sha256: digest(fs.readFileSync(filename)) });
}
const cjsLock = new Map(lock.cjsDependencies.map(item => [item.file, item.sha256]));
const additionalCjsPins = new Map([
  ['cjs/handlers/html/HTMLDocument.js', '72b3352944ba435ccd6748644c48f7fcb129248c65bad895a5b1715726c89494'],
  ['cjs/core/MathDocument.js', '3a7dad24d15a1137fa0bd8394a42284c690830c751102b6a31785a0f77eee1db'],
  ['cjs/core/MathList.js', '0d5b387e640a0d52afb8aa3509122fb75f8641623301d7e8a63d39aa656933bd'],
  ['cjs/util/LinkedList.js', '18eb268740bbbc5ae1ff2b2a05fcd7756eb332769a706f2046c2a6e38a395f6e'],
  ['cjs/util/BitField.js', '013fb7859af70895667478f4f3fce8f5c9ff71ccfb9f814eb0860af12d9c95f9'],
  ['cjs/handlers/html/HTMLMathItem.js', '1f2d77971d95f910d71212cd15eb73169ca5e564b04c7aa077396b70ba502012'],
  ['cjs/handlers/html/HTMLMathList.js', 'b0279e39487a28f8ff5cc18fa45504dcbb75fb46cc3943edbf1032a2a56e0f9f'],
  ['cjs/core/MmlTree/SerializedMmlVisitor.js', '73e6bdcfc72f9dc67cfa77d444a586c53161a698c1e5f67d9a74aedfc0422f50'],
  ['cjs/core/MmlTree/MmlVisitor.js', 'cdf5f3649ba7e30f988ebd9ea812afacb9cf3556e89c524c2d69d53a5c15f230'],
]);
for (const item of loadedFiles) {
  const expected = cjsLock.get(item.file) || additionalCjsPins.get(item.file);
  assert.ok(expected, `${item.file}: loaded CJS file has no fixed hash`);
  assert.equal(item.sha256, expected, `${item.file}: CJS lock mismatch`);
  item.pin = cjsLock.has(item.file) ? 'docs/math-port/evidence/texhtml-xml-source-lock.json' : 'texhtml-source.cjs additionalCjsPins';
}
loadedFiles.sort((a, b) => a.file.localeCompare(b.file));
const originalTests = [
  ['testsuite/tests/input/tex/TexHtml.test.ts', 'f6c3f886f0c26ca61b25705cf2bec26582d4517e64c440bc5f21b0da284edd28'],
  ['testsuite/tests/input/tex/__snapshots__/TexHtml.test.ts.snap', '9cecbe29f17cc8d60808393b93e9718418add234c31179bb2ef84381921cfcd9'],
  ['testsuite/src/setupTex.ts', '618dac22a39594454ffd680f2327300d5e67eab4a83a758afe675ad4fba0a585'],
].map(([file, expected]) => {
  const actual = digest(fs.readFileSync(path.join(sourceRoot, file)));
  assert.equal(actual, expected, `${file}: original test pin mismatch`);
  return { file, sha256: actual };
});
const output = { schema: 1, reference: lock.reference, commit: lock.commit, scopeSha256: lock.scopeSha256,
  sourceHashes: lock.sources,
  originalTests, loadedFiles, cases, originalPipelineCases, configuration: configObservation, creator, lifecycle,
  domStrings, nativeCoverage: { scene: 'not evaluated', svg: 'not evaluated' } };
const bytes = Buffer.from(JSON.stringify(output, null, 2) + '\n');
const expectedSourceJsonSha256 = 'c13c8eb7ca5c9dea20e5a264a4b1437194949c01bd4f8c719dbf5a2959dfc4d5';
assert.equal(digest(bytes), expectedSourceJsonSha256, 'Pinned TexHtml Source observations changed');
const destination = path.join(root, 'build/math-port/texhtml-source.json');
const fixturePath = path.join(root, 'tests/math/fixtures/packages/texhtml');

const {utf16Field, table, writeTables} = require('./fixture-data.cjs');
assert.equal(utf16Field('\\0A'), '\\u005c0A');
assert.equal(utf16Field('\0A'), '\\u0000A');
assert.equal(utf16Field('\ud800A'), '\\ud800A');

const lengthPart = value => `${String(value).length}:${value}`;
const sourceDomSnapshot = node => {
  let value = lengthPart(node.kind);
  if (node.kind === '#text') return value + lengthPart(node.text);
  if (node.kind === '#comment') return value + lengthPart(node.raw) + lengthPart(node.text);
  if (node.kind === '#document') {
    value += lengthPart(node.children.length);
    for (const child of node.children) value += sourceDomSnapshot(child);
    return value + lengthPart(node.textContent);
  }
  value += lengthPart(node.attrs.length);
  for (const [name, attribute] of node.attrs) value += lengthPart(name) + lengthPart(attribute);
  value += lengthPart(node.children.length);
  for (const child of node.children) value += sourceDomSnapshot(child);
  return value + lengthPart(node.outerHTML) + lengthPart(node.textContent);
};
const sourceMmlSnapshot = node => {
  if (!node) return lengthPart('null');
  let value = lengthPart(node.kind);
  const attributes = Object.entries(node.attrs || {}).sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0);
  value += lengthPart(attributes.length);
  for (const [name, attribute] of attributes) {
    value += lengthPart(name);
    if (attribute === null) value += lengthPart('null') + lengthPart('');
    else value += lengthPart(typeof attribute) + lengthPart(attribute);
  }
  const props = node.properties || {};
  const propertyNames = ['texClass', 'unicode', 'autoOP', 'fnOP', 'fixStretchy',
    'ignore-variant', 'noDictDef', 'texprimestyle', 'in-lists', 'sub-sup', 'open',
    'close', 'mathaccent', 'movesupsub', 'subsupOK', 'variantForm'];
  const selected = propertyNames.filter(name => Object.hasOwn(props, name));
  value += lengthPart(selected.length);
  for (const name of selected) {
    let property = props[name];
    if (name === 'in-lists' && Array.isArray(property)) property = property.join(',');
    value += lengthPart(name) + lengthPart(typeof property) + lengthPart(property);
  }
  value += lengthPart(node.text || '');
  if (node.html) {
    value += lengthPart('html') + sourceDomSnapshot(node.html.tree) +
      lengthPart(node.html.serialized) + lengthPart(node.html.textContent);
  } else value += lengthPart('no-html');
  value += lengthPart(node.children.length);
  for (const child of node.children) value += sourceMmlSnapshot(child);
  return value;
};
const definitions = {
  "direct": {columns: ["name_utf16", "input_utf16", "sourceSnapshot_utf16", "errorName_utf16", "errorId_utf16", "errorMessage_utf16", "initialAllowTexHTML_boolean"],
    rows: cases.filter(item => typeof item.tex === 'string').map(item => {
      const error = item.error || {};
      const allow = inputs.find(([name]) => name === item.name)?.[2];
      assert.equal(typeof allow, 'boolean', `missing input option for ${item.name}`);
      return [...[item.name, item.tex, item.result ? sourceMmlSnapshot(item.result) : '',
        error.name || '', error.id || '', error.message || ''].map(utf16Field), +allow];
    })},
  "pipeline": {columns: ["name_utf16", "input_utf16", "actualTex_utf16", "serializedMml_utf16", "sourceSnapshot_utf16"],
    rows: originalPipelineCases.map(item => [item.name, item.inputTex, item.actualTex,
      item.serializedMml, sourceMmlSnapshot(item.mml)].map(utf16Field))},
  "recovery": {columns: ["input_utf16", "errorId_utf16", "errorMessage_utf16", "recoveredInput_utf16", "recoveredSnapshot_utf16"],
    rows: [[cases.at(-1).before.tex, cases.at(-1).before.error.id, cases.at(-1).before.error.message,
      cases.at(-1).after.tex, sourceMmlSnapshot(cases.at(-1).after.result)].map(utf16Field)]},
  'dom-strings': {columns: ['value_utf16'], rows: domStrings.strings.map(value => [utf16Field(value)])},
  metadata: {columns: ['source_json_sha256'], rows: [[expectedSourceJsonSha256]]}
};
const writeRequested = process.argv.includes('--write-fixture');
const checkRequested = process.argv.includes('--check');
assert.ok(!(writeRequested && checkRequested), 'choose --write-fixture or bytecheck');
if (writeRequested) {
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  const unchanged = fs.existsSync(destination) && fs.readFileSync(destination).equals(bytes);
  if (!unchanged) fs.writeFileSync(destination, bytes);
  const fixtureUnchanged = Object.entries(definitions).every(([name, def]) => {
    const file = path.join(fixturePath, `${name}.tsv`);
    return fs.existsSync(file) && fs.readFileSync(file, 'utf8') === table(def.columns, def.rows);
  });
  writeTables(fixturePath, definitions, true);
  console.log(JSON.stringify({ mode: unchanged && fixtureUnchanged ? 'write-fixture-unchanged' : 'write-fixture', cases: cases.length,
    pipelineCases: originalPipelineCases.length, dependencies: loadedFiles.length,
    bytes: bytes.length, sha256: digest(bytes), fixture: path.relative(root, fixturePath).replaceAll('\\', '/') }));
} else {
  assert.ok(fs.existsSync(destination), 'candidate fixture missing; run --write-fixture');
  assert.deepEqual(fs.readFileSync(destination), bytes, 'Source bytecheck mismatch');
  writeTables(fixturePath, definitions, false);
  console.log(JSON.stringify({ mode: 'bytecheck', cases: cases.length, pipelineCases: originalPipelineCases.length, dependencies: loadedFiles.length,
    bytes: bytes.length, sha256: digest(bytes), fixture: path.relative(root, fixturePath).replaceAll('\\', '/') }));
}
