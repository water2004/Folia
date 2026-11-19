// Actual upstream CSS, CSS-free native exports, and exact same-host RGBA checks.
import assert from 'node:assert/strict';
import {mkdirSync, readFileSync, readdirSync, writeFileSync} from 'node:fs';
import {createRequire} from 'node:module';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {parseArgs} from 'node:util';
import {fileHash, loadMathJaxProfile, sha256} from '../../mermaid-cpp/math-oracle/profile.mjs';
import {materializeSourcePaint, paintCanvas, sourceStyles, svgHostCss} from './source-paint.mjs';

const directory = dirname(fileURLToPath(import.meta.url));
const require = createRequire(new URL('../../mermaid-cpp/math-oracle/package.json', import.meta.url));
const sharp = require('sharp');
const {values} = parseArgs({options: {
  native: {type: 'string'}, input: {type: 'string', default: join(directory, 'cases.json')},
  output: {type: 'string', default: 'build/math-svg-paint/comparison'},
  mathml: {type: 'string'}, 'mathml-binary': {type: 'string'},
}});
assert(values.native, 'Usage: node check.mjs --native <capture/report.json> [--output directory]');
const corpus = JSON.parse(readFileSync(values.input, 'utf8'));
const native = JSON.parse(readFileSync(values.native, 'utf8'));
assert.equal(corpus.schema, 1);
assert(corpus.cases.length > 0 && corpus.modes.length > 0);
assert.equal(native.corpusSha256, fileHash(values.input));
assert.deepEqual(native.records.map(row => row.name),
  corpus.cases.flatMap(item => corpus.modes.map(mode => item.name + '--' + mode)));
const destination = resolve(values.output);
mkdirSync(destination, {recursive: true});
const profile = loadMathJaxProfile();
const {adaptor, document, output} = profile;
let rootBox;
const createRoot = output.createRoot;
output.createRoot = function (wrapper) {
  const result = createRoot.call(this, wrapper);
  rootBox ??= {...wrapper.getOuterBBox()};
  return result;
};

