'use strict';

// Independent whole-dependency witness for pinned mhchemparser 4.2.1.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const hash = file => crypto.createHash('sha256')
  .update(fs.readFileSync(path.join(root, file))).digest('hex');

const lockPath = 'tools/math-port/locks/mhchem-source-lock.json';
const lock = JSON.parse(fs.readFileSync(path.join(root, lockPath), 'utf8'));
for (const file of [...lock.files, ...lock.generated]) {
  assert.equal(hash(file.path), file.sha256, `Pinned mhchem input changed: ${file.path}`);
}

const testFile = 'tools/mathjax-bundle/node_modules/mhchemparser/test/test-dist.html';
const expectedTestSha256 = '48e6534d119cdc67eca7e9e4e0c3f2399042faef6d33f5b506908cb736231740';
const expectedCorpusSha256 = 'f61a474e985768223800081ab1a406b93298d14e4e330cacbaf3e1a8989fd86e';
const expectedCounts = { cases: 1679, patterns: 6279, machines: 14, regexLiterals: 62, samples: 69 };

function loadSource() {
  const file = 'tools/mathjax-bundle/node_modules/mhchemparser/esm/mhchemParser.js';
  const code = fs.readFileSync(path.join(root, file), 'utf8')
    .replace('export class mhchemParser', 'class mhchemParser');
  return vm.runInNewContext(`${code}\n;({parser:_mhchemParser,texify:_mhchemTexify,api:mhchemParser})`);
}

function plain(value) {
  if (value === undefined) return { $: 'undefined' };
  if (Array.isArray(value)) return Array.from(value, plain);
  if (value && typeof value === 'object')
    return Object.fromEntries(Object.entries(value).map(([key, item]) => [key, plain(item)]));
  return value;
}

