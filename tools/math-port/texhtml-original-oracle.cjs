'use strict';
// Actual MathJax 4.1.3 TexHtml observations. The adaptor is the pinned Lite
// adaptor used by the output path; each record preserves source parser fields.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const lock = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/locks/texhtml-xml-source-lock.json'), 'utf8'));
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const item of lock.sources) {
  const file = path.join(sourceRoot, item.file);
  assert.equal(digest(file), item.sha256, `${item.file} pinned source mismatch`);
  const installed = path.join(packageRoot, item.file);
  assert.equal(fs.readFileSync(installed, 'utf8').replaceAll('\r\n', '\n'),
    fs.readFileSync(file, 'utf8').replaceAll('\r\n', '\n'), `${item.file} installed TypeScript mismatch`);
}

const load = name => require(path.join(packageRoot, 'cjs', name));
const { TeX } = load('input/tex.js');
const TexParser = load('input/tex/TexParser.js').default;
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const { LiteAdaptor } = load('adaptors/liteAdaptor.js');
const { ConfigurationHandler } = load('input/tex/Configuration.js');
const { MapHandler } = load('input/tex/MapHandler.js');
for (const name of ['base/BaseConfiguration', 'setoptions/SetOptionsConfiguration',
  'html/HtmlConfiguration', 'texhtml/TexHtmlConfiguration']) {
  load(`input/tex/${name}.js`);
}

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
  if (value instanceof Map) return { $: 'map', entries: [...value].map(([key, item]) => [plain(key, seen), plain(item, seen)]) };
  const result = {};
  for (const key of Reflect.ownKeys(value)) {
    result[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] = plain(value[key], seen);
  }
  return result;
}
function mml(node) {
  if (node == null) return null;
  const result = {
    kind: node.kind,
    attrs: plain(node.attributes?.getAllAttributes?.() || {}),
    properties: plain(node.getAllProperties?.() || {}),
    children: (node.childNodes || []).map(mml),
  };
  if (typeof node.getText === 'function') result.text = node.getText();
  if (node.kind === 'html') {
    result.htmlKind = node.getHTML()?.kind;
    result.html = node.getSerializedHTML?.() ?? null;
    result.textContent = node.textContent?.() ?? null;
  }
  return result;
}
function run(name, tex, enabled = false) {
  const packages = enabled ? ['base', 'html', 'texhtml', 'setoptions'] : ['base', 'html', 'texhtml'];
  const jax = new TeX({ packages, allowTexHTML: enabled });
  const adaptor = new LiteAdaptor();
  jax.setMmlFactory(new MmlFactory());
  jax.parseOptions.packageData.set('texhtml', { adaptor });
  let parser = null, result = null, error = null;
  try {
    parser = new TexParser(tex, {}, jax.parseOptions);
    result = mml(parser.mml());
  } catch (caught) { error = { name: caught.name || caught.constructor.name,
    id: caught.id ?? null, message: String(caught.message) }; }
  return {
    name, tex, allowTexHTML: jax.options.allowTexHTML,
    parser: parser ? { i: parser.i, string: parser.string, macroCount: parser.macroCount,
      allowTexHTML: parser.options.allowTexHTML } : null,
    result, error,
    texhtmlPackageData: { adaptorClass: jax.parseOptions.packageData.get('texhtml')?.adaptor?.constructor?.name ?? null },
    configuration: (() => {
      const value = ConfigurationHandler.get('texhtml');
      return value ? { name: value.name, options: plain(value.options),
        handler: plain(value.handler), preprocessors: plain(value.preprocessors), postprocessors: plain(value.postprocessors) } : null;
    })(),
    macroMap: (() => {
      const map = MapHandler.getMap('tex-html');
      return map ? { name: map.name, constructor: map.constructor.name,
        parser: plain(map.parser), entries: plain(map.map) } : null;
    })(),
  };
}

const cases = [
  run('texhtml-default-disabled', '<tex-html><b>hello</b></tex-html>'),
  run('texhtml-profile-enabled', String.raw`\setOptions{allowTexHTML=true}<tex-html><b data-probe="x">hello &amp; world</b></tex-html>\setOptions{allowTexHTML=false}`, true),
  run('texhtml-multiple-roots', '<tex-html><b>bold</b><i>italic</i></tex-html>', true),
  run('texhtml-empty', '<tex-html>   </tex-html>', true),
  run('texhtml-nested-marked', '<tex-html n="2"><span>outer<tex-html><b>inner</b></tex-html></span><!2></tex-html>', true),
  run('texhtml-missing-end', '<tex-html><b>open</b>', true),
];
const loadedFiles = [];
for (const filename of Object.keys(require.cache)) {
  const relative = path.relative(path.join(packageRoot, 'cjs'), filename);
  if (relative.startsWith('..') || path.isAbsolute(relative)) continue;
  loadedFiles.push({ file: `cjs/${relative.split(path.sep).join('/')}`, sha256: digest(filename) });
}
const cjsLock = new Map(lock.cjsDependencies.map(item => [item.file, item.sha256]));
for (const item of loadedFiles) assert.equal(item.sha256, cjsLock.get(item.file), `${item.file} pinned CJS dependency mismatch`);
const output = {
  schema: 1,
  reference: 'MathJax 4.1.3',
  commit: lock.commit,
  scopeSha256: lock.scopeSha256,
  sourceHashes: lock.sources,
  loadedFiles: loadedFiles.sort((a, b) => a.file.localeCompare(b.file)),
  cases,
};
const destination = path.resolve(root, process.argv[2] || 'build/math-port/texhtml-original.json');
fs.mkdirSync(path.dirname(destination), { recursive: true });
fs.writeFileSync(destination, JSON.stringify(output, null, 2) + '\n');
console.log(JSON.stringify({ cases: cases.length, dependencies: loadedFiles.length,
  destination: path.relative(root, destination) }));