function parse(svg) {
  const parsed = adaptor.parse(svg, 'text/html');
  const roots = adaptor.childNodes(adaptor.body(parsed)).filter(node => adaptor.kind(node) === 'svg');
  assert.equal(roots.length, 1, 'Expected a single SVG root');
  return roots[0];
}
function documentRoots(raw) {
  const parsed = adaptor.parse(raw, 'text/html');
  const body = adaptor.body(parsed);
  const container = adaptor.childNodes(body).find(node => adaptor.kind(node) === 'mjx-container');
  const roots = adaptor.childNodes(container ?? body).filter(node => adaptor.kind(node) === 'svg');
  assert(roots.length > 0, 'Native result has no direct SVG roots');
  return roots;
}
function fragmentDimensions(item, box) {
  assert(!box.pwidth, 'This paint corpus requires an intrinsic root viewport');
  const em = item.em ?? 16, scale = em * .5 / output.font.params.x_height;
  return {width: Math.max(box.w, em / 1000) * scale,
    height: Math.max(box.h + box.d, em / 1000) * scale, baseline: box.h * scale};
}
function documentDimensions(item, root) {
  const pixels = name => {
    const printed = adaptor.getAttribute(root, name);
    assert.match(printed, /^\d+(?:\.\d+)?(?:ex|%)$/, `Expected a supported document ${name}`);
    if (printed.endsWith('%')) {
      assert.equal(name, 'width', 'Only document width can be a percentage');
      assert(Number.isFinite(item.containerWidth) && item.containerWidth > 0,
        'Percentage SVG needs the actual Source fixture container width');
      return Number.parseFloat(printed) * item.containerWidth / 100;
    }
    return Number.parseFloat(printed) * (item.ex ?? (item.em ?? 16) * .5);
  };
  return {width: pixels('width'), height: pixels('height')};
}
function canvas(svg, metrics, css = '') {
  return paintCanvas(adaptor, parse(svg), metrics, css);
}
async function raster(svg, file) {
  const image = sharp(Buffer.from(svg), {density: 144}).ensureAlpha();
  const raw = await image.clone().raw().toBuffer({resolveWithObject: true});
  if (file) await image.png().toFile(file);
  return raw;
}
function difference(a, b) {
  assert.deepEqual(a.info, b.info, 'Raster dimensions differ');
  let differentPixels = 0, maximumChannelDifference = 0;
  for (let i = 0; i < a.data.length; i += 4) {
    let changed = false;
    for (let channel = 0; channel < 4; ++channel) {
      const delta = Math.abs(a.data[i + channel] - b.data[i + channel]);
      changed ||= delta !== 0;
      maximumChannelDifference = Math.max(maximumChannelDifference, delta);
    }
    differentPixels += Number(changed);
  }
  return {differentPixels, maximumChannelDifference};
}
const ink = image => {
  let pixels = 0;
  for (let i = 3; i < image.data.length; i += 4) pixels += Number(image.data[i] !== 0);
  return pixels;
};
function targetNodes(root, feature) {
  const selected = [];
  const walk = (node, parent, grandparent) => {
    const tag = adaptor.kind(node), attr = name => adaptor.getAttribute(node, name);
    const has = name => adaptor.hasAttribute(node, name);
    if ((feature === 'table-lines' && tag === 'line' && has('data-line')) ||
        (feature === 'table-frame' && tag === 'rect' && has('data-frame')) ||
        (feature === 'enclosure-lines' && tag === 'line' && !has('data-line')) ||
        (feature === 'enclosure-curves' && (tag === 'ellipse' || (tag === 'rect' && has('rx')))) ||
        (feature === 'glyphs' && (tag === 'path' || tag === 'use') && has('data-c')) ||
        (feature === 'link' && tag === 'a') ||
        (feature === 'error' && tag === 'g' && attr('data-mml-node') === 'merror') ||
        (feature === 'table-viewport' && tag === 'svg' && parent && grandparent &&
          adaptor.kind(parent) === 'g' && adaptor.kind(grandparent) === 'g' &&
          adaptor.getAttribute(grandparent, 'data-mml-node') === 'mtable')) selected.push(node);
    if (tag !== '#text') for (const child of adaptor.childNodes(node)) walk(child, node, parent);
  };
  walk(root);
  return selected;
}
function paintChecks(svg, item) {
  const root = parse(svg), checks = [];
  const rootGroup = adaptor.childNodes(root).find(node => adaptor.kind(node) === 'g');
  checks.push({name: 'glyph-root-stroke-stays-zero', passed: !!rootGroup &&
    adaptor.getAttribute(rootGroup, 'stroke-width') === '0'});
  for (const feature of ['table-lines', 'table-frame']) {
    const nodes = targetNodes(root, feature);
    if (item.features.includes(feature)) checks.push({name: feature + '-present', passed: nodes.length > 0});
    for (let index = 0; index < nodes.length; ++index) {
      const node = nodes[index], attr = name => adaptor.getAttribute(node, name);
      const dash = attr('class') === 'mjx-dashed' ? '140' : attr('class') === 'mjx-dotted' ? '0 140' : null;
      checks.push({name: `${feature}-${index}-paint`, passed: attr('fill') === 'none' &&
        attr('stroke-width') === '70' && !adaptor.hasAttribute(node, 'stroke-thickness') &&
        (!dash || attr('stroke-dasharray') === dash) &&
        (attr('class') !== 'mjx-dotted' || attr('stroke-linecap') === 'round'),
        attributes: adaptor.allAttributes(node)});
    }
  }
  for (const node of targetNodes(root, 'glyphs')) checks.push({name: 'glyph-blacker',
    passed: adaptor.getAttribute(node, 'stroke-width') === '3'});
  for (const node of targetNodes(root, 'table-viewport')) checks.push({name: 'table-overflow',
    passed: adaptor.getAttribute(node, 'overflow') === 'visible'});
  if (item.features.includes('undefined-command')) {
    const lines = targetNodes(root, 'table-lines');
    checks.push({name: 'unknown-command-is-not-hline', passed: lines.length === 1 &&
      adaptor.getAttribute(lines[0], 'data-line') === 'v'});
  }
  return checks;
}

