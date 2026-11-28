'use strict';
// Execute the corresponding CommonOutputJax methods from the pinned MathJax CJS package.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
function repositoryRoot() {
  const arg = process.argv.indexOf('--repo-root');
  if (arg >= 0 && (!process.argv[arg + 1] || process.argv.length !== arg + 2))
    throw new Error('usage: node <witness> [--repo-root <repository>]');
  const root = arg >= 0 ? path.resolve(process.argv[arg + 1]) : findRepositoryRoot();
  assert.ok(root, 'pass --repo-root <repository> or run the witness from within the repository');
  return root;
}
const root = repositoryRoot();
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const sourceTs = path.join(root, '.reference/MathJax-src/ts/output/common.ts');
const sourceCjs = path.join(packageRoot, 'src/cjs/output/common.js');
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
assert.equal(hash(sourceTs), '3686a8e803e26ae59d03c096ac2e6e5b1ab1332a5c4bcaa083cb9a02c2c61096');
assert.equal(hash(sourceCjs), 'b8a58d983e465f21ed36574472140bfd65e2696bb72f6c5647e0a2ae47eb1bd9');
for (const name of ['src', 'mathjax-newcm-font']) {
  const manifest = path.join(packageRoot, name, 'package.json');
  assert.equal(JSON.parse(fs.readFileSync(manifest, 'utf8')).version, '4.1.3');
}
const load = file => require(path.join(packageRoot, 'src/cjs', file));