function sourceCorpus() {
  const cases = [];
  const html = fs.readFileSync(path.join(root, testFile), 'utf8');
  assert.equal(hash(testFile), expectedTestSha256, 'Pinned dependency test corpus changed');
  const tests = html.slice(html.indexOf("test('tex'"), html.lastIndexOf('</script>'));
  vm.runInNewContext(tests, {
    test: (machine, input) => cases.push({ name: `dependency-${cases.length}`, machine, input }),
    output: () => {}, errorsFound: false
  });
  const extra = [
    '', ' ', '\t', '\n', '\u2212', '\u2013', '\u2014', '\u2010', '\u2026',
    'A\\bond{x}B', 'A\\bond{<}B\\bond{>}C', 'H_{(aq)}^+', '+IV', '-IX', '+-0',
    'A->[above][below]B', 'A<-->[above][below]B', 'A<=>[above][below]B',
    'A<=>>M[x]T[text]B', 'A<<=>T[text]M[x_i]B',
    '\\color{red}{H2O}', '\\color\\red{H2O}', '\\color{red}H2O',
    '\\frac{H2}{O2}', '\\overset{x}{H2}', '\\underset{x}{H2}', '\\underbrace{H2}_{x}',
    'n/2a H2O', '$2n-1$ H2O', '$-$H2O', '2 $n$ H2O',
    '{a_b-c d-e f}', 'H_{i,j}', 'H_{i}', 'H_{x}', 'H_{V,1}', 'H^{+-0}',
    '\\ce{H2O}', '\\pu{123 kg}', '\\ce{H2O} + \\pu{123 kg}',
    '^\\foo{a}{b}H_\\bar{x}{y}', 'A^', 'A_', 'A^_2', 'A_\\alpha',
    'A & B \\\\ C \\hline D', 'A~B|C/D', 'NaOH(aq,$\\infty$)',
    '(', ')', '{', '}', '$', '$x', '\\frac{a}', '\\color',
    '12345.12345 kg', '1.23456(0.01111) kg', '1.23456(1111) kg',
    '+-1,23456e-12 kg', '+/-1.25E{12} kg', '1.2*10^-3 kg', '1.2 x 10^3 kg',
    '10^{-3} kg', '123 kg2', '123 kg^-2', '123 kg^{-2}',
    '1 °C', '1 ^oC', '1 ^{o}C', '1 °F', '1 kg // °C', '1 {kg s} / {mol K}',
    '1 \\alpha2', '1 {kg}', '1 kg. m', '1 kg* m', '1 kg\\,m', '1 kg//mol//s',
    '😀', 'a\u0301', '\ud800', '\udfff'
  ];
  for (const machine of ['tex', 'ce', 'a', 'o', 'text', 'pq', 'bd', 'oxidation', 'tex-math',
    'tex-math tight', '9,9', 'pu', 'pu-2', 'pu-9,9']) {
    for (const input of extra) cases.push({ name: `machine-${machine}-${cases.length}`, machine, input });
  }
  let seed = 0x4d686368;
  const next = n => { seed ^= seed << 13; seed ^= seed >>> 17; seed ^= seed << 5; return (seed >>> 0) % n; };
  const atoms = ['H', 'Na', 'Cl', 'CO', 'Fe', '\\alpha', '(NH4)', '[AgCl2]', '{text}', '{$x$}'];
  const scripts = ['', '2', '^+', '^2-', '_{i}', '^{II}', '_2^+', '{}^3', '\\bond{~-}'];
  const joins = [' + ', '-', '=', ' -> ', ' <=>[H2O] ', ' ', '.', '*'];
  for (let i = 0; i < 400; ++i) {
    const input = atoms[next(atoms.length)] + scripts[next(scripts.length)] + joins[next(joins.length)] +
      atoms[next(atoms.length)] + scripts[next(scripts.length)];
    cases.push({ name: `combination-${i}`, machine: 'ce', input });
  }
  const samples = [...new Set(['', 'A', 'x', 'i', '1', '12', '12.3', '-1', ' ', '\t', '\n',
    '\\alpha ', '\\alpha{}', '\\greek', '{a{b}c}tail', '${a_b}$tail', '$x$tail',
    '^\\foo{x}{y}tail', '_\\foo{x}{y}tail', '\\foo{x}{y}tail', '\\foo{x}tail',
    '^{2-}tail', 'H2O', '(aq)', '(aq,$\\infty$) ', '($c$)', '_{(aq)}', '-(aq)',
    'A, B', ', ', '...', '....', '->', '<-->', '\u21cc', '\\frac{x}{y}tail', '\\underbrace{x}_{y}tail',
    '\\color{red}{H2O}tail', '\\color\\red{H2O}tail', '\\ce{H2O}tail', '\\pu{123 kg}tail',
    '+-1.2(0.01)e-3 kg', '1.2E3', '1.2 x 10^{-3}', '1^3', 'n/2', '$2n-1$', '2nH',
    'pH', 'iPr', 'V,1', '0.1x', 'x0.1', '(xyz)', '(x+y)', '{}^', 'v', '(v)', '^',
    '\\bond{~--}', '\\hline ', '\\,', '/', '//', '*', '😀', '}', '{x', '\ud800'])];
  const signature = JSON.stringify({ cases: cases.map(({ name, machine, input }) => [name, machine, input]), samples });
  const corpusSha256 = crypto.createHash('sha256').update(signature).digest('hex');
  assert.equal(cases.length, expectedCounts.cases);
  assert.equal(samples.length, expectedCounts.samples);
  assert.equal(corpusSha256, expectedCorpusSha256, 'Pinned parser cases/pattern samples changed');
  return { cases, samples };
}

const source = loadSource();
const data = sourceCorpus();
const capture = fn => {
  try { return { result: plain(fn()), error: null }; }
  catch (error) { return { result: null, error: plain(error) }; }
};
const sourceCases = data.cases.map(row => ({
  mode: 'parse', name: row.machine, input: row.input,
  observation: capture(() => {
    const parsed = source.parser.go(row.input, row.machine);
    const before = plain(parsed);
    const tex = source.texify.go(parsed, row.machine !== 'tex');
    const after = plain(parsed);
    const repeat = source.texify.go(parsed, row.machine !== 'tex');
    return { parsed: before, tex, after, repeat, afterRepeat: plain(parsed),
      toTex: source.api.toTex(row.input, row.machine) };
  })
}));
const sourcePatterns = [];
for (const name of Object.keys(source.parser.patterns.patterns)) {
  for (const input of data.samples) {
    sourcePatterns.push({ mode: 'pattern', name, input,
      observation: capture(() => source.parser.patterns.match_(name, input)) });
  }
}
const tables = {};
for (const [name, machine] of Object.entries(source.parser.stateMachines)) {
  tables[name] = {};
  for (const [state, transitions] of Object.entries(machine.transitions)) {
    tables[name][state] = transitions.map(({ pattern, task }) => ({
      pattern,
      actions: task.action_.map(({ type_, option }) => ({ type: type_, option: plain(option) })),
      nextState: task.nextState || '', revisit: !!task.revisit, toContinue: !!task.toContinue
    }));
  }
}
const regexLiterals = Object.entries(source.parser.patterns.patterns)
  .filter(([, pattern]) => typeof pattern !== 'function')
  .map(([name, pattern]) => ({ name, source: pattern.source, flags: pattern.flags }));
