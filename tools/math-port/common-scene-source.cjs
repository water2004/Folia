'use strict';
// Independent pinned MathJax Source witness for Common -> immutable Scene SVG.
// This deliberately never loads, reads, or derives expectations from native output.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const {createReference} = require('./reference-profile.cjs');
const {canonicalSceneCases} = require('./common-scene-cases.cjs');
const {byteField, binary64, table} = require('./fixture-data.cjs');

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'tests/math/fixtures/scenes/common.tsv');
const digest = data => crypto.createHash('sha256').update(data).digest('hex');
const profileCases = JSON.parse(fs.readFileSync(path.join(__dirname,
  'common-layout-reference/profile-cases.json'), 'utf8'));
const sourceLock = JSON.parse(fs.readFileSync(path.join(__dirname,
  'common-layout-reference/profile-source-lock.json'), 'utf8'));
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
assert.equal(digest(fs.readFileSync(scopePath)), sourceLock.scopeHash, 'profile scope lock changed');
assert.equal(digest(fs.readFileSync(path.join(__dirname,
  'common-layout-reference/profile-cases.json'))), sourceLock.corpusSha256, '75-case corpus changed');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
for (const dependency of sourceLock.dependencies)
  assert.equal(digest(fs.readFileSync(path.join(packageRoot, dependency.file))), dependency.sha256,
    `pinned profile/font dependency changed: ${dependency.file}`);
