'use strict';
// Actual MathJax 4.1.3 HTMLDomStrings options, CONFIG hook, and DOM traversal.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const lock = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/locks/texhtml-xml-source-lock.json'), 'utf8'));
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const item of lock.sources) {
  assert.equal(digest(path.join(sourceRoot, item.file)), item.sha256, `${item.file} pinned source mismatch`);
  assert.equal(fs.readFileSync(path.join(packageRoot, item.file), 'utf8').replaceAll('\r\n', '\n'),
    fs.readFileSync(path.join(sourceRoot, item.file), 'utf8').replaceAll('\r\n', '\n'), `${item.file} installed source mismatch`);
}
const load = name => require(path.join(packageRoot, 'cjs', name));
const { HTMLDomStrings } = load('handlers/html/HTMLDomStrings.js');
const { LiteAdaptor } = load('adaptors/liteAdaptor.js');
const { TeX } = load('input/tex.js');
for (const name of ['base/BaseConfiguration', 'html/HtmlConfiguration', 'texhtml/TexHtmlConfiguration'])
  load(`input/tex/${name}.js`);

function plain(value, seen = new WeakSet()) {
  if (value === undefined) return { $: 'undefined' };
  if (value === null || typeof value !== 'object') {
    if (typeof value === 'function') return { $: 'function', name: value.name, length: value.length };
    return value;
  }
  if (seen.has(value)) return { $: 'cycle' };
  seen.add(value);
  if (Array.isArray(value)) return value.map(item => plain(item, seen));
  const result = {};
  for (const key of Reflect.ownKeys(value)) result[typeof key === 'symbol' ? `symbol:${key.description}` : key] = plain(value[key], seen);
  return result;
}
function view(adaptor, node) {
  const kind = adaptor.kind(node);
  return { kind, outerHTML: kind === '#text' ? null : adaptor.outerHTML(node),
    value: kind === '#text' ? adaptor.value(node) : null,
    text: kind === '#text' ? adaptor.value(node) : adaptor.textContent(node) };
}
const defaultsBefore = plain(HTMLDomStrings.OPTIONS);
const custom = (node, adaptor) => `<custom>${adaptor.innerHTML(node)}</custom>`;
HTMLDomStrings.OPTIONS.includeHtmlTags['tex-html'] = custom;
new TeX({ packages: ['base', 'html', 'texhtml'] });
const replacement = {
  customPreserved: HTMLDomStrings.OPTIONS.includeHtmlTags['tex-html'] === custom,
  installed: plain(HTMLDomStrings.OPTIONS.includeHtmlTags['tex-html']),
};
delete HTMLDomStrings.OPTIONS.includeHtmlTags['tex-html'];
new TeX({ packages: ['base', 'html', 'texhtml'] });
const registered = HTMLDomStrings.OPTIONS.includeHtmlTags['tex-html'];
const adaptor = new LiteAdaptor();
const document = adaptor.parse('<div>before<br>middle<tex-html><b>inner</b><tex-html><i>deep</i></tex-html></tex-html>after</div>');
const body = adaptor.body(document);
const rootNode = adaptor.firstChild(body);
const instance = new HTMLDomStrings({});
instance.adaptor = adaptor;
const [strings, nodeLists] = instance.find(rootNode);
const rows = strings.map((string, i) => ({
  string,
  nodes: nodeLists[i].map(([node, length]) => ({ ...view(adaptor, node), length })),
}));
const output = {
  schema: 1,
  reference: 'MathJax 4.1.3',
  commit: lock.commit,
  scopeSha256: lock.scopeSha256,
  sourceHashes: lock.sources,
  defaultsBefore,
  configReplacement: replacement,
  registeredAfterConfig: plain(registered),
  instanceOptions: plain(instance.options),
  nestedMarkers: rows,
};
const loadedFiles = [];
for (const filename of Object.keys(require.cache)) {
  const relative = path.relative(path.join(packageRoot, 'cjs'), filename);
  if (relative.startsWith('..') || path.isAbsolute(relative)) continue;
  loadedFiles.push({ file: `cjs/${relative.split(path.sep).join('/')}`, sha256: digest(filename) });
}
const locked = new Map(lock.cjsDependencies.map(item => [item.file, item.sha256]));
for (const item of loadedFiles) assert.equal(item.sha256, locked.get(item.file), `${item.file} pinned dependency mismatch`);
output.loadedFiles = loadedFiles.sort((a, b) => a.file.localeCompare(b.file));
const destination = path.resolve(root, process.argv[2] || 'build/math-port/html-dom-strings-original.json');
fs.mkdirSync(path.dirname(destination), { recursive: true });
fs.writeFileSync(destination, JSON.stringify(output, null, 2) + '\n');
console.log(JSON.stringify({ cases: 2, loadedFiles: loadedFiles.length, destination: path.relative(root, destination) }));
