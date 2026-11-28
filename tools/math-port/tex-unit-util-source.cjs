'use strict';
// Actual pinned MathJax UnitUtil and its original UnitMap constructor; no stubs.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
let root = __dirname;
while (!fs.existsSync(path.join(root, 'tools/math-port/README.md'))) {
  const parent = path.dirname(root);
  if (parent === root) throw new Error('repository root not found');
  root = parent;
}
const source = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
assert.equal(JSON.parse(fs.readFileSync(path.join(source, 'package.json'))).version, '4.1.3');
const files = ['cjs/input/tex/UnitUtil.js', 'ts/input/tex/UnitUtil.ts'];
const hashes = Object.fromEntries(files.map(file => [file,
  crypto.createHash('sha256').update(fs.readFileSync(path.join(source, file))).digest('hex')]));
hashes['reference/ts/input/tex/UnitUtil.ts'] = crypto.createHash('sha256')
  .update(fs.readFileSync(path.join(root, '.reference/MathJax-src/ts/input/tex/UnitUtil.ts'))).digest('hex');
assert.deepEqual(hashes, JSON.parse(fs.readFileSync(path.join(root, 'tests/math/fixtures/tex-source/unit-util-source-lock.json'))));
const {UnitUtil: u} = require(path.join(source, files[0]));
const UnitMap = u.UNIT_CASES.constructor;
const defaults = [['em',1],['ex',.43],['pt',1/10],['pc',1.2],['px',7.2/72],
  ['in',7.2],['cm',7.2/2.54],['mm',7.2/25.4],['mu',1/18]];
const saved = u.UNIT_CASES;
const number = x => x === undefined ? 'absent' : Number.isNaN(x) ? 'NaN' :
  x === Infinity ? 'Infinity' : x === -Infinity ? '-Infinity' : Object.is(x,-0) ? '-0' : x;
const dimension = x => x[0] === null ? null : {value:x[0],unit:x[1],matched_length:x[2]};
const actual = {defaults:[],matches:[],formats:[],trims:[],maps:[]};
const fresh = entries => {u.UNIT_CASES = new UnitMap(entries || defaults);};
const snapshot = name => {const map=u.UNIT_CASES; return {name,unit:map.unit,
  end:map.dimenEnd.source,rest:map.dimenRest.source};};
const match = (input, rest=false) => ({input,rest,match:dimension(u.matchDimen(input,rest)),
  em:number(u.dimen2em(input))});
