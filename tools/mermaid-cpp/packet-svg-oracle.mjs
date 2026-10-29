// Independent Packet renderer observations. This executes the pinned npm
// Mermaid renderer and records its real DOM and DB; it does not import C++ or
// reproduce Packet layout/geometry formulas to manufacture expected results.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const sourceRoot = path.join(root, '.reference/mermaid-js');
const require = createRequire(path.join(root, 'tools/mermaid-cpp/oracle/package.json'));
const lock = JSON.parse(readFileSync(path.join(root, 'docs/mermaid-cpp/source-lock.json'), 'utf8'));
assert.equal(lock.commit, '1fb7869866dd619168d840291c9c54b4f129758b');
const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
for (const entry of lock.files) {
  assert.equal(sha256(readFileSync(path.join(sourceRoot, entry.path))), entry.sha256,
    `Changed official source: ${entry.path}`);
}
const sizeSource = 'packages/mermaid/src/setupGraphViewbox.js';
assert.equal(sha256(readFileSync(path.join(sourceRoot, sizeSource))),
  'cebb629053c67dac36635cc31262d1c5e8cc7ff26a645ca080474a649326327b', sizeSource);

const packagePath = require.resolve('mermaid/package.json');
assert.equal(JSON.parse(readFileSync(packagePath, 'utf8')).version, '12.2.0');
assert.equal(JSON.parse(readFileSync(require.resolve('jsdom/package.json'), 'utf8')).version, '26.1.0');
// The pinned published chunk's source map is an additional provenance check:
// the renderer/parser/DB being executed were built from the reviewed source.
const map = JSON.parse(readFileSync(path.join(path.dirname(packagePath),
  'dist/chunks/mermaid.core/diagram-I5XIIV2D.mjs.map'), 'utf8'));
for (const file of ['db.ts', 'parser.ts', 'renderer.ts', 'styles.ts', 'diagram.ts']) {
  const index = map.sources.findIndex(source => source.endsWith(`/diagrams/packet/${file}`));
  assert.notEqual(index, -1, `Published Packet source map lacks ${file}`);
  const checkedIn = readFileSync(path.join(sourceRoot, `packages/mermaid/src/diagrams/packet/${file}`), 'utf8');
  assert.equal(map.sourcesContent[index].replaceAll('\r\n', '\n'), checkedIn.replaceAll('\r\n', '\n'),
    `Published Packet ${file} differs from the pinned source`);
}

const { JSDOM } = await import(pathToFileURL(require.resolve('jsdom')));
const dom = new JSDOM('<!doctype html><html><body></body></html>', { url: 'https://example.invalid/' });
globalThis.window = dom.window;
globalThis.document = dom.window.document;
const { default: mermaid } = await import(pathToFileURL(require.resolve('mermaid')));
const { diagram: packetDefinition } = await import(pathToFileURL(path.join(path.dirname(packagePath),
  'dist/chunks/mermaid.core/diagram-I5XIIV2D.mjs')));
const { parse: parseAst } = await import(pathToFileURL(path.join(root,
  'tools/mermaid-cpp/oracle/node_modules/@mermaid-js/parser/dist/mermaid-parser.core.mjs')));
// The default style has no whitespace-bearing values. Compare its complete
// official rules separately from the geometry DOM, allowing compact output.
const defaultStyle = packetDefinition.styles().replace(/\s+/g, '');

const defaults = { rowHeight: 32, paddingX: 5, paddingY: 5, bitWidth: 32,
  bitsPerRow: 32, showBits: true, useMaxWidth: true, bitOrder: 'ascending' };