async function comparePart(item, name, sourceRoot, actualRoot, metrics, styles) {
    const row = {name, accepted: false, metrics};
    assert(Object.values(metrics).every(Number.isFinite) && metrics.width > 0 && metrics.height > 0);
    const source = adaptor.serializeXML(sourceRoot), actual = adaptor.serializeXML(actualRoot);
    const view = value => value === undefined ? null : value.trim().split(/\s+/).map(Number);
    for (const key of ['viewBox', 'data-mjx-viewBox']) {
      const sourceView = view(adaptor.getAttribute(sourceRoot, key));
      if (sourceView !== null) assert(sourceView.length === 4 && sourceView.every(Number.isFinite));
      assert.deepEqual(view(adaptor.getAttribute(actualRoot, key)), sourceView,
        `Native ${key} differs from upstream`);
    }
    const expected = canvas(source, metrics, styles.css);
    const bareSource = canvas(source, metrics);
    const actualCanvas = canvas(actual, metrics);
    const projected = adaptor.serializeXML(materializeSourcePaint(adaptor, parse(source), styles.rules));
    const projectedCanvas = canvas(projected, metrics);
    for (const [suffix, svg] of [['official', expected], ['official-bare', bareSource],
      ['official-explicit', projectedCanvas], ['native', actualCanvas]])
      writeFileSync(join(destination, `${name}-${suffix}.svg`), svg);
    const a = await raster(expected, join(destination, name + '-official.png'));
    const b = await raster(actualCanvas, join(destination, name + '-native.png'));
    const bare = await raster(bareSource, join(destination, name + '-official-bare.png'));
    const explicit = await raster(projectedCanvas);
    row.raster = difference(a, b);
    row.sourceCssEffect = difference(a, bare);
    row.sourceProjection = difference(a, explicit);
    row.inkPixels = {official: ink(a), native: ink(b)};
    row.paint = paintChecks(actual, item);
    row.features = [];
    for (const feature of item.features) {
      if (['invisible', 'dashed', 'dotted', 'undefined-command'].includes(feature)) continue;
      const sourceNodes = targetNodes(parse(source), feature);
      const nativeNodes = targetNodes(parse(actual), feature);
      assert.equal(nativeNodes.length, sourceNodes.length, `${feature} node count differs`);
      assert(sourceNodes.length > 0, `${feature} has no source primitives`);
      // Removing one primitive at a time must change real pixels. Table rules
      // are individually tested, even if several rules or glyphs overlap.
      const count = ['table-lines', 'table-frame', 'table-viewport'].includes(feature) ? sourceNodes.length : 1;
      for (let index = 0; index < count; ++index) {
        const withoutSource = parse(source), withoutNative = parse(actual);
        adaptor.remove(targetNodes(withoutSource, feature)[index]);
        adaptor.remove(targetNodes(withoutNative, feature)[index]);
        const removedA = await raster(canvas(adaptor.serializeXML(withoutSource), metrics, styles.css));
        const removedB = await raster(canvas(adaptor.serializeXML(withoutNative), metrics));
        const officialPixels = difference(a, removedA).differentPixels;
        const nativePixels = difference(b, removedB).differentPixels;
        row.features.push({feature, index, officialPixels, nativePixels,
          passed: officialPixels > 0 && nativePixels === officialPixels});
      }
    }
    if (item.features.includes('invisible')) row.features.push({feature: 'invisible',
      passed: row.inkPixels.official === 0 && row.inkPixels.native === 0});
    row.accepted = row.raster.differentPixels === 0 && row.sourceProjection.differentPixels === 0 &&
      row.paint.every(check => check.passed) && row.features.every(check => check.passed);
    row.hashes = {source: sha256(source), sourceCss: sha256(styles.css), native: sha256(actual),
      officialPng: fileHash(join(destination, name + '-official.png')),
      nativePng: fileHash(join(destination, name + '-native.png'))};
    return row;
}

