// Execute the exact pinned function bodies, isolated from unrelated DOM/config imports.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { stripTypeScriptTypes } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import vm from 'node:vm';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const sourceRoot = path.join(root, '.reference/mermaid-js');
const lock = JSON.parse(readFileSync(path.join(root, 'docs/mermaid-cpp/source-lock.json'), 'utf8'));
for (const entry of lock.files) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(sourceRoot, entry.path))).digest('hex'),
    entry.sha256, `Changed official source: ${entry.path}`);
}
const { directiveRegex } = await import(pathToFileURL(path.join(sourceRoot,
  'packages/mermaid/src/diagram-api/regexes.ts')));
const preprocess = readFileSync(path.join(sourceRoot, 'packages/mermaid/src/preprocess.ts'), 'utf8');
const utils = readFileSync(path.join(sourceRoot, 'packages/mermaid/src/utils.ts'), 'utf8');
function extract(source, start, end) {
  const from = source.indexOf(start);
  const to = source.indexOf(end, from + start.length);
  assert(from >= 0 && to > from, `Missing official function: ${start}`);
  return source.slice(from, to);
}
const cleanupSource = extract(preprocess, 'const cleanupText =', '\nconst processFrontmatter');
const removeSource = extract(utils, 'export const removeDirectives =', '\n/**').replace(/^export /, '');
const { cleanupText, removeDirectives } = vm.runInNewContext(
  stripTypeScriptTypes(cleanupSource + '\n' + removeSource) + '\n({cleanupText, removeDirectives})',
  { directiveRegex });

const cases = [];
const add = (name, input) => cases.push({ name, input });
add('empty', '');
add('line-endings', 'a\rb\r\nc\n\r\r\nd\u2028e\u2029f');
add('attributes', '<a x="one" y="two">text</a><br title="x\ny"/>');
add('attribute-spaces', '<x a = "keep" b= "keep" c="change" d=\'keep\'>');
add('quoted-greater-than', '<a x="a>b" y="c">');
add('nested-tag', '<a x="<b y="z">');
add('closing-tags', '</a x="keep"><!a x="keep"><?a x="keep">');
add('unicode-tags', '<é x="keep"><_ x="change"><9 x="change"><a-b x="change">');
add('dollar-attribute', '<a x="$&$1$$\'\\">');
add('unclosed-tags', '<a x="x" <b <c');
add('unclosed-quote', '<a x="z> <b y="ok">');
add('surrogates', '\ud800<a x="\udfff">\ud83d\ude00\r');
for (const name of ['init', 'initialize', 'wrap', 'nowrap', 'INIT', '_', '9', 'é', '']) {
  for (const body of ['', ':', ': true', ': {"theme":"dark"}', ' hello world', ':\rraw', ':\u2028raw']) {
    for (const end of ['}%%', '', '}%%%tail']) {
      add(`directive-${cases.length}`, `before %%{${name}${body}${end}\nafter`);
    }
  }
}
for (const space of ['\t', '\n', '\r\n', '\r', ' ', '\u00a0', '\u1680', '\u2000',
  '\u2028', '\u2029', '\u202f', '\u205f', '\u3000', '\ufeff', '\u0085', '\u180e', '\u200b']) {
  add(`space-${cases.length}`, `%%{${space}init${space}:${space}{"x":1}${space}}%%\npacket`);
}
add('multiple', '%%{wrap}%% A %%{init: {"theme":"dark"}}%% B %%{nowrap}%%');
add('nested-directive', '%%{init: {"x":"%%{wrap}%%"}}%%');
add('directives-in-comments', '%% comment %%{wrap}%%\n%%{init: {} }%%\npie');
let seed = 0x35109;
const random = () => (seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0);
const pieces = ['%%{', '}%%', '%%', 'init', 'wrap', ':', ' ', '\n', '\r', '\r\n',
  '\u2028', '\u2029', '\ufeff', '<a', '>', '="', '"', "'", 'x', '_', '9', '{', '}', '\ud800'];
for (let i = 0; i < 1000; ++i) {
  let input = '';
  const length = 1 + random() % 35;
  for (let j = 0; j < length; ++j) input += pieces[random() % pieces.length];
  add(`generated-${i}`, input);
}
const hex = text => Array.from({ length: text.length }, (_, i) =>
  text.charCodeAt(i).toString(16).padStart(4, '0')).join('') || '-';
const lines = cases.map(({ name, input }) => [name, hex(input), hex(cleanupText(input)),
  hex(removeDirectives(input)), hex(removeDirectives(cleanupText(input)))].join('\t'));
const expected = '# Mermaid 12.2.0 cleanupText/removeDirectives; UTF-16 hex fields\n' + lines.join('\n') + '\n';
const output = path.join(root, 'tests/mermaid_cpp/fixtures/preprocess-text.tsv');
if (process.argv.includes('--write')) writeFileSync(output, expected);
else assert.equal(readFileSync(output, 'utf8').replaceAll('\r\n', '\n'), expected);
console.log(JSON.stringify({ source: lock.commit, cases: cases.length, mode: process.argv.includes('--write') ? 'write' : 'check' }));