const cases = [];
const add = (name, source, config = {}, overrides = {}) => cases.push({
  name, source, config: { ...defaults, ...config }, ...overrides,
  phase: Object.keys(overrides).length ? 'renderer-data' : 'raw-parser-renderer',
});
const register = 'packet\n0-7: "DATA"\n8-11: "TYPE"\n12: "EN"\n13-15: "RESERVED"';
add('empty', 'packet');
add('empty-beta', 'packet-beta');
add('empty-title', 'packet\ntitle Packet title');
add('empty-hidden-bits', 'packet', { showBits: false });
add('empty-zero-height', 'packet', { showBits: false, paddingY: 0 });
add('single-bit', 'packet\n0: "one"');
add('single-byte', 'packet\n+8: "byte"');
add('register', register);
add('cross-row', 'packet\n0-10: "first"\n11-90: "spanning"');
add('exact-cut', 'packet\n0-16: "first"\n17-63: "spanning"');
add('partial-last-row', 'packet\n+64: "full"\n+3: "tail"');
add('one-bit-rows', 'packet\n+3: "three"', { bitsPerRow: 1, paddingX: 0 });
add('two-bit-rows', 'packet\n+3: "three"', { bitsPerRow: 2, paddingX: 0 });
add('seven-bit-rows', 'packet\n+20: "twenty"', { bitsPerRow: 7 });
add('eight-bit-rows', 'packet\n+8: "first"\n+8: "second"', { bitsPerRow: 8 });
add('sixteen-bit-rows', 'packet\n0-31: "word"', { bitsPerRow: 16 });
add('sixtyfour-bit-rows', 'packet\n0-79: "wide"', { bitsPerRow: 64 });
add('descending-register', register, { bitOrder: 'descending', bitsPerRow: 16 });
add('descending-partial', 'packet\n+3: "left"\n+1: "single"', { bitOrder: 'descending' });
add('descending-split', 'packet\n0-10: "first"\n11-90: "spanning"', { bitOrder: 'descending' });
add('hidden-bits', register, { showBits: false });
add('absolute-size', register, { useMaxWidth: false });
add('absolute-hidden-bits', register, { showBits: false, useMaxWidth: false });
add('fractional', 'packet\n+3: "first"\n+7: "second"',
  { rowHeight: 32.25, paddingX: 2.5, paddingY: 1.125, bitWidth: 3.75, bitsPerRow: 8 });
add('fractional-descending', 'packet\n+3: "first"\n+7: "second"',
  { rowHeight: 11.3, paddingX: 0.3, paddingY: 0.2, bitWidth: 1.1, bitsPerRow: 8, bitOrder: 'descending' });
add('no-padding', register, { paddingX: 0, paddingY: 0, showBits: false });
add('zero-field-width', 'packet\n+1: "zero"', { bitsPerRow: 1, bitWidth: 1, paddingX: 1 });
add('unsafe-integer-width', 'packet\n+1: "large"',
  { bitsPerRow: 1, bitWidth: 1000000000000000128, paddingX: 0 });
add('unsafe-integer-height', 'packet\n+1: "large"',
  { bitsPerRow: 1, rowHeight: 1000000000000000128, paddingY: 0, showBits: false });
add('fixed-large-number', 'packet\n+1: "large"', { bitsPerRow: 1, bitWidth: 1e20, paddingX: 0 });
add('exponential-large-number', 'packet\n+1: "large"', { bitsPerRow: 1, bitWidth: 1e21, paddingX: 0 });
add('near-zero-width', 'packet\n+1: "thin"', { paddingX: 32 - 1e-8 });
add('unicode', 'packet\ntitle 网络报文\n+8: "类型"\n+8: "长度"');
add('emoji', 'packet\ntitle Status 😀\n+8: "🚀"\n+8: "🧭"');
add('long-label', `packet\n+1: "${'wide label '.repeat(20)}"`);
add('literal-multiline-label', 'packet\n+8: "first\nsecond"');
add('escaped-newline-label', 'packet\n+8: "first\\nsecond"');
add('repeated-titles', 'packet\ntitle First\ntitle Final\n+8: "body"');
add('accessibility-does-not-alter-family-elements',
  'packet\naccTitle: Packet accessibility\naccDescr { First line\n Second line }\n+8: "body"');

// These fixtures deliberately inject text into the real DB after parsing.
// They test the renderer's text stage, NOT MermaidAPI preprocessing/sanitizing.
const textCases = [
  ['xml-text', '<&> "single\' double"'],
  ['markup-is-text', '</text><script>alert(1)</script><text>'],
  ['literal-entities', '&amp; &#13; &lt;'],
  ['tabs-and-newlines', '\tfirst\nsecond\t'],
  ['carriage-returns', 'a\rb\r\nc'],
  ['nonbreaking-spaces', 'a\u00a0b\u202fc'],
  ['combining', 'e\u0301 A\u030a'],
  ['rtl-text', 'مرحبا עברית'],
  ['supplementary', '\u{10437} \u{1f600} \u{10ffff}'],
  ['line-separators', 'a\u2028b\u2029c'],
  ['replacement-character', 'a\ufffdb'],
  ['empty-label', ''],
];
for (const [name, label] of textCases) {
  add(name, 'packet\n+40: "original"', { bitsPerRow: 16 }, { label });
}
add('xml-title', 'packet\n+1: "body"', {}, { title: '<&> "title"' });
add('title-carriage-return', 'packet\n+1: "body"', {}, { title: 'first\rsecond' });
add('empty-title-override', 'packet\ntitle Original\n+1: "body"', {}, { title: '' });
add('all-text-overrides', register, { bitOrder: 'descending', showBits: false, useMaxWidth: false },
  { label: '⛰ & <field>', title: '😀 <title>' });

