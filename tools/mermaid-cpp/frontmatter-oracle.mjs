// Execute the pinned original extractor and its exact locked YAML schema.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { stripTypeScriptTypes } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';
import vm from 'node:vm';
import yaml from './oracle/node_modules/js-yaml/index.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const sourceRoot = path.join(root, '.reference/mermaid-js');
const lock = JSON.parse(readFileSync(path.join(root, 'docs/mermaid-cpp/source-lock.json'), 'utf8'));
for (const entry of lock.files) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(sourceRoot, entry.path))).digest('hex'), entry.sha256, entry.path);
}
assert.equal(JSON.parse(readFileSync(new URL('./oracle/node_modules/js-yaml/package.json', import.meta.url))).version, '4.3.0');
const { matchFrontMatter } = await import(pathToFileURL(path.join(sourceRoot, 'packages/mermaid/src/diagram-api/regexes.ts')));
const source = readFileSync(path.join(sourceRoot, 'packages/mermaid/src/diagram-api/frontmatter.ts'), 'utf8');
const body = source.slice(source.indexOf('export function extractFrontMatter')).replace(/^export /, '');
const extractFrontMatter = vm.runInNewContext(stripTypeScriptTypes(body) + '\nextractFrontMatter', { yaml, matchFrontMatter });
const hex = value => Array.from({ length: value.length }, (_, i) => value.charCodeAt(i).toString(16).padStart(4, '0')).join('') || '-';
// Typed snapshots retain NaN, infinities, negative zero and every UTF-16 unit.
function snapshot(value) {
  if (value === null) return 'N';
  if (typeof value === 'boolean') return value ? 'T' : 'F';
  if (typeof value === 'number') return `D${Object.is(value, -0) ? '-0' : String(value)};`;
  if (typeof value === 'string') return `S${hex(value)};`;
  if (Array.isArray(value)) return `[${value.map(snapshot).join('')}]`;
  return `{${Object.keys(value).sort().map(key => `S${hex(key)};${snapshot(value[key])}`).join('')}}`;
}
const cases = [];
const add = (name, input) => cases.push({ name, input });
const bodyCase = (name, body) => add(name, `---\n${body}\n---\npie\n`);
add('absent', 'pie\n');
add('unclosed', '---\ntitle: retained');
add('tab-dedent', '\t---\n\ttitle: tabbed\n\tconfig:\n\t  pie: {showData: true}\n\t---\npie');
add('mixed-dedent', '  ---\n  title: indented\nconfig: {theme: dark}\n  ---\npie');
add('crlf', '---\r\ntitle: crlf\r\n---\r\npie');
add('bom', '\ufeff---\ntitle: bom\n---\npie');
const scalars = ['', 'null', 'Null', 'NULL', '~', 'true', 'True', 'TRUE', 'false', 'False', 'FALSE',
  'yes', 'on', 'off', 'tRuE', '0', '-0', '+0', '00', '012', '-12', '+12', '0xFF', '-0b101', '0o17',
  '0x', '0XFF', '1_000', '1:20', '1.25', '.5', '-.5', '+.5', '1.', '1e3', '-1e-7', '1e309',
  '1e-999', '.inf', '-.Inf', '+.INF', '.nan', '.NaN', '-.nan', '2026-10-21', '"true"', "'12'",
  '""', "'it''s quoted'", '"escaped\\nline\\tend"', '"\\u4e2d\\U0001f600"',
  '[]', '{}', '[1, null, false, [2, 3], {x: 4}]', '{a: 1}', '{toString: false}',
  '!!str true', '!!bool TRUE', '!!int 0xff', '!!float 1', '!!null null', '!!int nope',
  '!!timestamp 2026-10-21', '!custom value', '!!binary QQ==', '!!map {}', '!!seq []',
  '!!str', '!!null', '!!bool', '!!int', '!!float', '!!seq', '!!map',
  '9007199254740993', '9007199254740995', '0x20000000000001', '0x20000000000003',
  '0o400000000000000001', '-00', '-0.0', '2.2250738585072014e-308', '5e-324', '1e-324',
  '1.7976931348623157e308', '1.7976931348623159e308', '"\\0"', '"\\u0001"',
  '"\\uD800"', '"\\uDC00"', '"\\uD83D\\uDE00"', '"\\U00110000"',
  '"\\N\\_\\L\\P"', '"\\a\\b\\v\\f\\e"', '"bad\\q"', '"bad\\\'"',
  '!!seq ""', '!!map ""', '!!seq null', '!!map null'];
