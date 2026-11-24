'use strict';
// Original MathJax option layering and ParseOptions' four actual style functions.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const base = 'tools/mathjax-bundle/node_modules/@mathjax/src';
const locks = {
  '.reference/MathJax-src/ts/input/tex/ParseOptions.ts': 'ab60a07dbb24040c0fb9bd67b942755ffc5b0b18980abf1938736d8d90b22209',
  '.reference/MathJax-src/ts/util/Options.ts': 'b810ffc09864eb8ed9295d829b229194d7e38aaac6c8f01525bc1a4b190a61e3',
  '.reference/MathJax-src/ts/input/tex/TexConstants.ts': 'd9380972dd21333d4699ff351f6af0a589f7c1f9096f0918dc1152287aed37b5',
  [`${base}/cjs/input/tex/ParseOptions.js`]: 'ff19d70539cb76911c5f1d7673ddfd908183dcf5849c91cb068b19e58da8a9ec',
  [`${base}/cjs/util/Options.js`]: '76f3df8a3e899fba1d0944d5b79818b586c1d9f95cc7021c395fbb10295db660',
  [`${base}/cjs/input/tex/TexConstants.js`]: '5c30716472a64ee59b58ddc1c4fe53c7d00baf9d0a37e2288e307996d5dfbcff'
};
assert.equal(JSON.parse(fs.readFileSync(path.join(root, base, 'package.json'))).version, '4.1.3');
assert.equal(process.versions.unicode, '16.0');
for (const [file, hash] of Object.entries(locks))
  assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root, file))).digest('hex'), hash, file);
const {default: ParseOptions} = require(path.join(root, base, 'cjs/input/tex/ParseOptions.js'));
const {defaultOptions} = require(path.join(root, base, 'cjs/util/Options.js'));

const initial = {
  maxBuffer: 5120, physics: {italicdiff: true, arrowdel: true},
  noundefined: {color: 'red', size: 'small'}, protectedMacros: ['a', 'b']
};
const layer = {maxBuffer: 0, physics: {italicdiff: false}, noundefined: {color: ''}, protectedMacros: ['c']};
const merged = defaultOptions({}, initial, layer);
assert.deepEqual(merged, {
  maxBuffer: 0, physics: {italicdiff: false, arrowdel: true},
  noundefined: {color: '', size: 'small'}, protectedMacros: ['c']
});
layer.protectedMacros.push('changed');
assert.deepEqual(merged.protectedMacros, ['c']);
assert.deepEqual(initial.physics, {italicdiff: true, arrowdel: true});

const permissions = defaultOptions({},
  {setoptions: {allowOptions: {tex: {maxBuffer: false, mathStyle: true}, physics: true}}},
  {setoptions: {allowOptions: {tex: {maxMacros: false, mathStyle: false}, color: false}}});
assert.deepEqual(permissions.setoptions.allowOptions,
  {tex: {maxBuffer: false, mathStyle: false, maxMacros: false}, physics: true, color: false});
defaultOptions(permissions, {setoptions: {allowOptions: {tex: false, physics: {italicdiff: false}}}});
assert.deepEqual(permissions.setoptions.allowOptions,
  {tex: false, physics: {italicdiff: false}, color: false});

const macros = defaultOptions({}, {macros: {a: ['old', 1, 'x'], b: 'keep'}}, {macros: {a: 'new', c: 'add'}});
assert.deepEqual(macros.macros, {a: 'new', b: 'keep', c: 'add'});
const regexp = /a/g;
const regexOptions = defaultOptions({}, {numberPattern: regexp});
assert.equal(regexOptions.numberPattern, regexp);
regexOptions.numberPattern.lastIndex = 2;
assert.equal(regexp.lastIndex, 2);
assert.equal(ParseOptions.getVariant.get('tex'), undefined);

const inputs = ['', 'A', 'a', 'É', 'é', 'Å', 'å', 'Å', 'ǅ', 'ά', 'Α', 'ϴ', 'ϑ', 'Ω', 'ς',
  'ß', 'ﬁ', '\ud800', '\u0301a', '𝔸', '𝑎', 'ā', 'ÿ', 'aB', 'Αabc', '\u03a2', 'K', 'ſ', 'a\0', '\0a'];
const rows = inputs.map(input => [input,
  ParseOptions.getVariant.get('TeX')(input, false),
  ParseOptions.getVariant.get('TeX')(input, true),
  ParseOptions.getVariant.get('French')(input)]);
for (const input of inputs) {
  for (const bold of [false, true]) {
    assert.equal(ParseOptions.getVariant.get('ISO')(input, bold), 'italic');
    assert.equal(ParseOptions.getVariant.get('upright')(input, bold), 'normal');
    assert.equal(ParseOptions.getVariant.get('French')(input, bold), rows[inputs.indexOf(input)][3]);
  }
}
const {utf16Field, writeTables} = require('./fixture-data.cjs');
writeTables(path.join(root, 'tests/math/fixtures/input/parse-options'), {
  cases: {columns: ["input_utf16", "tex_utf16", "texBold_utf16", "french_utf16"],
    rows: rows.map(row => row.map(utf16Field))}
});
console.log(JSON.stringify({status: 'passed', styleObservations: inputs.length * 8, sourceFiles: Object.keys(locks).length,
  limit: 'Native option values/default layering and style resolver functions only; no constructor or complete TeX pipeline acceptance.'}));