const hex16 = text => Array.from({ length: text.length }, (_, i) =>
  text.charCodeAt(i).toString(16).padStart(4, '0')).join('') || '-';
const wordsText = words => words.map(word => word.map(block =>
  [block.start, block.end, block.bits, hex16(block.label)].join(',')).join(';')).join('/') || '-';
const configText = config => [config.rowHeight, config.paddingX, config.paddingY, config.bitWidth,
  config.bitsPerRow, Number(config.showBits), Number(config.useMaxWidth),
  Number(config.bitOrder === 'descending')].join(',');

const lines = [];
for (const entry of cases) {
  mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', packet: entry.config });
  document.body.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" id="packet-oracle"></svg>';
  // Acquire the actual registered family, then feed its parser raw input. This
  // avoids accidentally accepting outer preprocessing as C++ parser evidence.
  const diagram = await mermaid.mermaidAPI.getDiagramFromText('packet');
  diagram.db.clear();
  diagram.parser.parser.yy = diagram.db;
  await diagram.parser.parse(entry.source);
  const ast = await parseAst('packet', entry.source);
  assert.equal(diagram.db.getDiagramTitle(), ast.title ?? '', `${entry.name}: title sanitization boundary`);
  const rawLabels = new Set(ast.blocks.map(block => block.label));
  assert(diagram.db.getPacket().flat().every(block => rawLabels.has(block.label)),
    `${entry.name}: unexpected raw-parser label transformation`);
  if ('label' in entry) {
    for (const word of diagram.db.getPacket()) for (const block of word) block.label = entry.label;
  }
  if ('title' in entry) diagram.db.getDiagramTitle = () => entry.title;
  const words = wordsText(diagram.db.getPacket());
  await diagram.renderer.draw(entry.source, 'packet-oracle', '12.2.0', diagram);
  const svg = document.querySelector('svg');
  svg.removeAttribute('id');
  const [, , width, height] = svg.getAttribute('viewBox').split(' ').map(Number);
  // HTML DOM serialization spells NBSP as &nbsp;, which is not a built-in XML
  // entity. Preserve the same Unicode text; likewise preserve CR through XML.
  const output = svg.outerHTML.replaceAll('&nbsp;', '\u00a0').replaceAll('\r', '&#13;');
  lines.push([entry.name, entry.phase, hex16(entry.source), configText(entry.config),
    'label' in entry ? hex16(entry.label) : '~', 'title' in entry ? hex16(entry.title) : '~',
    width, height, words, Buffer.from(output, 'utf8').toString('hex')].join('\t'));
}
dom.window.close();

const expected = '# Official Mermaid 12.2.0 Packet DB and real jsdom SVG observations\n' +
  '# default-style-utf8-hex\t' + Buffer.from(defaultStyle, 'utf8').toString('hex') + '\n' +
  '# name phase source-utf16 config label-override title-override width height words svg-utf8-hex\n' +
  lines.join('\n') + '\n';
const destination = path.join(root, 'tests/mermaid_cpp/fixtures/packet-svg.tsv');
assert(!(process.argv.includes('--write') && process.argv.includes('--check')), 'Choose one mode');
if (process.argv.includes('--write')) writeFileSync(destination, expected);
else assert.equal(readFileSync(destination, 'utf8').replaceAll('\r\n', '\n'), expected,
  'Packet SVG source observations changed; review source and regenerate explicitly.');
console.log(JSON.stringify({ source: lock.commit, package: 'mermaid@12.2.0', cases: cases.length,
  rendererDataCases: cases.filter(entry => entry.phase === 'renderer-data').length,
  mode: process.argv.includes('--write') ? 'write' : 'check' }));
