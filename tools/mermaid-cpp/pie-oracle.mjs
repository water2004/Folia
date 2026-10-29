// Observe the actual pinned Mermaid parser and Pie DB. No lexer, parser,
// duplicate handling, or DB implementation is substituted in this script.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arguments_ = process.argv.slice(2);
assert(arguments_.every(value => value === '--write' || value === '--check') &&
  !(arguments_.includes('--write') && arguments_.includes('--check')),
  'Usage: node tools/mermaid-cpp/pie-oracle.mjs [--write|--check]');
const sourceRoot = path.join(root, '.reference/mermaid-js');
const packageRoot = path.join(root, 'tools/mermaid-cpp/oracle');
const modules = path.join(packageRoot, 'node_modules');
const commit = '1fb7869866dd619168d840291c9c54b4f129758b';
const lock = JSON.parse(readFileSync(path.join(root, 'docs/mermaid-cpp/source-lock.json'), 'utf8'));
assert.equal(lock.commit, commit);
assert.equal(execFileSync('git', ['-C', sourceRoot, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim(),
  commit, 'The reference checkout is not the pinned Mermaid commit.');
const pieSources = [
  ['packages/parser/src/language/pie/pie.langium', 'fe9346e70f5d5028fa5aaa0470ba651e445c904e615feaf79a4eb2b5a401118d'],
  ['packages/parser/src/language/pie/valueConverter.ts', 'd29b30fd73f0a1d9e1da7cea12eae1a13194bee4c886ace4b212e800857b01a8'],
  ['packages/parser/src/language/pie/tokenBuilder.ts', 'bb664434307f4417c451dee98beee2788a52a8689e25e89e022cb59e1e3aad4a'],
  ['packages/parser/src/language/pie/module.ts', '6ff1bcd758ab6e23f98cc94b842cabd82fd074f44574bfacbd9167f0437ad5cf'],
  ['packages/mermaid/src/diagrams/pie/pieParser.ts', 'd6f05e3845ec051d07a12d16f45e4ed2d44c0deb0022561582ada77ad4e0b8a4'],
  ['packages/mermaid/src/diagrams/pie/pieDb.ts', 'e64377c8d3e0501a859b46e329ca1aacd2e205faa0aaed203be012324f868471'],
  ['packages/mermaid/src/diagrams/pie/pieTypes.ts', 'd4b451e65c4ae18301583853ac60ba7e25ff8fec01b3abfd1fb90bf3760f31ba'],
  ['packages/mermaid/src/diagrams/common/populateCommonDb.ts', '5d2a306522f65f8672a42dbec11b060818f8c9d3d07a7ae9328d3a55a8050856'],
  ['packages/mermaid/src/diagrams/common/commonDb.ts', '40579cae9a11c8267eea337388892f2d5b508c29b4e77458f2b37bddd45a5f0e'],
];
for (const entry of [...lock.files, ...pieSources.map(([path, sha256]) => ({ path, sha256 }))]) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(sourceRoot, entry.path))).digest('hex'),
    entry.sha256, `Changed official source: ${entry.path}`);
}
const packageLock = readFileSync(path.join(packageRoot, 'pnpm-lock.yaml'), 'utf8');
for (const [name, version] of Object.entries({ '@mermaid-js/parser': '2.0.1', mermaid: '12.2.0', jsdom: '26.1.0' })) {
  assert.equal(JSON.parse(readFileSync(path.join(modules, name, 'package.json'), 'utf8')).version, version);
  assert(packageLock.includes(`${name}@${version}:`) || packageLock.includes(`${name}@${version}':`),
    `The frozen pnpm lock does not pin ${name}@${version}.`);
}
const load = relative => import(pathToFileURL(path.join(modules, relative)));
const { parse } = await load('@mermaid-js/parser/dist/mermaid-parser.core.mjs');
const { JSDOM } = await load('jsdom/lib/api.js');
const dom = new JSDOM('<!doctype html><html><body></body></html>', { pretendToBeVisual: true });
globalThis.window = dom.window;
globalThis.document = dom.window.document;
const { default: mermaid } = await load('mermaid/dist/mermaid.core.mjs');