const rows = [];
let recordIndex = 0, savedCss = false;
for (const item of corpus.cases) for (const mode of corpus.modes) {
  const target = native.records[recordIndex++], name = target.name;
  const row = {name, case: item.name, mode, accepted: false, parts: []};
  try {
    assert.equal(target.status, 0, target.error ?? 'Native render failed');
    assert(!target.error && target.svg);
    rootBox = null;
    output.options.displayOverflow = mode === 'fragment' ? 'overflow' : 'linebreak';
    output.options.linebreaks = {...output.options.linebreaks, inline: mode !== 'fragment'};
    const container = document.convert(item.tex, {display: mode !== 'inline', em: item.em ?? 16,
      ex: (item.em ?? 16) * .5, containerWidth: item.width ?? 1280});
    const roots = adaptor.childNodes(container).filter(node => adaptor.kind(node) === 'svg');
    const actualRoots = documentRoots(target.svg);
    assert(roots.length > 0, 'Source has no direct SVG roots');
    assert.equal(actualRoots.length, roots.length, 'Native and Source SVG fragment counts differ');
    if (mode === 'fragment') assert.equal(roots.length, 1, 'Standalone export must have one SVG root');
    assert(rootBox);
    const styles = sourceStyles(profile);
    if (!savedCss) {
      writeFileSync(join(destination, 'mathjax.css'), styles.css);
      writeFileSync(join(destination, 'mathjax-svg-host.css'), svgHostCss(styles.css));
      savedCss = true;
    }
    // Keep the complete original host output as well as every isolated SVG.
    // Multipart inline output is compared part-for-part; this is paint evidence,
    // not an assertion about an HTML host's line wrapping or fragment placement.
    writeFileSync(join(destination, name + '-official-original.xml'), adaptor.serializeXML(container));
    writeFileSync(join(destination, name + '-native-original.xml'), target.svg);
    for (let index = 0; index < roots.length; ++index) {
      const sourceRoot = roots[index], actualRoot = actualRoots[index];
      const metrics = mode === 'fragment' ? fragmentDimensions(item, rootBox)
        : documentDimensions(item, sourceRoot);
      if (mode === 'fragment') {
        assert(target.metrics, 'Fragment metrics missing');
        for (const key of ['width', 'height', 'baseline'])
          assert(Math.abs(target.metrics[key] - metrics[key]) <= 1e-9, `Fragment ${key} differs`);
      } else {
        for (const key of ['width', 'height']) assert.equal(adaptor.getAttribute(actualRoot, key),
          adaptor.getAttribute(sourceRoot, key), `Document ${key} differs`);
      }
      const partName = roots.length === 1 ? name : name + '--part' + (index + 1);
      row.parts.push(await comparePart(item, partName, sourceRoot, actualRoot, metrics, styles));
    }
    row.accepted = row.parts.every(part => part.accepted);
    for (const key of ['raster', 'sourceProjection', 'sourceCssEffect']) row[key] = {
      differentPixels: row.parts.reduce((sum, part) => sum + part[key].differentPixels, 0),
      maximumChannelDifference: Math.max(...row.parts.map(part => part[key].maximumChannelDifference)),
    };
  } catch (error) {
    row.error = {name: error.name, message: error.message};
  }
  rows.push(row);
  console.log(`${row.accepted ? 'PASS' : 'FAIL'} ${name}` + (row.error ? ': ' + row.error.message : ''));
}

