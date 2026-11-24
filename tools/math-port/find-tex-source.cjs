'use strict';
// Independent Source witness for the native FindTeX string-scanning contract.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const fixturePath = path.join(root, 'tests/math/fixtures/input/find-tex');
const pins = [
  ['.reference/MathJax-src/ts/input/tex/FindTeX.ts',
    '65355d94cac0c1e8a790313c52e231212733b5a98d6f6697c2c7864812a72944'],
  ['tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/FindTeX.js',
    '1361bd1171e5ef0c8210342d89c9cebd660dbb9d4810bc571c64172b3a5c3d4b'],
  ['.reference/MathJax-src/testsuite/tests/input/tex/Tex.test.ts',
    '480eb2ccc84e6c87576c31ca2c63d5e136e5638f98cc5ab45264e4a3cacc5f29']
];
const sha = file => crypto.createHash('sha256')
  .update(fs.readFileSync(path.join(root, file))).digest('hex');
for (const [file, expected] of pins)
  assert.equal(sha(file), expected, 'pinned input changed: ' + file);

const {FindTeX} = require(path.join(root,
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/input/tex/FindTeX.js'));
const sourceTests = [
  'abc $$ x + 1 $$ def',
  'abc \\begin{equation} x=y \\end{equation} def',
  'abc $$a + {\\bf b} + c$$ def',
  'abc \\$ def',
  'abc \\ref{x} def',
  'abc $$ x + 1',
  'abc \\begin{align} x + 1'
];
const sourceOptions = c => ({
  inlineMath: c.inlineMath || [['\\(', '\\)']],
  displayMath: c.displayMath || [['$$', '$$'], ['\\[', '\\]']],
  processEscapes: c.processEscapes,
  processEnvironments: c.processEnvironments,
  processRefs: c.processRefs
});

const cases = [];
for (let bits = 0; bits < 8; ++bits) {
  cases.push({name: 'options-' + bits, strings: [
    'a \\$ b \\ref{x} c $$q$$ \\begin{equation}z\\end{equation}',
    'again \\$ $$r$$ \\eqref{y}'
  ], processEscapes: !!(bits & 1), processEnvironments: !!(bits & 2),
  processRefs: !!(bits & 4)});
}
for (let i = 0; i < sourceTests.length; ++i)
  cases.push({name: 'upstream-' + i, strings: [sourceTests[i]],
    processEscapes: true, processEnvironments: true, processRefs: true});
cases.push(
  {name: 'brace-controls', strings: ['L $$a {b $$ c} \\} d \\{ e$$ R \\text{\\$}'],
    processEscapes: true, processEnvironments: true, processRefs: true},
  {name: 'overlap-longest', strings: ['<a>one</a> <ab>two</ab> <a>three</a>'],
    inlineMath: [['<a>', '</a>'], ['<ab>', '</ab>']], displayMath: [],
    processEscapes: false, processEnvironments: false, processRefs: false},
  {name: 'duplicate-last-mode', strings: ['##d##'],
    inlineMath: [['##', '##']], displayMath: [['##', '##']],
    processEscapes: false, processEnvironments: false, processRefs: false},
  {name: 'environment-cases', strings: [
    'x \\begin {align*}a {b} \\end{align*} y \\begin{align}c\\end{align}',
    'bad \\begin{align}open'
  ], processEscapes: false, processEnvironments: true, processRefs: false},
  {name: 'ref-escapes', strings: ['\\ref {x} \\eqref{y} \\$ \\\\ \\% \\ref{unfinished'],
    processEscapes: true, processEnvironments: false, processRefs: true},
  {name: 'utf16-nul-surrogates',
    strings: ['😀 $$x$$ \u0000 \ud800 \udfff $$y$$', 'tail 😀 \\ref{z}'],
    processEscapes: false, processEnvironments: false, processRefs: true},
  {name: 'repeat-cursor', strings: ['$$first$$ then $$second$$ \\$'],
    processEscapes: true, processEnvironments: true, processRefs: true}
);

const {utf16Field, utf16List, writeTables} = require('./fixture-data.cjs');
const inputs = [], expected = [];
for (const [index, c] of cases.entries()) {
  const options = sourceOptions(c);
  const found = new FindTeX(options).findMath(c.strings);
  inputs.push([utf16Field(c.name), c.strings.length, utf16List(c.strings),
    options.inlineMath.length, utf16List(options.inlineMath.flat()),
    options.displayMath.length, utf16List(options.displayMath.flat()),
    +c.processEscapes, +c.processEnvironments, +c.processRefs, found.length]);
  for (const [row, m] of found.entries()) {
    expected.push([index, row, ...[m.math, m.open, m.close].map(utf16Field),
      m.n, m.start.n, m.end.n,
      m.display === null ? 'escaped' : m.display ? 'display_math' : 'inline_math']);
  }
}
writeTables(fixturePath, {
  'source-cases': {
    columns: ['name_utf16', 'string_count', 'strings_utf16_list', 'inline_pair_count',
      'inline_utf16_list', 'display_pair_count', 'display_utf16_list',
      'process_escapes', 'process_environments', 'process_refs', 'expected_count'],
    rows: inputs
  },
  'source-expected': {
    columns: ['case_index', 'row_index', 'math_utf16', 'open_utf16', 'close_utf16', 'n', 'start', 'end', 'display'],
    rows: expected
  }
});
const recordCount = expected.length;
console.log('FindTeX Source witness: ' + cases.length + ' parser inputs; ' +
  recordCount + ' records; ' + sourceTests.length +
  ' upstream Tex.test.ts Finder inputs');