const cases = canonicalSceneCases(profileCases);
for (const [name, tex, display, width] of [
  ['scene-identifiers-numbers', 'abc+123.45+\\mathbb{R}', false, 1280],
  ['scene-text-composition', '\\text{漢字😀 é}+x', false, 1280],
  ['scene-unknown-run', '\\text{☃☃☃}+\\text{漢字}', false, 1280],
  ['scene-fixed-operator', '\\sum_{i=1}^{n}+\\int_0^1 x\\,dx', false, 1280],
  ['scene-assembled-operator', '\\left\\langle\\frac{1}{x}\\right\\rangle', true, 1280],
  ['scene-css-id-href', '\\href{https://example.test/a?b=1&c=2}{\\cssId{probe-id}{x+1}}', false, 1280],
  ['scene-row-multiline', '\\begin{aligned}a&=b+c+d+e+f\\\\x&=y+z\\end{aligned}', true, 120],
]) cases.push({name, tex, display, width, features: ['base']});
// Script-family acceptance inputs are appended after the original 82 records.
// Keep each input focused on one mathematical family so an inline break in
// another family cannot obscure its Source geometry.
for (const [name, tex, display, width, features] of [
  ['scene-script-sub', 'x_i', false, 1280, ['base']],
  ['scene-script-sup', 'x^i', false, 1280, ['base']],
  ['scene-script-subsup', 'x_i^j', false, 1280, ['base']],
  ['scene-script-nested', 'x_i^{j_k}', false, 1280, ['base']],
  ['scene-sum-limits-inline', '\\sum_i', false, 1280, ['base']],
  ['scene-sum-limits-display', '\\sum_i', true, 1280, ['base']],
  ['scene-overline-single', '\\overline{x}', false, 1280, ['base']],
  ['scene-widehat-single', '\\widehat{x}', false, 1280, ['base']],
  ['scene-prescript-sup-slot', '\\prescript{a}{}{X}', false, 1280, ['mathtools']],
  ['scene-prescript-sub-slot', '\\prescript{}{b}{X}', false, 1280, ['mathtools']],
  ['scene-prescript-both-slots', '\\prescript{a}{b}{X}', false, 1280, ['mathtools']],
]) cases.push({name, tex, display, width, features});
// Inline break coordination, text-space opportunities and an empty zero-style
// frame are captured as complete Source output, including root geometry.
for (const [name, tex, display, width, features] of [
  ['scene-inline-break-before', 'x\\mmlToken{mo}[linebreak="newline" linebreakstyle="before"]{+}y', false, 1280, ['base']],
  ['scene-inline-break-after', 'x\\mmlToken{mo}[linebreak="newline" linebreakstyle="after"]{+}y', false, 1280, ['base']],
  ['scene-text-wordbreak', '\\text{one two three four}', true, 40, ['base']],
  ['scene-zero-size-frame', '\\bbox[border:0px solid]{}', false, 1280, ['bbox']],
]) cases.push({name, tex, display, width, features});
for (const [name, tex, display, width] of [
  ['scene-vertical-extender-assembly', '\\left(\\frac{a}{\\frac{b}{\\frac{c}{d}}}\\right)', true, 1280],
  ['scene-horizontal-extender-assembly', '\\overbrace{abcdefghij}', true, 1280],
]) cases.push({name, tex, display, width, features: ['base']});
// Phantom keeps the measured box while suppressing its hidden drawing.
// Narrow makeboxes force negative mpadded dw and preserve Source's explicit
// center/right alignment metadata around a raised rule.
for (const [name, tex, display, width, features] of [
  ['scene-simple-phantom', '\\phantom{x}', true, 1280, ['base']],
  ['scene-padding-negative-dw-center', '\\makebox[.25em][c]{\\rule[.2em]{1em}{.4em}}', false, 1280, ['base']],
  ['scene-padding-negative-dw-right', '\\makebox[.25em][r]{\\rule[.2em]{1em}{.4em}}', false, 1280, ['base']],
]) cases.push({name, tex, display, width, features});
// Table-focused inputs are appended after the original 102 records. The first
// five reuse exact TeX inputs from the independent 14-case table witness;
// additions pin dashed row/column lines, labels, nesting and percentage sizing.
for (const item of [
  {name: 'scene-table-array-basic', tex: String.raw`\begin{array}{cc}a&b\\c&d\end{array}`, display: true},
  {name: 'scene-table-array-column-rules', tex: String.raw`\begin{array}{c|c||r}a&b&c\\d&e&f\end{array}`, display: true},
  {name: 'scene-table-array-row-rules', tex: String.raw`\begin{array}{cc}\hline a&b\\\hline c&d\\\hline\end{array}`, display: true},
  {name: 'scene-table-array-cell-padding', tex: String.raw`\begin{array}{c@{\quad}r}x&1\\longer&22\end{array}`, display: true},
  {name: 'scene-table-array-paragraph-column', tex: String.raw`\begin{array}{p{3em}c}a+b+c&x\\d+e+f&y\end{array}`, display: true},
  {name: 'scene-table-row-dashed', tex: String.raw`\begin{array}{cc}\hdashline a&b\\\hdashline c&d\\\hdashline\end{array}`, display: true},
  {name: 'scene-table-column-dashed', tex: String.raw`\begin{array}{c:c}a&b\\c&d\end{array}`, display: true},
  {name: 'scene-table-label-right', tex: String.raw`\begin{align}x&=1\tag{TableSceneRightA}\\y&=2\tag{TableSceneRightB}\end{align}`, display: true, tagSide: 'right'},
  {name: 'scene-table-label-left-metric', tex: String.raw`\begin{align}x&=1\tag{TableSceneLeftA}\\y&=2\tag{TableSceneLeftB}\end{align}`, display: true, em: 20, ex: 7, tagSide: 'left'},
  {name: 'scene-table-nested-label', tex: String.raw`\begin{align}\begin{aligned}x&=1\\y&=2\end{aligned}&=0\tag{TableSceneNestedA}\end{align}`, display: true, tagSide: 'right'},
]) cases.push({...item, width: 1280, features: ['base', 'ams']});
// Source-validated CommonMenclose line/radical cases promoted from the ignored
// enclosure discovery witness. Preserve their exact full38 input metrics.
for (const [name, tex, display, em, ex, width] of [
  ['scene-enclose-all-lines', String.raw`\enclose{top bottom left right horizontalstrike verticalstrike updiagonalstrike downdiagonalstrike}{x}`, false, 16, 8, 1280],
  ['scene-enclose-paired-sides', String.raw`\enclose{actuarial madruwb}{x}`, false, 16, 8, 1280],
  ['scene-enclose-radical-top', String.raw`\enclose{radical top}{x}`, false, 16, 8, 1280],
]) cases.push({name, tex, display, em, ex, width, features: ['enclose']});