let mathml = null;
if (values.mathml) {
  assert(values['mathml-binary'], '--mathml requires --mathml-binary for capture provenance');
  const binary = resolve(values['mathml-binary']), binaryHash = fileHash(binary);
  mathml = {binary: {path: binary, sha256: binaryHash}, families: {},
    input: 'Existing compiled direct MathML Source fixture nodes through Common LayoutJob / SVG writer',
    reference: 'Unmodified committed MathJax 4.1.3 / NewCM SVG fixture bytes; no TeX input claim'};
  const families = {
    table: {count: 8, fixture: 'tests/math/fixtures/common-source/table-mathml-scene.inc'},
    'table-profile': {count: 14, fixture: 'tests/math/fixtures/table_profile_source.hpp'},
  };
  for (const [family, info] of Object.entries(families)) {
    const location = resolve(values.mathml, family);
    const files = readdirSync(location).filter(name => name.endsWith('.json')).sort();
    assert.equal(files.length, info.count, `${family} capture is incomplete or contains stale cases`);
    mathml.families[family] = {...info, fixtureSha256: fileHash(resolve(directory, '../../..', info.fixture))};
    for (const file of files) {
      const item = JSON.parse(readFileSync(join(location, file), 'utf8'));
      const name = 'mathml--' + item.name, row = {name, case: item.name, mode: 'direct-mathml',
        family, accepted: false, parts: []};
      try {
        assert.equal(item.schema, 1);
        assert.equal(item.family, family);
        assert.match(item.name, /^[a-zA-Z0-9_-]+$/);
        const sourceFile = join(location, item.name + '.source.svg');
        const nativeFile = join(location, item.name + '.native.svg');
        const sourceRoots = documentRoots(readFileSync(sourceFile, 'utf8'));
        const actualRoots = documentRoots(readFileSync(nativeFile, 'utf8'));
        assert.equal(sourceRoots.length, 1, 'Direct MathML fixture should have one document root');
        assert.equal(actualRoots.length, 1);
        const sourceRoot = sourceRoots[0], actualRoot = actualRoots[0];
        for (const key of ['width', 'height']) assert.equal(adaptor.getAttribute(actualRoot, key),
          adaptor.getAttribute(sourceRoot, key), `Direct MathML ${key} differs`);
        item.features = ['table-lines', 'table-frame', 'table-viewport', 'glyphs']
          .filter(feature => targetNodes(sourceRoot, feature).length > 0);
        if (!item.features.length) item.features.push('invisible');
        const styles = sourceStyles(profile), metrics = documentDimensions(item, sourceRoot);
        const part = await comparePart(item, name, sourceRoot, actualRoot, metrics, styles);
        row.parts.push(part);
        row.accepted = part.accepted;
        for (const key of ['raster', 'sourceProjection', 'sourceCssEffect']) row[key] = part[key];
        row.capture = {sourceSha256: fileHash(sourceFile), nativeSha256: fileHash(nativeFile),
          manifestSha256: fileHash(join(location, file))};
      } catch (error) {
        row.error = {name: error.name, message: error.message};
      }
      rows.push(row);
      console.log(`${row.accepted ? 'PASS' : 'FAIL'} ${name}` + (row.error ? ': ' + row.error.message : ''));
    }
  }
  assert.equal(fileHash(binary), binaryHash, 'MathML capture executable changed during comparison');
}
const report = {schema: 1, reference: 'Pinned MathJax 4.1.3 / NewCM with actual host CSS',
  nativeReportSha256: fileHash(values.native), nativeBinary: native.binary,
  corpusSha256: fileHash(values.input), sourceLock: profile.provenance, rasterizer: sharp.versions,
  comparison: 'Exact 2x transparent RGBA, no registration, filtering or tolerance',
  hostProjection: 'Only mjx-container selector tag maps to svg[data-mathjax-host]; all CSS declarations retained',
  multipartInline: 'Every SVG part compared independently; HTML line wrapping and part placement are not tested',
  mathml, browserCompared: false, windowsUiTested: false,
  total: rows.length, accepted: rows.filter(row => row.accepted).length, records: rows};
writeFileSync(join(destination, 'report.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({destination, total: report.total, accepted: report.accepted}));
process.exitCode = report.accepted === report.total ? 0 : 1;
