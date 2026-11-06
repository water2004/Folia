// Execute Mermaid's pinned detectDirective body, not a reimplementation.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { stripTypeScriptTypes } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';
import vm from 'node:vm';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const sourceRoot = path.join(root, '.reference/mermaid-js');
const lock = JSON.parse(readFileSync(path.join(root, 'docs/mermaid-cpp/source-lock.json'), 'utf8'));
for (const entry of lock.files) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(sourceRoot, entry.path))).digest('hex'), entry.sha256, entry.path);
}
const { directiveRegex } = await import(pathToFileURL(path.join(sourceRoot, 'packages/mermaid/src/diagram-api/regexes.ts')));
const source = readFileSync(path.join(sourceRoot, 'packages/mermaid/src/utils.ts'), 'utf8');
function extract(start, end) {
  const from = source.indexOf(start);
  const to = source.indexOf(end, from + start.length);
  assert(from >= 0 && to > from, start);
  return source.slice(from, to).replace(/^export /, '');
}
const program = stripTypeScriptTypes(
  extract('const directiveWithoutOpen =', '\n/**') + '\n' +
  extract('export const detectDirective =', '\nexport const removeDirectives')) + '\ndetectDirective';
const hex = value => Array.from({ length: value.length }, (_, i) => value.charCodeAt(i).toString(16).padStart(4, '0')).join('') || '-';
function snapshot(value) {
  if (value === undefined) return 'U';
  if (value === null) return 'N';
  if (typeof value === 'boolean') return value ? 'T' : 'F';
  if (typeof value === 'number') return `D${Object.is(value, -0) ? '-0' : String(value)};`;
  if (typeof value === 'string') return `S${hex(value)};`;
  if (Array.isArray(value)) return `[${value.map(snapshot).join('')}]`;
  return `{${Object.keys(value).sort().map(key => `S${hex(key)};${snapshot(value[key])}`).join('')}}`;
}
const cases = [];
const add = (name, input) => cases.push({ name, input });
add('empty', '');
add('none', ' \ufeffpie\r\n ');
add('quotes-outside', "  pie 'title'  ");
for (const type of ['init', 'initialize', 'wrap', 'nowrap', 'INIT', 'reinit', 'initX', 'initialize_','wraparound', '9', '_', '中文', '']) {
  for (const body of ['', ':', ':true', ': false', ':null', ':0', ':-0', ':1e3', ': {"a":1}', ": {'theme':'dark'}", ': [true,null,3]', ': "word"', ': {bad:1}', ': [1,]', ' plain words']) {
    add(`form-${cases.length}`, `%%{${type}${body}}%%\npie`);
    add(`unclosed-${cases.length}`, `%%{${type}${body}`);
  }
}
const argumentsToCheck = [
  '{}', '[]', '{"a":1,"a":2}', '{"__proto__":{"x":1},"constructor":2,"toString":3}',
  '[0,-0,0.0,-0.0,9007199254740993,1e309,-1e309,1e-999,-1e-999,5e-324,1e-324]',
  '[1.7976931348623157e308,1.7976931348623159e308,2.2250738585072014e-308,0e9999,-0e9999]',
  '[1e+2,1e-2,1E2]', '[00]', '[+1]', '[.5]', '[1.]', '[0x1]', '[NaN]', '[Infinity]', '[1_000]',
  '[1e]', '[1e+]', '[--1]', '[-01]', '[true,false,null]', '[True]', '[undefined]', '[1] trailing',
  '"\\uD800"', '"\\uDC00"', '"\\uD83D\\uDE00"', '"literal\ud800"', '"😀中文"',
  '"\\u0000\\b\\f\\n\\r\\t\\\\\\/\\\""', '"\\x41"', '"\\uZZZZ"', '"bad\nline"',
  '"a\u2028b"', '"a\u2029b"', '"it\'s"', "'single'", '"\\\'"', '"}%%"',
  '{"a":{"b":[1,{"c":false}]}}', '{"a":1,}', '[,1]', '{1:2}', '{"a" 1}',
  '{"a":}', '[1 2]', '"unclosed', '"control\u0001"', '[\u00a01]', '[\t1\r\n]',
];
for (const body of argumentsToCheck) add(`json-${cases.length}`, `%%{init: ${body}}%%`);
for (const space of [' ', '\t', '\n', '\r', '\r\n', '\u00a0', '\u1680', '\u2000', '\u2028', '\u2029', '\u202f', '\u205f', '\u3000', '\ufeff', '\u0085', '\u180e', '\u200b']) {
  add(`space-${cases.length}`, `${space}%%{${space}init${space}:${space}{"a":1}${space}}%%${space}`);
  add(`body-space-${cases.length}`, `%%{init: [1,${space}2]}%%`);
}
for (const line of ['\n', '\r', '\r\n', '\u2028', '\u2029', '']) {
  add(`comment-${cases.length}`, `pie %%}%% 'comment' %%{init: {bad}}%%${line}%%{wrap}%%`);
  add(`ordinary-comment-${cases.length}`, `%% comment %%{init: {"a":1}}%%${line}pie`);
}
add('multiple', '%%{wrap}%% %%{init:{"a":1}}%% %%{initialize:{"b":2}}%% %%{nowrap}%%');
add('failure-discards-earlier', '%%{wrap}%% %%{init:{bad}}%% %%{wrap}%%');
add('filter-skips-bad-json', '%%{other:{bad}}%% %%{init:{"a":1}}%% %%{wrap}%%');
add('nested-delimiter', '%%{init:{"x":"%%{wrap}%%"}}%%');
add('fallback-trim-before-comment', "\n%%}%%comment\n  pie 'title'\n");
let seed = 0x941723;
const random = () => (seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0);
const pieces = ['%%{', '}%%', '%%}%%', 'init', 'wrap', 'nowrap', 'initialize', ':', ' ', '\n', '\r', '\r\n', '\u2028', '\u2029', '\ufeff', '"', "'", 'x', '_', '9', '{', '}', '[', ']', ',', '\ud800'];
for (let i = 0; i < 600; ++i) {
  let input = '';
  const length = 1 + random() % 35;
  for (let j = 0; j < length; ++j) input += pieces[random() % pieces.length];
  add(`generated-${i}`, input);
}
const filters = { all: null, init: /(?:init\b)|(?:initialize\b)/, wrap: 'wrap' };
const lines = [];
for (const { name, input } of cases) for (const [filter, pattern] of Object.entries(filters)) {
  // Each observation models an independent invocation. An upstream exception can
  // otherwise leave the shared global regex's lastIndex set for the next call.
  const detect = vm.runInNewContext(program, {
    directiveRegex: new RegExp(directiveRegex.source, directiveRegex.flags),
    log: { debug() {}, error() {} },
  });
  const result = detect(input, pattern);
  const entries = Array.isArray(result) ? result : [result];
  lines.push([name, filter, hex(input), entries.map(d => `${snapshot(d.type)}${snapshot(d.args)}`).join('|')].join('\t'));
}
const expected = '# Mermaid 12.2.0 detectDirective, isolated calls; name/filter/UTF-16 input/typed results\n' + lines.join('\n') + '\n';
const output = path.join(root, 'tests/mermaid_cpp/fixtures/directives.tsv');
if (process.argv.includes('--write')) writeFileSync(output, expected);
else assert.equal(readFileSync(output, 'utf8').replaceAll('\r\n', '\n'), expected);
console.log(JSON.stringify({ source: lock.commit, inputs: cases.length, observations: lines.length, mode: process.argv.includes('--write') ? 'write' : 'check' }));