// Complete pinned SVG menclose notation inventory and four Source boundaries,
// appended after the byte-locked original 115 records.
const enclosureNotationNames = [
  'top', 'right', 'bottom', 'left', 'actuarial', 'madruwb', 'updiagonalstrike',
  'downdiagonalstrike', 'horizontalstrike', 'verticalstrike', 'box', 'roundedbox',
  'circle', 'phasorangle', 'uparrow', 'downarrow', 'leftarrow', 'rightarrow',
  'updownarrow', 'leftrightarrow', 'updiagonalarrow', 'northeastarrow', 'southeastarrow',
  'northwestarrow', 'southwestarrow', 'northeastsouthwestarrow',
  'northwestsoutheastarrow', 'longdiv', 'radical'
];
for (const notation of enclosureNotationNames)
  cases.push({name: `scene-enclose-notation-${notation}`,
    tex: String.raw`\enclose{${notation}}{x}`, display: false, em: 16, ex: 8,
    width: 1280, features: ['enclose']});
for (const item of [
  {name: 'scene-enclose-boundary-roundedbox-empty',
    tex: String.raw`\enclose{roundedbox}{}`, notation: 'roundedbox'},
  {name: 'scene-enclose-boundary-uparrow-left',
    tex: String.raw`\enclose{uparrow left}{x}`, notation: 'uparrow left'},
  {name: 'scene-enclose-boundary-rightarrow-top',
    tex: String.raw`\enclose{rightarrow top}{x}`, notation: 'rightarrow top'},
  {name: 'scene-enclose-boundary-arrow-custom-parameters',
    tex: String.raw`\enclose{rightarrow}[data-thickness=2px,data-padding=3px,data-arrowhead={2 3 4}]{x}`,
    notation: 'rightarrow'}
]) cases.push({...item, display: false, em: 16, ex: 8, width: 1280, features: ['enclose']});

// Preserve the older CommonWrapper broken-border geometry witness after all
// existing records; the original 75-case Source corpus and case order stay fixed.
for (const [name, style] of [['scene-border-dashed', 'dashed'],
                             ['scene-border-dotted', 'dotted']])
  cases.push({name, tex: String.raw`\bbox[border:2px ${style} red]{x+y}`,
    display: false, em: 16, ex: 8, width: 1280, features: ['bbox']});

function attr(svg, name) {
  const rootTag = svg.match(/<svg\b[^>]*>/)?.[0] || '';
  return tagAttr(rootTag, name);
}
function tagAttr(tag, name) {
  const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  return tag.match(new RegExp('\\b' + escaped + '="([^"]*)"'))?.[1] ?? '';
}
function nestedAssemblyPieces(svg, axis) {
  const openings = [...svg.matchAll(/<svg\b[^>]*>/g)];
  const transform = axis === 'vertical'
    ? /transform="scale\(1,[^)]+\)"/
    : /transform="scale\([^,]+,1\)"/;
  const pieces = [];
  for (const opening of openings.slice(1)) {
    const close = svg.indexOf('</svg>', opening.index);
    if (close < 0) continue;
    const piece = svg.slice(opening.index, close + '</svg>'.length);
    if (piece.includes('<path data-c=') && transform.test(piece)) pieces.push(piece);
  }
  return pieces;
}