assert.equal(sourceCases.length, expectedCounts.cases);
assert.equal(sourcePatterns.length, expectedCounts.patterns);
assert.equal(Object.keys(tables).length, expectedCounts.machines);
assert.equal(regexLiterals.length, expectedCounts.regexLiterals);

const quote16 = value => `"${Array.from({ length: value.length }, (_, i) =>
  `\\u${value.charCodeAt(i).toString(16).padStart(4, '0')}`).join('')}"`;
const wire = value => {
  if (typeof value === 'string') return quote16(value);
  if (value === null) return 'null';
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  if (Array.isArray(value)) return `[${value.map(wire).join(',')}]`;
  return `{${Object.entries(value).map(([key, item]) => `${JSON.stringify(key)}:${wire(item)}`).join(',')}}`;
};
// Table machine/state names are object keys but the established native observer
// emits them through its UTF-16 quote routine; retain that exact record format.
const tableWire = value => `{${Object.entries(value).map(([key, item]) =>
  `${quote16(key)}:${item && !Array.isArray(item) ? tableWire(item) : wire(item)}`).join(',')}}`;
const allRows = [
  ...sourceCases.map(row => ({ ...row, expected: wire(row.observation) })),
  ...sourcePatterns.map(row => ({ ...row, expected: wire(row.observation) })),
  { mode: 'tables', name: '', input: '', expected: wire({ result: tables, error: null })
    .replace(wire(tables), tableWire(tables)) },
  { mode: 'regexes', name: '', input: '', expected: wire({ result: regexLiterals, error: null }) }
];
assert.equal(allRows.length, 7960, 'Complete parser/pattern/table/regex corpus is required');

// Store reference observations as runtime data. Escaping UTF-16 code units
// preserves malformed strings exactly without compiling multi-megabyte literals.
const escape16 = value => {
  let result = '';
  for (let i = 0; i < value.length; ++i) {
    const unit = value.charCodeAt(i);
    result += unit >= 32 && unit < 127 && unit !== 92 && unit !== 35
      ? value[i] : `\\u${unit.toString(16).padStart(4, '0')}`;
  }
  return result;
};
const fixture = [
  '# Pinned reference observations; values exported without normalization.',
  '# mode\tname_utf16\tinput_utf16\texpected_json',
  `# source_lock_sha256=${hash(lockPath)}`
];
for (const row of allRows) {
  assert(!/[\t\r\n]/.test(row.expected), 'Wire observations must fit one TSV field');
  fixture.push([row.mode, escape16(row.name), escape16(row.input), row.expected].join('\t'));
}
fixture.push('');
const bytes = fixture.join('\n');
const target = path.join(root, 'tests/math/fixtures/mhchem_parser.tsv');
if (process.argv.includes('--write-fixture')) {
  const current = fs.existsSync(target) ? fs.readFileSync(target, 'utf8') : null;
  if (current !== bytes) fs.writeFileSync(target, bytes);
} else assert.equal(fs.readFileSync(target, 'utf8'), bytes,
  'mhchem parser fixture differs from pinned Source; pass --write-fixture to update');
console.log(JSON.stringify({ source: 'mhchemparser 4.2.1', sourceLockSha256: hash(lockPath),
  cases: sourceCases.length, patterns: sourcePatterns.length, machines: Object.keys(tables).length,
  regexes: regexLiterals.length, total: allRows.length, fixture: path.relative(root, target),
  mode: process.argv.includes('--write-fixture') ? 'write' : 'bytecheck' }));