const cases = [];
const add = (id, source, compareDb = true) => {
  assert(!cases.some(item => item.id === id), `Duplicate observation name: ${id}`);
  cases.push({ id, source, compareDb });
};
add('empty-source', '');
add('whitespace-source', ' \t\r\n');
add('empty-pie', 'pie');
add('empty-show-data', 'pie showData');
add('indented-header', '\t pie \t\n');
add('header-wrong-case', 'Pie\n"x":1');
add('wrong-diagram', 'notPie\n"x":1');
add('header-boundary', 'piex\n"x":1');
add('show-data-boundary', 'pie showDatax\n"x":1');
add('show-data-wrong-case', 'pie showdata\n"x":1');
add('repeated-show-data', 'pie showData showData\n"x":1');
add('show-data-after-newline', 'pie\nshowData\n"x":1');
add('header-same-line', 'pie "x":1');
add('show-data-same-line', 'pie showData "x":1');
add('no-space-before-label', 'pie"x":1');
add('show-data-no-space-before-label', 'pie showData"x":1');
add('basic', 'pie\n"x":1\n"y":2\n');
add('show-data', 'pie showData\n"x":1\n"y":2');
add('crlf', 'pie\r\n"x":1\r\n"y":2\r\n');
add('trailing-lf', 'pie\n"x":1\n\n');
add('trailing-cr', 'pie\n"x":1\r');
add('blank-lines', '\n\npie\n\n"x":1\n\n"y":2\n');
add('horizontal-spacing', 'pie\n\t "x" \t: \t1 \t\n');
for (const [id, number] of Object.entries({
  zero: '0', negative_zero: '-0', decimal: '1.25', decimal_zero: '0.0',
  negative_decimal_zero: '-0.0', leading_decimal_zeroes: '0001.250',
  single_leading_decimal_zero: '01.5', negative_decimal_leading_zeroes: '-00.5',
  negative: '-1', negative_fraction: '-0.25', negative_leading_zeroes: '-0001.25',
  integer_leading_zero: '01', negative_integer_leading_zero: '-01',
  leading_dot: '.25', negative_leading_dot: '-.25', trailing_dot: '1.', double_dot: '1..2',
  positive_sign: '+1', exponent: '1e3', negative_exponent: '1e-3',
  hex: '0x10', comma: '1,2', doubled_sign: '--1', spaced_sign: '- 1',
  infinity_keyword: 'Infinity', nan_keyword: 'NaN',
  safe_integer_limit: '9007199254740991', rounded_integer: '9007199254740993',
  shortest_integer: '1000000000000000128', exponential_integer: '1000000000000000000000',
  negative_shortest_integer: '-1000000000000000128',
  negative_exponential_integer: '-1000000000000000000000',
  underflow: '0.' + '0'.repeat(400) + '1',
  negative_underflow: '-0.' + '0'.repeat(400) + '1',
  subnormal_round_down: '0.' + '0'.repeat(323) + '2',
  subnormal_round_up: '0.' + '0'.repeat(323) + '3',
  minimum_subnormal: '0.' + '0'.repeat(323) + '5',
  negative_minimum_subnormal: '-0.' + '0'.repeat(323) + '5',
  infinity_digits: '9'.repeat(400), negative_infinity_digits: '-' + '9'.repeat(400),
  infinity_decimal: '9'.repeat(400) + '.5',
})) add(`number-${id}`, `pie\n"x":${number}`);
for (const [id, body] of Object.entries({
  missing_label: ':1', unquoted_label: 'x:1', missing_colon: '"x" 1',
  missing_value: '"x":', extra_colon: '"x"::1', unclosed_quote: '"x:1',
  semicolon: '"x":1;', two_sections_one_line: '"x":1 "y":2',
  garbage_after_value: '"x":1 trailing', garbage_before_label: 'oops "x":1',
})) add(`malformed-${id}`, `pie\n${body}`);
add('empty-label', 'pie\n"":1');
add('single-quoted-label', "pie\n'single':1");
add('label-whitespace', 'pie\n"  x \t ":1');
add('unicode-label', 'pie\n"中文 Ελληνικά 😀 e\u0301":1');
add('duplicate-label', 'pie\n"x":1\n"y":2\n"x":3');
add('duplicate-zero-first', 'pie\n"x":0\n"x":3');
add('duplicate-negative-zero-first', 'pie\n"x":-0\n"x":3');
add('duplicate-negative-later', 'pie\n"x":1\n"x":-3');
add('duplicate-negative-first', 'pie\n"x":-1\n"x":3');
add('case-sensitive-labels', 'pie\n"x":1\n"X":2');
add('prototype-labels', 'pie\n"__proto__":1\n"constructor":2\n"__proto__":3');
add('unicode-normalization-labels', 'pie\n"é":1\n"e\u0301":2');
add('escape-quote', 'pie\n"a\\\"b":1', false);
add('escape-backslash', 'pie\n"a\\\\b":1', false);
add('escape-known', 'pie\n"\\b\\f\\n\\r\\t\\v\\0":1', false);
add('escape-unknown', 'pie\n"\\u0041\\x42\\q\\/":1', false);
add('escape-duplicate', 'pie\n"a\\nb":1\n"a\nb":2', false);
add('raw-multiline-label', 'pie\n"a\nb":1', false);
add('raw-crlf-label', 'pie\n"a\r\nb":1', false);
add('escaped-lf', 'pie\n"a\\\nb":1', false);
add('escaped-crlf', 'pie\n"a\\\r\nb":1', false);
add('escaped-ls', 'pie\n"a\\\u2028b":1', false);
add('lone-surrogate-label', 'pie\n"\ud800x\udfff":1', false);
add('entity-label', 'pie\n"#35; &amp; &#65;":1', false);
add('html-label', 'pie\n"<b>bold</b><br/>next":1', false);
add('inline-comment-marker-in-label', 'pie\n"a%%b":1', false);
add('title', 'pie\ntitle A chart\n"x":1');
add('empty-title', 'pie\ntitle\n"x":1');
add('title-at-eof', 'pie\ntitle A chart');
add('title-no-boundary', 'pie\ntitlefoo\n"x":1');
add('repeated-title', 'pie\ntitle First\ntitle Second\n"x":1');
add('metadata-between-sections', 'pie\n"x":1\ntitle Between\n"y":2');
add('accessibility', 'pie\naccTitle: chart title\naccDescr: chart description\n"x":1');
add('empty-accessibility', 'pie\naccTitle:\naccDescr:\n"x":1');
add('repeated-accessibility', 'pie\naccTitle: First\naccTitle: Second\naccDescr: First\naccDescr: Second\n"x":1');
add('metadata-horizontal-space', 'pie\n\t title  spaced\t\t title \t\naccTitle:\t A  B\t\n"x":1');
add('metadata-comments', 'pie\ntitle A %% comment\naccTitle: B %% comment\naccDescr: C %% comment\n"x":1', false);
add('multiline-description', 'pie\naccDescr {\n  first\n\n\t second  word\n}\n"x":1');
add('multiline-description-crlf', 'pie\naccDescr { a\r\nb }\n"x":1', false);
add('multiline-description-ls', 'pie\naccDescr { a\u2028 b\u2029 c }\n"x":1', false);
add('description-whitespace-before-brace', 'pie\naccDescr\n\t{ first }\n"x":1');
add('description-missing-brace', 'pie\naccDescr { first\n"x":1');
add('metadata-html', 'pie\ntitle <b>Bold</b>\naccDescr: <em>description</em>\n"x":1', false);
for (const [id, source] of Object.entries({
  comment_before_header: '%% a comment\npie\n"x":1',
  comment_after_header: 'pie\n%% a comment\n"x":1',
  inline_comment: 'pie\n"x":1 %% comment\n"y":2',
  empty_comment_marker: 'pie\n%%\n"x":1',
  comment_without_final_newline: 'pie\n"x":1\n%% ending',
  yaml_before_header: '---\ntitle: Frontmatter\n---\npie\n"x":1',
  empty_yaml_before_header: '---\n---\npie\n"x":1',
  yaml_inside_grammar: 'pie\n---\nignored: true\n---\n"x":1',
  yaml_indented_opening: 'pie\n \t---\nignored: true\n---\n"x":1',
  yaml_close_at_eof: 'pie\n---\nignored: true\n---',
  yaml_wrong_close_indent: 'pie\n---\nx\n ---\n"x":1',
  yaml_close_boundary: 'pie\n---\nx\n---x\n"x":1',
  yaml_missing_close: 'pie\n---\nx\n"x":1',
  directive_before_header: '%%{init:{"pie":{"textPosition":0.5}}}%%\npie\n"x":1',
  directive_multiline: 'pie\n%%{\nopaque text\n}%%\n"x":1',
  directive_eats_newline: 'pie\n"x":1 %%{opaque}%%\n"y":2',
  directive_after_title: 'pie\ntitle X %%{opaque}%%\n"x":1',
  directive_close_boundary: 'pie\n%%{opaque}%%x\n"x":1',
  directive_close_at_eof: 'pie\n%%{opaque}%%',
  directive_unclosed_is_comment: 'pie\n%%{opaque\n"x":1',
})) add(id, source, false);
const spaces = ['\t', '\n', '\v', '\f', '\r', ' ', '\u00a0', '\u1680',
  ...Array.from({ length: 11 }, (_, index) => String.fromCharCode(0x2000 + index)),
  '\u2028', '\u2029', '\u202f', '\u205f', '\u3000', '\ufeff', '\u0085', '\u180e', '\u200b', '\u2060'];