async function capture() {
  const ref = createReference(); // exactly once per process; profile/pins validated there
  const records = [];
  const configuredParseAllowTexHTML = ref.input.parseOptions.options.allowTexHTML;
  for (const item of cases) {
    let svg = '', error = null;
    const inputOptionsAllowTexHTML = ref.input.options.allowTexHTML ?? null;
    const initialAllowTexHTML = inputOptionsAllowTexHTML ??
      configuredParseAllowTexHTML ?? false;
    const parseOptionsBeforeConvert = ref.input.parseOptions.options.allowTexHTML ?? null;
    try {
      // The profile default is the Source's right-side tag placement. Specific
      // left-label witnesses record their per-input TeX option explicitly.
      ref.input.parseOptions.options.tagSide = item.tagSide ?? 'right';
      const node = ref.document.convert(item.tex, {
        display: item.display, em: item.em ?? 16, ex: item.ex ?? 8,
        containerWidth: item.width
      });
      svg = ref.adaptor.serializeXML(node);
    } catch (e) { error = {name: e?.name || 'Error', message: String(e?.message || e)}; }
    const finalAllowTexHTML = ref.input.parseOptions.options.allowTexHTML ?? null;
    const finalInputOptionsAllowTexHTML = ref.input.options.allowTexHTML ?? null;
    records.push({...item, tagSide: item.tagSide ?? '', em: item.em ?? 16, ex: item.ex ?? 8, svg, error,
      initialAllowTexHTML, inputOptionsAllowTexHTML, parseOptionsBeforeConvert,
      finalAllowTexHTML, finalInputOptionsAllowTexHTML,
      root: {width: attr(svg, 'width'), height: attr(svg, 'height'), viewBox: attr(svg, 'viewBox'),
        xmlns: attr(svg, 'xmlns')},
      geometry: [...svg.matchAll(/<(?:path|rect|line|polygon|g)\b[^>]*>/g)].map(m => m[0])});
  }
  const nonDefaultAllowTexHTML = records.slice(0, 75)
    .filter(record => record.initialAllowTexHTML !== false)
    .map(({name, initialAllowTexHTML, inputOptionsAllowTexHTML, parseOptionsBeforeConvert}) =>
      ({name, initialAllowTexHTML, inputOptionsAllowTexHTML, parseOptionsBeforeConvert}));
  const evidencePath = path.join(root, 'build/math-port/common-source/common-scene-inputs.json');
  fs.mkdirSync(path.dirname(evidencePath), {recursive: true});
  fs.writeFileSync(evidencePath, JSON.stringify({
    reference: ref.provenance,
    records: records.map(({name, tex, display, em, ex, width, tagSide, features,
      initialAllowTexHTML, inputOptionsAllowTexHTML, parseOptionsBeforeConvert,
      finalAllowTexHTML, finalInputOptionsAllowTexHTML, error, svg}) => ({
      name, input: {tex, display, em, ex, width, tagSide, features},
      initialAllowTexHTML, inputOptionsAllowTexHTML, parseOptionsBeforeConvert,
      finalAllowTexHTML, finalInputOptionsAllowTexHTML,
      sourceError: error, outputContainsMmlError: svg.includes('data-mjx-error=') ||
        svg.includes('data-mml-node="merror"')
    })),
    canonicalNonDefaultInitialAllowTexHTML: nonDefaultAllowTexHTML
  }, null, 2) + '\n');
  assert.deepEqual(nonDefaultAllowTexHTML, [],
    'Canonical Source inputs contain nondefault initial allowTexHTML; Root review required before fixture changes');
  const byName = new Map(records.map(record => [record.name, record]));
  for (const [name, axis] of [['scene-vertical-extender-assembly', 'vertical'],
                              ['scene-horizontal-extender-assembly', 'horizontal']]) {
    const record = byName.get(name);
    assert.ok(record && !record.error, `${name} must convert in pinned Source`);
    const pieces = nestedAssemblyPieces(record.svg, axis);
    assert.ok(pieces.length > 0, `${name} must contain Source nested SVG extender pieces`);
  }
  const phantom = byName.get('scene-simple-phantom');
  assert.ok(phantom && !phantom.error, 'simple phantom must convert in pinned Source');
  assert.ok(phantom.svg.includes('data-mml-node="mphantom"'),
    'simple phantom must preserve its Source wrapper');
  assert.ok(!phantom.svg.includes('data-c='), 'simple phantom must suppress all glyph drawing');
  const [viewX, viewY, viewWidth, viewHeight] = phantom.root.viewBox.split(/\s+/).map(Number);
  assert.ok(Number.isFinite(viewX) && Number.isFinite(viewY) && viewWidth > 0 && viewHeight > 0,
    'simple phantom must retain nonzero completed Source dimensions');
  for (const [name, alignment] of [
    ['scene-padding-negative-dw-center', 'center'],
    ['scene-padding-negative-dw-right', 'right'],
  ]) {
    const record = byName.get(name);
    assert.ok(record && !record.error, `${name} must convert in pinned Source`);
    assert.ok(record.svg.includes(`data-mml-node="mpadded" data-align="${alignment}"`),
      `${name} must retain Source data-align=${alignment}`);
    assert.ok(record.svg.includes('width="1000"') && record.root.viewBox.includes(' 250 '),
      `${name} must demonstrate negative dw against its wider rule content`);
    assert.ok(record.svg.includes('transform="translate(0,200)"'),
      `${name} must retain its positive vertical offset`);
  }
  const requireSourceMarkup = (name, fragments) => {
    const record = byName.get(name);
    assert.ok(record && !record.error, `${name} must convert in pinned Source`);
    for (const fragment of fragments)
      assert.ok(record.svg.includes(fragment), `${name} Source SVG must contain ${fragment}`);
  };
  for (const name of ['scene-table-array-basic', 'scene-table-array-cell-padding',
                      'scene-table-array-paragraph-column'])
    requireSourceMarkup(name, ['data-mml-node="mtable"']);
  requireSourceMarkup('scene-table-array-row-rules', ['data-mml-node="mtable"', 'data-line="h"', 'mjx-solid']);
  requireSourceMarkup('scene-table-array-column-rules', ['data-mml-node="mtable"', 'data-line="v"', 'mjx-solid']);
  requireSourceMarkup('scene-table-row-dashed', ['data-mml-node="mtable"', 'data-frame-styles="dashed', 'data-line="h"', 'mjx-dashed', 'stroke-dasharray=']);
  requireSourceMarkup('scene-table-column-dashed', ['data-mml-node="mtable"', 'data-line="v"', 'mjx-dashed']);
  const enclosureData = name => {
    const record = byName.get(name);
    assert.ok(record && !record.error, `${name} must convert in pinned Source`);
    assert.ok(record.svg.includes('data-mml-node="menclose"'), `${name} must contain Source menclose`);
    const start = record.svg.indexOf('data-mml-node="menclose"');
    const subtree = record.svg.slice(start);
    const viewBox = attr(record.svg, 'viewBox').split(/\s+/).map(Number);
    assert.equal(viewBox.length, 4, `${name} Source viewBox must have four values`);
    assert.ok(viewBox.every(Number.isFinite) && viewBox[2] > 0 && viewBox[3] > 0,
      `${name} Source root geometry must be finite and nonzero`);
    const tags = [...subtree.matchAll(/<(?:path|rect|line|ellipse|polygon)\b[^>]*>/g)]
      .map(match => match[0]);
    return {record, subtree, viewBox, tags};
  };
  for (const notation of enclosureNotationNames) {
    const {tags} = enclosureData(`scene-enclose-notation-${notation}`);
    assert.ok(tags.length > 0, `${notation} Source notation must emit a drawable primitive`);
  }
  const sourceLineCount = (name, expected) => {
    const {tags} = enclosureData(name);
    assert.equal(tags.filter(tag => tag.startsWith('<line')).length, expected,
      `${name} Source line primitive count`);
  };
  sourceLineCount('scene-enclose-notation-top', 1);
  sourceLineCount('scene-enclose-notation-actuarial', 2);
  sourceLineCount('scene-enclose-notation-madruwb', 2);
  sourceLineCount('scene-enclose-notation-updiagonalstrike', 1);
  sourceLineCount('scene-enclose-notation-downdiagonalstrike', 1);
  sourceLineCount('scene-enclose-notation-horizontalstrike', 1);
  sourceLineCount('scene-enclose-notation-verticalstrike', 1);
  assert.ok(enclosureData('scene-enclose-notation-box').tags.some(tag => tag.startsWith('<rect')),
    'box must preserve Source rect geometry');
  assert.ok(enclosureData('scene-enclose-notation-roundedbox').tags.some(tag => tag.startsWith('<rect')),
    'roundedbox must preserve Source rect geometry');
  assert.ok(enclosureData('scene-enclose-notation-circle').tags.some(tag => tag.startsWith('<ellipse')),
    'circle must preserve Source ellipse geometry');
  const roundedEmpty = enclosureData('scene-enclose-boundary-roundedbox-empty');
  assert.ok(!roundedEmpty.subtree.includes('data-c='), 'empty roundedbox must have no child glyph');
  const roundedRect = roundedEmpty.tags.find(tag => tag.startsWith('<rect'));
  assert.ok(roundedRect, 'empty roundedbox must emit a Source rect');
  const roundedWidth = Number(tagAttr(roundedRect, 'width'));
  const roundedRadius = Number(tagAttr(roundedRect, 'rx'));
  assert.ok(Number.isFinite(roundedWidth) && Number.isFinite(roundedRadius) &&
    roundedWidth > 0 && roundedRadius > roundedWidth / 2,
    'empty roundedbox must preserve Source rx greater than half-width');
  const phasor = enclosureData('scene-enclose-notation-phasorangle').tags
    .filter(tag => tag.startsWith('<path')).at(-1);
  const longdiv = enclosureData('scene-enclose-notation-longdiv').tags
    .filter(tag => tag.startsWith('<path')).at(-1);
  for (const [name, tag, join] of [
    ['phasorangle', phasor, 'mitre'], ['longdiv', longdiv, 'round']
  ]) {
    assert.ok(tag && /^<path style="stroke-width: [^"]+;"/.test(tag),
      `${name} Source stroke-width must be sole first style property`);
    assert.ok(tagAttr(tag, 'stroke-linejoin') === join && tagAttr(tag, 'stroke-linecap') === 'round' &&
      tagAttr(tag, 'fill') === 'none' && tagAttr(tag, 'd'),
      `${name} Source path paint attributes and join spelling`);
  }
  assert.match(tagAttr(longdiv, 'd'), /\ba\s+[^ ]+\s+[^ ]+\s+0\s+0,1\s+0\s+853/,
    'longdiv must preserve its Source relative arc command and delta');
  for (const notation of ['uparrow', 'downarrow', 'leftarrow', 'rightarrow', 'updownarrow',
    'leftrightarrow', 'updiagonalarrow', 'northeastarrow', 'southeastarrow', 'northwestarrow',
    'southwestarrow', 'northeastsouthwestarrow', 'northwestsoutheastarrow']) {
    const arrow = enclosureData(`scene-enclose-notation-${notation}`).tags
      .filter(tag => tag.startsWith('<path') && !tag.includes('data-c=')).at(-1);
    assert.ok(arrow && /\bl\s/.test(tagAttr(arrow, 'd') || ''),
      `${notation} must preserve relative Source arrow path commands`);
  }
  for (const [notation, transform] of [
    ['uparrow', 'rotate(90 '], ['downarrow', 'rotate(-90 '], ['leftarrow', 'rotate(-180 '],
    ['updownarrow', 'rotate(-90 '], ['updiagonalarrow', 'rotate(38.378 '],
    ['northeastarrow', 'rotate(38.378 '], ['southeastarrow', 'rotate(-38.378 '],
    ['northwestarrow', 'rotate(141.622 '], ['southwestarrow', 'rotate(-141.622 '],
    ['northeastsouthwestarrow', 'rotate(38.378 '], ['northwestsoutheastarrow', 'rotate(-38.378 ']
  ]) {
    const arrow = enclosureData(`scene-enclose-notation-${notation}`).tags
      .filter(tag => tag.startsWith('<path') && !tag.includes('data-c=')).at(-1);
    assert.ok(tagAttr(arrow, 'transform')?.includes(transform),
      `${notation} must preserve its Source arrow transform`);
  }
  const leftArrow = enclosureData('scene-enclose-boundary-uparrow-left').tags
    .filter(tag => tag.startsWith('<path') && !tag.includes('data-c=')).at(-1);
  const topArrow = enclosureData('scene-enclose-boundary-rightarrow-top').tags
    .filter(tag => tag.startsWith('<path') && !tag.includes('data-c=')).at(-1);
  assert.ok(tagAttr(leftArrow, 'transform')?.includes('translate(133.5 0)'),
    'uparrow+left must preserve nonzero Source x translation');
  assert.ok(tagAttr(topArrow, 'transform')?.includes('translate(0 -133.5)'),
    'rightarrow+top must preserve nonzero Source y translation');
  sourceLineCount('scene-enclose-boundary-rightarrow-top', 1);
  const customArrow = enclosureData('scene-enclose-boundary-arrow-custom-parameters');
  const customPath = customArrow.tags.filter(tag => tag.startsWith('<path') && !tag.includes('data-c=')).at(-1);
  const defaultRightArrow = enclosureData('scene-enclose-notation-rightarrow').tags
    .filter(tag => tag.startsWith('<path') && !tag.includes('data-c=')).at(-1);
  assert.ok(customArrow.subtree.includes('data-thickness="2px"') &&
    customArrow.subtree.includes('data-padding="3px"') &&
    customArrow.subtree.includes('data-arrowhead="2 3 4"') &&
    customArrow.subtree.includes('data-arrowhead={2 3 4}') &&
    tagAttr(customPath, 'd') !== tagAttr(defaultRightArrow, 'd'),
    'custom arrowhead/padding/thickness Source input must alter its completed path');
  const radicalEnclosure = enclosureData('scene-enclose-notation-radical');
  assert.ok(radicalEnclosure.subtree.includes('data-mml-node="msqrt"') &&
    radicalEnclosure.subtree.includes('data-c="221A"'),
    'radical notation must render a nested msqrt root glyph');
  sourceLineCount('scene-enclose-all-lines', 8);
  sourceLineCount('scene-enclose-paired-sides', 4);
  const radicalTop = enclosureData('scene-enclose-radical-top');
  assert.ok(radicalTop.subtree.includes('data-mml-node="msqrt"') &&
    radicalTop.subtree.includes('data-c="221A"') &&
    radicalTop.subtree.includes('translate(853,0)') &&
    radicalTop.tags.filter(tag => tag.startsWith('<line')).length === 1,
    'radical+top must keep the independent Source root glyph and top-rule geometry');
  for (const [name, side] of [['scene-table-label-right', 'right'],
                              ['scene-table-label-left-metric', 'left'],
                              ['scene-table-nested-label', 'right']]) {
    const record = byName.get(name);
    assert.ok(record && !record.error, `${name} must convert in pinned Source`);
    assert.ok(record.svg.includes('data-labels="true"'), `${name} must retain Source labels`);
    assert.ok(record.svg.includes('data-mml-node="mlabeledtr"'), `${name} must retain labeled rows`);
    assert.equal(record.tagSide, side, `${name} tag side must be explicit Source input metadata`);
    assert.ok(record.svg.includes('width="100%"') && record.svg.includes('data-mjx-viewBox='),
      `${name} must preserve the Source percentage viewport`);
    assert.ok(record.svg.includes(`mjx-eqn:${name.includes('left') ? 'TableSceneLeftA' :
      name.includes('nested') ? 'TableSceneNestedA' : 'TableSceneRightA'}`),
      `${name} must retain its unique explicit Source tag identity`);
  }
  const nestedTable = byName.get('scene-table-nested-label');
  assert.ok((nestedTable.svg.match(/data-mml-node="mtable"/g) || []).length >= 2,
    'nested label witness must contain nested MathML table structures');
  const metricTable = byName.get('scene-table-label-left-metric');
  assert.equal(metricTable.em, 20);
  assert.equal(metricTable.ex, 7);
  assert.equal(metricTable.width, 1280);
  assert.ok(metricTable.svg.includes('scale(63.14)'),
    'nondefault metric label table must pin Source top-table scale precision=2');
  return {records};
}

