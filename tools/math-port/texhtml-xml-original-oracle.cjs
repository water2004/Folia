'use strict';
// Actual CommonXmlNode and SvgXmlNode behavior over MathJax's canonical MML,
// SVG wrapper factory, and LiteAdaptor. This records no synthetic geometry.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const srcRoot = path.join(root, '.reference/MathJax-src');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const lock = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/locks/texhtml-xml-source-lock.json'), 'utf8'));
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const item of lock.sources.slice(1)) {
  const original = path.join(srcRoot, item.file);
  assert.equal(digest(original), item.sha256, `${item.file} pinned source mismatch`);
  assert.equal(fs.readFileSync(path.join(packageRoot, item.file), 'utf8').replaceAll('\r\n', '\n'),
    fs.readFileSync(original, 'utf8').replaceAll('\r\n', '\n'), `${item.file} installed source mismatch`);
}
const load = name => require(path.join(packageRoot, 'cjs', name));
const { SvgWrappers } = load('output/svg/Wrappers.js');
const { SvgWrapperFactory } = load('output/svg/WrapperFactory.js');
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const { LiteAdaptor } = load('adaptors/liteAdaptor.js');
const adaptor = new LiteAdaptor();
const mml = new MmlFactory();
const factory = new SvgWrapperFactory(SvgWrappers);
const start = adaptor.node('div');
const container = adaptor.node('div');
const jax = {
  font: { params: { rule_thickness: .06 }, getVariant: () => null, getChar: () => null },
  adaptor, options: { htmlHDW: 'use', scale: 1 },
  math: { metrics: { em: 16, scale: 2, family: 'SourceFont' }, start: { node: start } },
  pxPerEm: 16, nodeMap: new Map(), fixed: (number, digits = 3) => Number(number).toFixed(digits),
  container, html: (kind, definition = {}, children = []) => adaptor.node(kind, definition, children),
  svg: (kind, definition = {}, children = []) => adaptor.node(kind,
    { xmlns: 'http://www.w3.org/2000/svg', ...definition }, children),
};
factory.jax = jax;
const parentNode = mml.create('math');
const parent = new SvgWrappers.math(factory, parentNode, null);
parent.variant = 'normal';
parent.styles = { get: name => name === 'font-family' ? 'ParentFont' : null };

function failure(fn) {
  try { return { value: fn(), error: null }; }
  catch (error) { return { value: null, error: { name: error.name || error.constructor.name,
    message: String(error.message), id: error.id ?? null } }; }
}
function observe(name, definition, option = 'use') {
  jax.options.htmlHDW = option;
  const xml = adaptor.node('section', definition, [
    adaptor.node('b', { class: 'probe', style: 'font-weight: bold' }, [adaptor.text('xml & html')]),
  ]);
  const node = mml.create('XML').setXML(xml, adaptor);
  const wrapper = new SvgWrappers.XML(factory, node, parent);
  const bbox = failure(() => {
    const result = wrapper.getBBox();
    return { w: result.w, h: result.h, d: result.d, scale: result.scale, rscale: result.rscale };
  });
  const styles = failure(() => wrapper.getFontStyles());
  const html = failure(() => {
    const result = wrapper.getHTML();
    return { kind: adaptor.kind(result), serialized: adaptor.outerHTML(result),
      childKind: adaptor.kind(adaptor.firstChild(result)),
      childSerialized: adaptor.outerHTML(adaptor.firstChild(result)) };
  });
  const svgRoot = adaptor.node('svg', { xmlns: 'http://www.w3.org/2000/svg' });
  const placed = failure(() => {
    wrapper.toSVG([svgRoot]);
    return { svg: adaptor.outerHTML(svgRoot), children: adaptor.childNodes(svgRoot).map(node => adaptor.kind(node)) };
  });
  return {
    name, option, definition,
    xml: { kind: adaptor.kind(xml), serialized: adaptor.outerHTML(xml), textContent: adaptor.textContent(xml) },
    bbox, styles, html, svg: placed,
    measurementNodesAfterCalls: { containerChildren: adaptor.childNodes(container).map(node => adaptor.kind(node)),
      containerParent: adaptor.parent(container) ? adaptor.kind(adaptor.parent(container)) : null },
  };
}

const cases = [
  observe('xml-hdw-use', { 'data-mjx-hdw': '1em .25em 2em' }, 'use'),
  observe('xml-hdw-force', { 'data-mjx-hdw': '1.5em .5em 3em' }, 'force'),
  observe('xml-hdw-disabled', { 'data-mjx-hdw': '1em .25em 2em' }, 'ignore'),
  observe('xml-measured-lite-dom', {}, 'use'),
];
const loadedFiles = [];
for (const filename of Object.keys(require.cache)) {
  const relative = path.relative(path.join(packageRoot, 'cjs'), filename);
  if (relative.startsWith('..') || path.isAbsolute(relative)) continue;
  loadedFiles.push({ file: `cjs/${relative.split(path.sep).join('/')}`, sha256: digest(filename) });
}
const cjsLock = new Map(lock.cjsDependencies.map(item => [item.file, item.sha256]));
for (const item of loadedFiles) assert.equal(item.sha256, cjsLock.get(item.file), `${item.file} pinned CJS dependency mismatch`);
const output = { schema: 1, reference: 'MathJax 4.1.3', commit: lock.commit,
  scopeSha256: lock.scopeSha256, sourceHashes: lock.sources, loadedFiles: loadedFiles.sort((a, b) => a.file.localeCompare(b.file)), cases };
const destination = path.resolve(root, process.argv[2] || 'build/math-port/texhtml-xml-original.json');
fs.mkdirSync(path.dirname(destination), { recursive: true });
fs.writeFileSync(destination, JSON.stringify(output, null, 2) + '\n');
console.log(JSON.stringify({ cases: cases.length, dependencies: loadedFiles.length, destination: path.relative(root, destination) }));