spaces.forEach((space, index) => {
  add(`space-${index}-header`, `pie${space}"x":1`, false);
  add(`space-${index}-description`, `pie\naccDescr${space}{ a${space}b }\n"x":1`, false);
});
let seed = 0x71ec0de;
const random = () => (seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0);
for (let index = 0; index < 72; ++index) {
  let source = index % 2 ? 'pie showData\n' : 'pie\n';
  if (index % 3 === 0) source += `title Generated ${index}\naccDescr { stable generated chart }\n`;
  const sections = 1 + random() % 10;
  for (let section = 0; section < sections; ++section) {
    const label = `slice ${random() % 5} Ω`;
    const integer = random() % 100;
    const value = index % 11 === 0 && section === sections - 1 ? `-${1 + integer}` :
      `${integer}${random() % 2 ? '.25' : ''}`;
    source += `"${label}":${value}\n`;
  }
  add(`generated-valid-${index}`, source);
}
for (let index = 0; index < 24; ++index) {
  const value = 1 + random() % 100;
  const variants = [`"x" ${value}`, `"x":${value} "y":2`, `"x":+${value}`,
    `"x":${value};`, `"x":0${value}`, `x:${value}`];
  add(`generated-invalid-${index}`, `pie\n${variants[index % variants.length]}\n`);
}