async function run() {
  const { mathjax } = load('mathjax.js');
  const { TeX } = load('input/tex.js');
  const { SVG } = load('output/svg.js');
  const { liteAdaptor } = load('adaptors/liteAdaptor.js');
  const { RegisterHTMLHandler } = load('handlers/html.js');
  const { Styles } = load('util/Styles.js');
  const { StyleJsonSheet } = load('util/StyleJson.js');
  const packageNewcm = require(path.join(packageRoot, 'mathjax-newcm-font/cjs/svg.js'));
  const adaptor = liteAdaptor({ fontSize: 16 });
  RegisterHTMLHandler(adaptor);
  const output = new SVG({ fontData: packageNewcm.MathJaxNewcmFont,
    fontCache: 'none', exFactor: .5, linebreaks: { inline: true } });
  const input = new TeX({ packages: ['base'] });
  const document = mathjax.document('', { InputJax: input, OutputJax: output });
  await document.convertPromise('x+y', { display: true, em: 16, ex: 8, containerWidth: 400 });

  const css = output.cssFontStyles(['serif', true, true], {});
  const font = output.getFontData(new Styles('font-family: serif; font-style: italic; font-weight: bold;'));
  const metrics = output.getMetricsFor(adaptor.node('span'), true);
  const sheet = new StyleJsonSheet();
  output.addFontStyles(sheet);
  output.addWrapperStyles(sheet);
  assert.equal(output.containerWidth, undefined,
    'overflow layout leaves the linebreak constraint unset');
  assert.ok(Number.isNaN(Math.max(output.containerWidth / 10, output.containerWidth - 1)),
    'an unset constraint must not shrink ordinary auto-width tables to zero');
  output.getLinebreakWidth();
  const linebreakWidth = output.containerWidth;
  for (const child of output.math.root.childNodes) output.markInlineBreaks(child);
  const marked = [];
  output.math.root.walkTree(node => {
    if (node.getProperty('breakable')) marked.push(node.kind);
  });
  const processed = !!output.math.root.getProperty('process-breaks');
  output.unmarkInlineBreaks(output.math.root);

  // Exercise the two Source branches that deliberately mark the *parent* of
  // the node being scanned. Build these with the pinned MmlFactory and its
  // ordinary appendChild links so the witness observes real MML node behavior.
  const factory = output.math.root.factory;
  const make = kind => factory.create(kind);
  const append = (parent, child) => parent.appendChild(child);
  const setBreakOperator = parent => {
    const operator = make('mo');
    operator.attributes.set('linebreak', 'before');
    operator.attributes.set('linebreakstyle', 'before');
    operator.texClass = 3; // TEXCLASS.REL
    append(operator, factory.create('text').setText('+'));
    append(parent, operator);
  };
  const styleMath = make('math');
  const styleContainer = make('mrow');
  const style = make('mstyle');
  const styleContents = make('mrow');
  append(styleContents, make('mi'));
  setBreakOperator(styleContents);
  append(style, styleContents);
  // Source's recursive-mark handoff condition is an existing process-breaks
  // property on the wrapper child; seed that state so this check isolates the
  // parent chosen by its branch.
  style.setProperty('process-breaks', 'true');
  append(styleContainer, style);
  append(styleMath, styleContainer);
  output.markInlineBreaks(styleContainer);
  const styleParentMarked = !!styleContainer.parent.getProperty('process-breaks');
  assert.equal(!!styleContainer.getProperty('process-breaks'), false);
  assert.equal(styleParentMarked, true,
    'mstyle branch marks the scanned node parent, as Source does');

  const semanticMath = make('math');
  const semanticContainer = make('mrow');
  const semanticAdded = make('mrow');
  semanticAdded.attributes.set('data-semantic-added', 'true');
  append(semanticAdded, make('mi'));
  setBreakOperator(semanticAdded);
  semanticAdded.setProperty('process-breaks', 'true');
  append(semanticContainer, semanticAdded);
  append(semanticMath, semanticContainer);
  output.markInlineBreaks(semanticContainer);
  const semanticParentMarked = !!semanticContainer.parent.getProperty('process-breaks');
  assert.equal(!!semanticContainer.getProperty('process-breaks'), true);
  assert.equal(semanticParentMarked, true,
    'data-semantic-added branch marks the scanned node parent, as Source does');

  let calls = 0;
  output.measureTextNode = () => ({ w: ++calls, h: 2, d: 1 });
  output.unknownCache.clear();
  const measure = (text, variant, fontData) =>
    output.measureTextNodeWithCache(null, text, variant, fontData);
  const explicit = ['serif', true, false];
  const bold = ['serif', false, true];
  const first = measure('x', '-explicitFont', explicit);
  const cached = measure('x', '-explicitFont', explicit);
  const otherExplicit = measure('x', '-explicitFont', bold);
  const normal = measure('x', 'normal', ['', false, false]);
  const normalOtherText = measure('y', 'normal', ['', false, false]);
  const actual = {
    forceInlineBreaks: output.forceInlineBreaks,
    family: output.font.getFamily('serif'), css,
    fontData: font,
    styles: { selectorCount: Object.keys(sheet.styles).length,
      cssLength: sheet.getStyleString().length },
    inlineBreaks: { linebreakWidth, marked, processed,
      nestedParentMarks: { style: styleParentMarked, semanticAdded: semanticParentMarked },
      cleared: !output.math.root.getProperty('process-breaks') },
    metrics: { em: metrics.em, ex: metrics.ex, containerWidth: metrics.containerWidth,
      display: metrics.display, family: metrics.family },
    cache: { calls, first, cached, otherExplicit, normal, normalOtherText,
      keys: Array.from(output.unknownCache.keys()) },
  };
  const fixtureRoot = process.env.FOLIA_COMMON_SOURCE_FIXTURE_ROOT || path.join(root, 'tests/math/fixtures/common-source');
  const expectedPath = path.join(fixtureRoot, 'jax-source-witness.expected.json');
  assert.deepEqual(actual, JSON.parse(fs.readFileSync(expectedPath, 'utf8')));
  assert.equal(actual.cache.calls, 4, 'Source caches by variant key and exact text');
  assert.equal(actual.metrics.display, true, 'getMetricsFor copies its display request');
  const outputPath = path.join(root, 'build/math-port/common-source/jax.json');
  fs.mkdirSync(path.dirname(outputPath), {recursive: true});
  fs.writeFileSync(outputPath, JSON.stringify(actual, null, 2) + '\n');
  process.stdout.write(`pinned CommonOutputJax Source witness passed: metrics, CSS font data, cache keys; ${path.relative(root, outputPath)}\n`);
}

run().catch(error => { process.stderr.write(error.stack + '\n'); process.exitCode = 1; });