function generate({records}) {
  return table(
    ['name_bytes', 'tex_bytes', 'display', 'em_binary64', 'ex_binary64', 'width',
      'tag_side_bytes', 'svg_bytes', 'root_width_bytes', 'root_height_bytes',
      'root_view_box_bytes', 'geometry_bytes', 'error_name_bytes', 'error_message_bytes'],
    records.map(r => [byteField(r.name), byteField(r.tex), +r.display, binary64(r.em),
      binary64(r.ex), r.width, byteField(r.tagSide ?? ''), byteField(r.svg),
      byteField(r.root.width), byteField(r.root.height), byteField(r.root.viewBox),
      byteField(JSON.stringify(r.geometry)), byteField(r.error?.name ?? ''),
      byteField(r.error?.message ?? '')]),
    [`scope=${sourceLock.scopeHash}; profile_corpus=${sourceLock.corpusSha256}; font_Common_dependency_files=${sourceLock.dependencies.length}`]);
}

(async () => {
  const witness = await capture();
  const next = generate(witness);
  const actualHash = fs.existsSync(out) ? digest(fs.readFileSync(out)) : null;
  const nextHash = digest(next);
  if (process.argv.includes('--write-fixture')) {
    if (actualHash !== nextHash) fs.writeFileSync(out, next);
    process.stdout.write(`wrote ${path.relative(root, out)} sha256=${nextHash} records=${witness.records.length}\n`);
  } else {
    assert.equal(actualHash, nextHash, `Source fixture differs; inspect Source observations, then use --write-fixture. expected fixture hash ${actualHash}, generated ${nextHash}`);
    process.stdout.write(`Source bytecheck ok: ${witness.records.length} records sha256=${nextHash}\n`);
  }
})().catch(error => { process.stderr.write(error.stack + '\n'); process.exitCode = 1; });