// UTF-16 code-unit hex preserves lone surrogates and embedded NUL. Numeric -0
// is spelled separately because JS String(-0) would erase the observed sign.
const hex = text => Array.from({ length: text.length }, (_, index) =>
  text.charCodeAt(index).toString(16).padStart(4, '0')).join('') || '-';
const number = value => Object.is(value, -0) ? '-0' : String(value);
const boolean = value => value ? '1' : '0';
const row = values => values.join('\t');
const message = error => error instanceof Error ? error.message : String(error);
const lines = [
  '# Mermaid Pie observations v1; strings are UTF-16 hex; - means empty/absent.',
  `# Official source ${commit}; mermaid@12.2.0; @mermaid-js/parser@2.0.1; jsdom@26.1.0.`,
  '# case: id source raw_ok native_finite title acc_title acc_descr show_data db_ok db_error',
  '# section/db_section/full_section: id label value. Numeric -0 preserves its sign.',
  '# raw_error: id message. Native syntax diagnostic wording is not an equality contract.',
  '# full_case: id ok title acc_title acc_descr show_data error.',
  '# case/section observe RAW grammar. db_section observes the REAL Mermaid API DB only for explicitly comparable inputs.',
  '# db_ok=- excludes cross-stage comparison; full_case/full_section retain the separate preprocessing/sanitization observation.',
  '# native_finite=0 marks the deliberate native rejection of nonfinite values accepted by the official raw grammar.',
  '# No render or synthetic DOM measurement is performed; this fixture establishes no SVG or font-metric parity.',
];
const totals = { cases: cases.length, rawSuccess: 0, rawFailure: 0, nativeSafetyRejections: 0,
  dbSuccess: 0, dbFailure: 0, dbExcluded: 0, fullSuccess: 0, fullFailure: 0 };