for (const [i, value] of scalars.entries()) {
  bodyCase(`scalar-${i}`, `config: {value: ${value}}`);
  bodyCase(`title-${i}`, `title: ${value}\ndisplayMode: ${value}`);
  bodyCase(`config-${i}`, `config: ${value}`);
}
for (const [name, body] of Object.entries({
  empty: '', rootNull: 'null', rootSequence: '[1, 2]', rootString: 'plain',
  nested: 'title: Diagram\ndisplayMode: compact\nconfig:\n  theme: dark\n  pie:\n    showData: true\n    textPosition: 0.75\n  colors: [red, "#fff", null]',
  literal: 'title: |\n  first\n  second\nconfig: {theme: dark}',
  folded: 'title: >-\n  first\n  second', literalKeep: 'title: |+\n  first\n\n',
  flow: 'config: {theme: dark, pie: {showData: true}, list: [a, b,]}',
  alias: 'base: &base {theme: dark, sizes: [1, 2]}\nconfig: *base',
  aliasScalar: 'base: &base title\ntitle: *base',
  mergeIsString: 'base: &base {theme: dark}\nconfig: {<<: *base, theme: default}',
  duplicate: 'title: one\ntitle: two', duplicateCoerced: 'config: {true: 1, "true": 2}',
  duplicateNested: 'config: {x: {a: 1, a: 2}}', missingAlias: 'config: *missing',
  invalidFlow: 'config: [1, 2', invalidIndent: 'config:\n  x: 1\n y: 2',
  keyTypes: 'config: {null: 1, true: 2, 12: 3, .nan: 4, .inf: 5}',
  sequenceKey: 'config: { ? [a, null, true] : value }',
  objectKey: 'config: { ? {x: 1} : value }', nestedKey: 'config: { ? [[x]] : value }',
  proto: 'config: {__proto__: {theme: dark}, constructor: 1, toString: 2}',
  ignored: 'unknown: [1, 2]\ntitle: known', comment: '# comment\ntitle: value # trailing',
  tagMismatch: 'config: !!seq {}', tabs: 'config:\n\tx: 1',
  multiple: 'title: first\n...\n---\ntitle: second',
  emptyStringAtEnd: 'config: !!str', emptySeqAtEnd: 'config: !!seq', emptyMapAtEnd: 'config: !!map',
  nonSpecific: 'config: ! 12', reusedAnchor: 'a: &x first\nb: &x second\ntitle: *x',
  nul: 'title: before\u0000after', unicodeSeparators: 'title: a\u0085b\u2028c\u2029d',
})) bodyCase(name, body);
// Regression: non-verbatim tag suffixes stop at whitespace, not flow
// punctuation. Empty nodes with a separator are valid; attached punctuation
// is invalid. Keep both outcomes so a blanket collection rejection cannot pass.
for (const tag of ['!!str', '!!null', '!!seq', '!!map', '!']) {
  for (const [separatorName, separator] of Object.entries({space: ' ', tab: '\t', lf: '\n', crlf: '\r\n'})) {
    bodyCase(`empty-tag-map-${tag}-${separatorName}`, `config: {value: ${tag}${separator}}`);
    bodyCase(`empty-tag-sequence-${tag}-${separatorName}`, `config: [${tag}${separator}]`);
    bodyCase(`empty-tag-comma-${tag}-${separatorName}`, `config: {value: ${tag}${separator}, other: yes}`);
  }
  for (const [name, body] of Object.entries({map: `config: {value: ${tag}}`, sequence: `config: [${tag}]`, comma: `config: [${tag}, 1]`})) {
    bodyCase(`attached-tag-${tag}-${name}`, body);
  }
}
for (const type of ['str', 'null', 'seq', 'map']) {
  bodyCase(`verbatim-empty-${type}`, `config: {value: !<tag:yaml.org,2002:${type}>}`);
  bodyCase(`verbatim-sequence-${type}`, `config: [!<tag:yaml.org,2002:${type}>, 1]`);
}
bodyCase('empty-tag-nested', 'config: {a: [!!str , {b: !!seq }], c: !!map }');
bodyCase('empty-tag-anchor', 'config: {a: &empty !!seq , b: *empty}');
bodyCase('empty-tag-schema-error', 'config: {a: !!int }');
bodyCase('adjacent-tag-collection', 'config: !!seq[]');
bodyCase('separated-tag-collection', 'config: !!seq []');
for (let i = 0; i < 80; ++i) bodyCase(`generated-${i}`, `title: ${i}\nconfig: {pie: {showData: ${i % 2 ? 'true' : 'false'}, width: ${i * 17}}, values: [${i}, "${i}"]}`);
const lines = cases.map(({ name, input }) => {
  try {
    const result = extractFrontMatter(input);
    // Explicit native safety boundary: the grammar backend rejects surrogate
    // escapes (even a pair) and out-of-range Unicode escapes. Retain the actual
    // official result so this divergence cannot be mistaken for source parity.
    const nativeUnicodeBoundary = /\\uD[89A-Fa-f][0-9A-Fa-f]{2}|\\U00110000/.test(input);
    return [name, hex(input), nativeUnicodeBoundary ? 'unicode-boundary' : 'ok', hex(result.text), snapshot(result.metadata)].join('\t');
  } catch {
    return [name, hex(input), 'error', '-', '-'].join('\t');
  }
});
const output = path.join(root, 'tests/mermaid_cpp/fixtures/frontmatter.tsv');
const expected = '# Original extractFrontMatter + js-yaml 4.3.0 JSON_SCHEMA; typed UTF-16 snapshots\n' + lines.join('\n') + '\n';
if (process.argv.includes('--write')) writeFileSync(output, expected);
else assert.equal(readFileSync(output, 'utf8').replaceAll('\r\n', '\n'), expected);
console.log(JSON.stringify({ cases: cases.length, source: lock.commit, mode: process.argv.includes('--write') ? 'write' : 'check' }));
