// The pinned @zenuml/core SVG renderer is independent of a DOM. Its real
// OffscreenCanvas path is supplied by Skia; the package's character-count
// fallback is forbidden. These records are not browser acceptance results.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {createRequire} from 'node:module';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {loadCorpus, repository, hash} from './corpus.mjs';

const requireOracle = createRequire(new URL('../oracle/package.json', import.meta.url));
const {createCanvas, GlobalFonts} = requireOracle('@napi-rs/canvas');
const canvasVersion = requireOracle('@napi-rs/canvas/package.json').version;
assert.equal(canvasVersion, '1.1.1', 'Use the locked real-font backend');
const argument = name => process.argv.find(value => value.startsWith(`--${name}=`))?.slice(name.length + 3);
const output = resolve(argument('output') ?? 'build/mermaid-regression/zenuml-canvas');
const source = resolve(process.env.FOLIA_ZENUML_SOURCE ?? 'build/mermaid-cpp/zenuml-source');
mkdirSync(output, {recursive: true});
const integrity = 'qeQMapuE0XETML2mRw2MIIrMCjcZBhRDSwW10//q8XQ+VyExzkAn39VCBwxIbbPdTBt6gJnaBBixF9A4hsIxcA==';
assert.equal(createHash('sha512').update(readFileSync(resolve(source, 'core-3.49.2.tgz'))).digest('base64'), integrity);

let canvasCreations = 0;
const fontMeasurements = new Map();
globalThis.OffscreenCanvas = class {
  constructor(width, height) {
    this.canvas = createCanvas(width, height);
    ++canvasCreations;
  }
  getContext(kind) {
    assert.equal(kind, '2d');
    const context = this.canvas.getContext(kind);
    assert(context, 'A real Canvas context is mandatory; never use estimated widths');
    return new Proxy(context, {
      get(target, key) {
        if (key === 'measureText') return text => {
          const metrics = target.measureText(text);
          assert(Number.isFinite(metrics.width) && metrics.width >= 0);
          fontMeasurements.set(JSON.stringify([target.font, text]), metrics.width);
          return metrics;
        };
        const value = Reflect.get(target, key, target);
        return typeof value === 'function' ? value.bind(target) : value;
      },
      set(target, key, value) { return Reflect.set(target, key, value, target); }
    });
  }
};
// Unrelated UI preferences are read during package initialization only.
globalThis.location = {href: 'file://folia-zenuml-canvas', search: ''};
globalThis.localStorage = {getItem: () => null};
const bundle = readFileSync(resolve(source, 'package/dist/zenuml.esm.mjs'), 'utf8');
assert(bundle.includes('new D8(n, b2), a = new Z8(n)'));
assert(bundle.includes('function XI(e, t) {\n  const n = R8(e);'));
assert(bundle.includes('function O8() {'));
const instrumented = resolve(source, 'package/dist/folia-canvas-oracle.mjs');
writeFileSync(instrumented, bundle + `
export function canvasOracleModel(code) {
  const root = R8(code);
  if (!root || root.parser._syntaxErrors) throw new Error('Invalid official ZenUML input');
  const horizontal = new D8(root, b2), vertical = new Z8(root);
  if (!O8()) throw new Error('Canvas unavailable: character-count fallback forbidden');
  return {
    measurements: [...new Set([...horizontal.participantModels.map(p => p.label || p.name),
      ...horizontal.ownableMessages.map(m => m.signature)])].map(text => [text.trim(), b2(text, 0)]),
    title: root.title?.()?.content?.() ?? '', participants: horizontal.participantModels,
    messages: horizontal.ownableMessages, constraints: horizontal.m,
    positions: horizontal.participantModels.map(p => ({name: p.name, x: horizontal.getPosition(p.name)})),
    vertical: vertical.entries(), height: vertical.getTotalHeight()
  };
}
`);
const {renderToSvg, canvasOracleModel} = await import(pathToFileURL(instrumented).href);
const records = loadCorpus().records.filter(item => item.family === 'zenuml');
const result = [];
for (const item of records) {
  assert.equal(Object.keys(item.config ?? {}).length, 0,
    `${item.id}: the standalone ZenUML renderer has no Mermaid configuration adapter`);
  const code = item.source.replace(/^\s*zenuml/, '');
  try {
    const model = canvasOracleModel(code), rendered = renderToSvg(code);
    assert(rendered.geometry && rendered.width > 0 && rendered.height > 0);
    writeFileSync(resolve(output, `${item.id}.svg`), rendered.svg);
    writeFileSync(resolve(output, `${item.id}.geometry.json`), JSON.stringify(rendered.geometry, null, 2) + '\n');
    result.push({id: item.id, source: item.source, model, svg: `${item.id}.svg`,
      sourceSha256: hash(item.source), svgSha256: hash(rendered.svg),
      width: rendered.width, height: rendered.height});
  } catch (error) { throw new Error(`${item.id}: ${error.message}`, {cause: error}); }
}
assert(canvasCreations > 0 && fontMeasurements.size > 0, 'No real text shaping occurred');
const fonts = Array.from(fontMeasurements, ([key, width]) => ({font: JSON.parse(key)[0], text: JSON.parse(key)[1], width}));
const report = {
  upstream: {package: '@zenuml/core', version: '3.49.2', integrity,
    gitHead: '213c239587ffa1f3dbb54d8b64468c3038ced55d'},
  backend: {name: '@napi-rs/canvas', version: canvasVersion, engine: 'Skia',
    browser: false, estimatedWidths: false, canvasCreations, fontMeasurements: fonts.length,
    availableFonts: GlobalFonts.families},
  cases: result
};
writeFileSync(resolve(output, 'official.json'), JSON.stringify(report, null, 2) + '\n');
writeFileSync(resolve(output, 'font-measurements.json'), JSON.stringify(fonts, null, 2) + '\n');
const hex = text => Array.from({length: text.length}, (_, i) => text.charCodeAt(i).toString(16).padStart(4, '0')).join('');
const fixture = result.map(row => `${row.id}\t${hex(row.source)}\t${hex(JSON.stringify(row.model))}`).join('\n') + '\n';
writeFileSync(resolve(output, 'zenuml-canvas.tsv'), fixture);
if (process.argv.includes('--write-fixture')) {
  writeFileSync(resolve(repository, 'tests/mermaid_cpp/fixtures/zenuml-canvas.tsv'), fixture);
}
console.log(JSON.stringify({cases: result.length, backend: '@napi-rs/canvas ' + canvasVersion,
  canvasCreations, fontMeasurements: fonts.length, output}));