for (const { id, source, compareDb } of cases) {
  let ast;
  let rawError = '';
  try { ast = await parse('pie', source); ++totals.rawSuccess; }
  catch (error) { rawError = message(error); ++totals.rawFailure; }
  mermaid.initialize({ startOnLoad: false, securityLevel: 'strict' });
  let full;
  let fullError = '';
  try {
    const diagram = await mermaid.mermaidAPI.getDiagramFromText(source);
    assert.equal(diagram.type, 'pie');
    full = { title: diagram.db.getDiagramTitle(), accTitle: diagram.db.getAccTitle(),
      accDescr: diagram.db.getAccDescription(), showData: diagram.db.getShowData(),
      sections: Array.from(diagram.db.getSections(), ([label, value]) => ({ label, value })) };
  } catch (error) { fullError = message(error); }
  ++totals[full ? 'fullSuccess' : 'fullFailure'];
  const finite = !ast || ast.sections.every(section => Number.isFinite(section.value));
  if (!finite) ++totals.nativeSafetyRejections;
  const compare = Boolean(ast) && finite && compareDb;
  if (compare && full) {
    for (const key of ['title', 'accTitle', 'accDescr']) {
      assert.equal(full[key], ast[key] ?? '', `${id}: preprocessing changed ${key}; isolate these stages.`);
    }
    assert.equal(full.showData, ast.showData);
    for (const section of full.sections) {
      assert(ast.sections.some(raw => raw.label === section.label && Object.is(raw.value, section.value)),
        `${id}: preprocessing changed a section; isolate these stages.`);
    }
  }
  if (compare) ++totals[full ? 'dbSuccess' : 'dbFailure'];
  else ++totals.dbExcluded;
  lines.push(row(['case', id, hex(source), boolean(ast), boolean(finite), hex(ast?.title ?? ''),
    hex(ast?.accTitle ?? ''), hex(ast?.accDescr ?? ''), boolean(ast?.showData),
    compare ? boolean(full) : '-', hex(compare && !full ? fullError : '')]));
  for (const section of ast?.sections ?? []) lines.push(row(['section', id, hex(section.label), number(section.value)]));
  if (rawError) lines.push(row(['raw_error', id, hex(rawError)]));
  if (compare && full) for (const section of full.sections) {
    lines.push(row(['db_section', id, hex(section.label), number(section.value)]));
  }
  lines.push(row(['full_case', id, boolean(full), hex(full?.title ?? ''), hex(full?.accTitle ?? ''),
    hex(full?.accDescr ?? ''), boolean(full?.showData), hex(fullError)]));
  for (const section of full?.sections ?? []) lines.push(row(['full_section', id, hex(section.label), number(section.value)]));
}
const expected = lines.join('\n') + '\n';
const output = path.join(root, 'tests/mermaid_cpp/fixtures/pie.tsv');
if (arguments_.includes('--write')) writeFileSync(output, expected);
else assert.equal(readFileSync(output, 'utf8').replaceAll('\r\n', '\n'), expected,
  'Pie observations changed; inspect the pinned packages and regenerate explicitly.');
dom.window.close();
console.log(JSON.stringify({ source: commit, ...totals, mode: arguments_.includes('--write') ? 'write' : 'check' }));
