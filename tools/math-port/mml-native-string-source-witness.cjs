'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
function findRepositoryRoot(start) {
  let current=path.resolve(start);
  while (!fs.existsSync(path.join(current, 'tools/math-port/README.md'))) {
    const parent=path.dirname(current);
    if(parent===current) throw new Error('Cannot locate repository root');
    current=parent;
  }
  return current;
}
const rootOption=process.argv.indexOf('--repo-root');
const repo=rootOption<0 ? findRepositoryRoot(__dirname) : path.resolve(process.argv[rootOption+1]);
const sourcePath = path.join(repo, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/string.js');
assert.equal(require(path.join(repo,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')).version,'4.1.3');
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(sourcePath)).digest('hex'),
  'e1f09bca257dd268b9483b7310e4c77039dd022f01c5e9f5a62f47a981921e9c');
const source = require(sourcePath);
const cases = [];
function check(name, actual, expected) { assert.deepStrictEqual(actual, expected, name); cases.push({name, actual}); }
function caught(fn) { try { fn(); } catch (error) { return {name:error.name, message:error.message}; } throw new Error('expected source exception'); }
check('sortLength length then lexical order', [source.sortLength('bbb','aa'), source.sortLength('a','b'), source.sortLength('b','b')], [-1,-1,0]);
check('sortLength uses UTF-16 string length', source.sortLength('😀','a'), -1);
check('quotePattern escapes regexp syntax', source.quotePattern('^$(){}.+*?-|[]:\\'), '\\^\\$\\(\\)\\{\\}\\.\\+\\*\\?\\-\\|\\[\\]\\:\\\\');
check('unicodeChars astral and isolated surrogates', source.unicodeChars('A😀x\uD800y\uDC00'), [65,0x1F600,120,0xD800,121,0xDC00]);
check('unicodeString valid scalar and surrogate code points', source.unicodeString([65,0x1F600,0xD800,0xDC00]), 'A😀\uD800\uDC00');
check('unicodeString preserves an isolated surrogate', source.unicodeString([0xD800]), '\uD800');
check('unicodeString permits zero and max scalar', source.unicodeString([0,0x10FFFF]), '\0\uDBFF\uDFFF');
for (const point of [-1,0x110000,1.5,NaN,Infinity])
  check(`unicodeString invalid ${String(point)}`, caught(()=>source.unicodeString([point])), {name:'RangeError',message:`Invalid code point ${String(point)}`});
check('isPercent with ECMAScript whitespace', [source.isPercent('50%\u00A0'),source.isPercent('50%\uFEFF'),source.isPercent('% '),source.isPercent('50% x'),source.isPercent('')], [true,true,true,false,false]);
check('split Unicode whitespace and empty input', [source.split(' \u00A0alpha\u2003beta\uFEFF '),source.split(''),source.split('\u3000\u2028')], [['alpha','beta'],[''],['']]);
check('replaceUnicode valid fixed/braced escapes and escaped slash', source.replaceUnicode('x\\U0041/\\U{ 1F600 }/\\\\z'), 'xA/😀/\\z');
check('replaceUnicode fallback escape and malformed braced sequence', [caught(()=>source.replaceUnicode('\\a')),caught(()=>source.replaceUnicode('\\Uq')),source.replaceUnicode('\\U{1}'),caught(()=>source.replaceUnicode('\\U{1234567}'))], [{name:'RangeError',message:'Invalid code point NaN'},{name:'RangeError',message:'Invalid code point NaN'},'\u0001',{name:'RangeError',message:'Invalid code point NaN'}]);
check('replaceUnicode preserves line terminator escape', source.replaceUnicode('x\\\ny'), 'x\\\ny');
check('toEntity uses uppercase hexadecimal code points', [source.toEntity('a'),source.toEntity('😀'),source.toEntity('\uD800')], ['&#x61;','&#x1F600;','&#xD800;']);
check('toEntity empty-input Source error', caught(()=>source.toEntity('')), {name:'TypeError',message:"Cannot read properties of undefined (reading 'toString')"});
console.log(JSON.stringify({packageVersion:require(path.join(repo,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')).version,sourcePath:path.relative(repo,sourcePath).replaceAll('\\','/'),sourceSha256:crypto.createHash('sha256').update(fs.readFileSync(sourcePath)).digest('hex'),passed:cases.length,cases},null,2));
