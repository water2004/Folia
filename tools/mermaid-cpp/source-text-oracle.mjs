// Runs the pinned, unmodified Mermaid TypeScript functions. No native code is
// imported when producing expected results.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const lock = JSON.parse(readFileSync(path.join(root, 'docs/mermaid-cpp/source-lock.json'), 'utf8'));
const sourceRoot = path.join(root, '.reference/mermaid-js');
for (const entry of lock.files) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(sourceRoot, entry.path))).digest('hex'),
    entry.sha256, `Changed official source: ${entry.path}`);
}
const { matchFrontMatter, stripAnyComments } = await import(pathToFileURL(path.join(sourceRoot,
  'packages/mermaid/src/diagram-api/regexes.ts')));
const { cleanupComments } = await import(pathToFileURL(path.join(sourceRoot,
  'packages/mermaid/src/diagram-api/comments.ts')));

const cases = [];
const add = (name, input) => cases.push({ name, input });
add('empty', '');
add('ordinary', 'flowchart TD\n  A[Start] --> B[Finish]\n');
add('frontmatter', '---\ntitle: Hello\n---\ngraph TD\n');
add('frontmatter-crlf', '---\r\ntitle: CRLF\r\n---\r\ngraph TD\n');
add('indented', '  ---\n  title: indented\n  ---\ngraph TD\n');
add('scalar-fence', '---\ntitle: |\n  ---\n  body\n---\ngraph TD\n');
add('greedy-opening', '---\n\n---\n\nMORE\n---\n');
add('empty-body', '---\n\n\n---\ngraph TD\n');
add('unclosed', '---\ntitle: unclosed\ngraph TD\n');
add('no-close-newline', '---\ntitle: no newline\n---');
add('different-indent', '  ---\nbody\n---\n');
add('four-dashes', '----\nbody\n----\n');
add('not-at-start', 'graph TD\n---\nbody\n---\n');
add('astral-offset', '---\ntitle: \ud83d\ude00\n---\npacket\n');
add('unpaired-surrogates', '---\ntitle: \ud800x\udfff\n---\n%%\ud800\n');
add('inline-comment', 'A --> B %% inline\nC-->D\n');
add('mid-token-comment', 'a%%b\n');
add('comment-no-newline', 'A\n%% no newline');
add('only-empty-marker', '%%');
add('empty-comment', '%%\n');
add('directives', "%% ordinary\n%%{init: {'theme': 'dark'}}%%\n  %% more\npacket\n");
add('nested-markers', '%%' + 'x%%'.repeat(80));
add('nested-markers-newline', '%%' + 'x%%'.repeat(80) + '\n');
add('whitespace-document', ('\n' + ' '.repeat(4)).repeat(80));
add('unclosed-whitespace', '---\n' + ' \n'.repeat(80));
add('closed-whitespace', '---\n' + ' \n'.repeat(80) + '---\n');

// ECMAScript WhiteSpace + LineTerminator, plus lookalikes which are NOT \s.
const spaces = ['\t', '\n', '\v', '\f', '\r', ' ', '\u00a0', '\u1680',
  ...Array.from({ length: 11 }, (_, i) => String.fromCharCode(0x2000 + i)),
  '\u2028', '\u2029', '\u202f', '\u205f', '\u3000', '\ufeff',
  '\u0085', '\u180e', '\u200b', '\u2060'];
spaces.forEach((space, i) => {
  add(`space-${i}-comment`, `A${space}%% comment\nB`);
  add(`space-${i}-leading`, `${space}%% comment\n${space}packet\n`);
  add(`space-${i}-fence`, `${space}---\ntitle: x\n${space}---\npacket\n`);
  add(`space-${i}-body`, `---${space}\nbody${space}\n---${space}\npacket\n`);
});
for (const separator of ['\r', '\n', '\r\n', '\u2028', '\u2029']) {
  const suffix = [...separator].map(c => c.charCodeAt(0).toString(16)).join('-');
  for (const comment of ['%%', '%%x', '%%{x', '%%%']) {
    add(`break-${suffix}-${comment.length}-${comment.charCodeAt(2) || 0}`,
      `A${separator}  ${comment}${separator}B\n`);
  }
}

// Deterministic inputs probe combinations beyond the handwritten corpus.
let seed = 0x12f01a;
const random = () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed; };
const pieces = ['---', '----', '\n', '\r\n', '\r', ' ', '\t', 'a', 'title: x',
  '%%', '%%{', '}', '\u2028', '\ufeff', '\ud83d\ude00', '%%comment\n'];
for (let i = 0; i < 700; ++i) {
  let input = '';
  const count = 1 + random() % 20;
  for (let j = 0; j < count; ++j) input += pieces[random() % pieces.length];
  add(`generated-${i}`, input);
}

// Four hex digits per UTF-16 code unit preserve even lone surrogates. '-' means
// empty, so tab fields never rely on a CSV/JSON implementation in C++ tests.
const hex = text => Array.from({ length: text.length }, (_, i) =>
  text.charCodeAt(i).toString(16).padStart(4, '0')).join('') || '-';
const lines = cases.map(({ name, input }) => {
  const front = matchFrontMatter(input);
  return [name, hex(input), hex(stripAnyComments(input)), hex(cleanupComments(input)),
    front ? String(front.length) : '-', hex(front?.indent ?? ''), hex(front?.body ?? '')].join('\t');
});
const expected = '# Mermaid 12.2.0 source text observations; UTF-16 hex fields\n' + lines.join('\n') + '\n';
const output = path.join(root, 'tests/mermaid_cpp/fixtures/source-text.tsv');
if (process.argv.includes('--write')) writeFileSync(output, expected);
else assert.equal(readFileSync(output, 'utf8').replaceAll('\r\n', '\n'), expected,
  'Source observations changed; inspect the pinned source and regenerate explicitly.');
console.log(JSON.stringify({ source: lock.commit, cases: cases.length, mode: process.argv.includes('--write') ? 'write' : 'check' }));