try {
  fresh();
  for (const [unit] of defaults) actual.defaults.push({unit,factor:number(u.UNIT_CASES.get(unit)),
    ...match('1'+unit)});
  for (const [input, rest] of [
    ['',false],['bad',false],['2',false],['2EM',false],['em',false],['2%',false],
    ['+2.pt',false],['-,5em',false],['1,25ex',false],['2..em',false],['2e3em',false],
    [' 2 em \t',false],['\u00A0+2,5\u202Fem\uFEFF',false],['2em tail',false],
    ['2em  tail',true],['2em\ttail',true],['\t2 em  tail',true],['2em\r\n',false],
    ['2\u0085em',false],['-0em',false],['-0mu',false],['1mu',false],['18mu',false],
    ['-18mu',false],['0,01mu',false],['9'.repeat(400)+'em',false],
    ['-0.'+'0'.repeat(400)+'1em',false],['0.'+'0'.repeat(400)+'1mu',false],
    ['100000000000000000000000000000000mu',false]
  ]) actual.matches.push(match(input,rest));

  for (const x of [0,-0,0.0005999999999999999,0.0006,-0.0006,0.000999,1,1.2345,
    1.2346,-1.2345,1.0005,2.675,1000,999999999999999900000,1e21,-1e21,
    1e30,1e100,1.234e100,Number.MAX_VALUE,NaN,Infinity,-Infinity])
    actual.formats.push({input:number(x),output:u.em(x)});

  for (const text of ['', '  a  ', ' a\\ ', ' a\\  ', 'a\\\t', 'a\\\u00A0',
    'a\\\u00A0 ', '\\\\ ', '𝛼 ', '\u0085a\u0085', '\u180Ea\u180E', '\u200Ba\u200B'])
    actual.trims.push({input:text,output:u.trimSpaces(text)});
  for (const cp of [0x9,0xA,0xB,0xC,0xD,0x20,0xA0,0x1680,0x2000,0x2001,0x2002,
    0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,0x2009,0x200A,0x2028,0x2029,
    0x202F,0x205F,0x3000,0xFEFF]) {
    const text=String.fromCharCode(cp)+'x'+String.fromCharCode(cp);
    actual.trims.push({input:text,output:u.trimSpaces(text)});
  }

  fresh([['x',1],['pt',.1],['x',2]]);
  actual.maps.push({...snapshot('duplicate-order'),x:number(u.UNIT_CASES.get('x'))});
  const same=u.UNIT_CASES.set('x',3)===u.UNIT_CASES;
  u.UNIT_CASES.set('z',4);
  actual.maps.push({...snapshot('update-append'),same,...match('2z')});
  const old=u.UNIT_CASES.dimenEnd;
  const absent=u.UNIT_CASES.delete('missing'),kept=old===u.UNIT_CASES.dimenEnd;
  const removed=u.UNIT_CASES.delete('x'),rebuilt=old!==u.UNIT_CASES.dimenEnd;
  u.UNIT_CASES.set('x',5);
  actual.maps.push({...snapshot('erase-reinsert'),absent,kept,removed,rebuilt});

  fresh([['pt',.1],['zero',0],['nan',NaN],['neg',-2],['inf',Infinity]]);
  actual.maps.push({name:'truthy-fallback',values:['zero','nan','neg','inf','missing'].map(
    key=>({key,value:number(u.UNIT_CASES.get(key))})),...match('2zero')});
  u.UNIT_CASES.delete('pt');
  actual.maps.push({name:'missing-pt',values:['zero','nan','neg','missing'].map(
    key=>({key,value:number(u.UNIT_CASES.get(key))})),invalid:match('bad'),zero:match('2zero')});
  u.UNIT_CASES.set('pt',0);
  actual.maps.push({name:'zero-pt',value:number(u.UNIT_CASES.get('missing')),...match('bad')});
  u.UNIT_CASES.set('pt',NaN);
  actual.maps.push({name:'nan-pt',value:number(u.UNIT_CASES.get('missing')),...match('bad')});

  fresh([]); actual.maps.push({...snapshot('empty'),...match('2')});
  fresh([['',3],['pt',.1]]);
  actual.maps.push({name:'empty-unit',valid:match('2'),invalid:match('bad')});
  fresh([['e',1],['em',2],['pt',.1]]);
  actual.maps.push({name:'prefix',partial:match('2emtail',true),complete:match('2em')});
  fresh([['x.',3],['pt',.1]]);
  actual.maps.push({name:'raw-regex',literal:match('2xy'),rest:match('2xyMORE',true)});
  fresh([['𝛼',2],['pt',.1]]); actual.maps.push({name:'utf16-unit',...match('2𝛼tail',true)});
  fresh(); u.UNIT_CASES.set('mu',.25);
  actual.maps.push({name:'live-mu-factor',...match('2mu')});
  u.UNIT_CASES.set('em',2);
  actual.maps.push({name:'mu-second-em-factor',...match('2mu')});
  u.UNIT_CASES.set('mu',0);
  actual.maps.push({name:'mu-zero-fallback',...match('2mu')});
  u.UNIT_CASES.set('mu',Infinity);
  actual.maps.push({name:'mu-infinite-factor',...match('2mu')});
} finally {u.UNIT_CASES=saved;}

assert.deepEqual(actual,JSON.parse(fs.readFileSync(path.join(root,'tests/math/fixtures/tex-source/unit-util-source-expected.json'))));
process.stdout.write(JSON.stringify({status:'passed',defaults:actual.defaults.length,
  matches:actual.matches.length,formats:actual.formats.length,trims:actual.trims.length,
  maps:actual.maps.length,sourceFiles:files.length,referenceFiles:1})+'\n');
