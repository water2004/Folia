// Observations from the pinned, installed official packages. This file does not
// implement a Packet lexer, parser, layout algorithm, or rendering substitute.
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
  'Usage: node tools/mermaid-cpp/packet-oracle.mjs [--write|--check]');
const sourceRoot = path.join(root, '.reference/mermaid-js');
const packageRoot = path.join(root, 'tools/mermaid-cpp/oracle');
const modules = path.join(packageRoot, 'node_modules');
const commit = '1fb7869866dd619168d840291c9c54b4f129758b';
const lock = JSON.parse(readFileSync(path.join(root, 'docs/mermaid-cpp/source-lock.json'), 'utf8'));
assert.equal(lock.commit, commit, 'The Mermaid reference commit changed.');
assert.equal(execFileSync('git', ['-C', sourceRoot, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim(),
  commit, 'The reference checkout is not the pinned Mermaid commit.');
for (const entry of lock.files) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(sourceRoot, entry.path))).digest('hex'),
    entry.sha256, `Changed official source: ${entry.path}`);
}
const packageLock = readFileSync(path.join(packageRoot, 'pnpm-lock.yaml'), 'utf8');
const dependencies = { '@mermaid-js/parser': '2.0.1', mermaid: '12.2.0', jsdom: '26.1.0' };
for (const [name, version] of Object.entries(dependencies)) {
  const installed = JSON.parse(readFileSync(path.join(modules, name, 'package.json'), 'utf8'));
  assert.equal(installed.version, version, `Unexpected installed ${name} version.`);
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

const defaults = {
  rowHeight: 32, paddingX: 5, paddingY: 5, bitWidth: 32, bitsPerRow: 32,
  showBits: true, useMaxWidth: true, bitOrder: 'ascending',
};
const cases = [];
const add = (id, source, config = {}, compareLayout = true) => {
  assert(!cases.some(item => item.id === id), `Duplicate observation name: ${id}`);
  cases.push({ id, source, config: { ...defaults, ...config }, compareLayout });
};

add('empty-source', '');
add('whitespace-source', ' \t\r\n');
add('empty-packet', 'packet');
add('empty-beta', 'packet-beta');
add('indented-header', '\t packet \t\n');
add('beta-no-boundary', 'packet-beta0:"x"');
add('packet-needs-boundary', 'packet0:"x"');
add('header-wrong-case', 'Packet\n0:"x"');
add('wrong-diagram', 'notPacket\n0:"x"');
add('header-trailing-garbage', 'packet extra');
add('single-bit', 'packet\n0:"x"');
add('single-row', 'packet\n0-31:"word"\n');
add('one-line-header', 'packet 0:"x"');
add('relative-bits', 'packet\n+1:"a"\n+7:"b"\n+24:"c"\n');
add('mixed-addressing', 'packet-beta\n0-3:"a"\n+2:"b"\n6:"c"\n7-11:"d"\n');
add('field-crossing-rows', 'packet\n0-18:"cross"\n+6:"tail"', { bitsPerRow: 8 });
add('partial-last-row', 'packet\n+12:"head"\n+1:"flag"', { bitsPerRow: 8 });
add('single-bit-rows', 'packet\n+3:"span"\n+1:"last"', { bitsPerRow: 1 });
add('reverse-range', 'packet\n4-1:"x"');
add('initial-gap', 'packet\n1:"x"');
add('later-gap', 'packet\n0:"x"\n2:"y"');
add('overlap', 'packet\n0-7:"x"\n7:"y"');
add('zero-bits', 'packet\n+0:"x"');
add('zero-bits-after-field', 'packet\n0:"x"\n+0:"y"');
add('negative-start', 'packet\n-1:"x"');
add('negative-count', 'packet\n+-1:"x"');
add('leading-zero', 'packet\n00:"x"');
add('leading-zero-count', 'packet\n+01:"x"');
add('decimal-start', 'packet\n1.5:"x"');
add('zero-decimal', 'packet\n0.0:"x"');
add('scientific-start', 'packet\n1e3:"x"');
add('missing-end', 'packet\n0-:"x"');
add('missing-colon', 'packet\n0 "x"');
add('missing-label', 'packet\n0:');
add('unquoted-label', 'packet\n0:x');
add('unclosed-double-quote', 'packet\n0:"x');
add('unclosed-single-quote', "packet\n0:'x");
add('two-fields-one-line', 'packet\n0:"x" 1:"y"');
add('semicolon-separator', 'packet\n0:"x";\n1:"y"');
add('extra-colon', 'packet\n0::"x"');
add('safe-integer-limit', 'packet\n9007199254740991:"x"');
add('rounded-integer', 'packet\n9007199254740993:"x"');
add('shortest-integer-diagnostic', 'packet\n1000000000000000128:"x"');
add('exponential-integer-diagnostic', 'packet\n1000000000000000000000:"x"');

add('empty-label', 'packet\n0:""');
add('single-quoted-label', "packet\n0:'single'");
add('unicode-label', 'packet\n0:"中文 Ελληνικά 😀 e\u0301"');
add('escape-quote', 'packet\n0:"a\\\"b"', {}, false);
add('escape-backslash', 'packet\n0:"a\\\\b"', {}, false);
add('escape-known', 'packet\n0:"\\b\\f\\n\\r\\t\\v\\0"', {}, false);
add('escape-unknown', 'packet\n0:"\\u0041\\x42\\q\\/"', {}, false);
add('raw-multiline-label', 'packet\n0:"a\nb"', {}, false);
add('raw-crlf-label', 'packet\n0:"a\r\nb"', {}, false);
add('escaped-lf', 'packet\n0:"a\\\nb"', {}, false);
add('escaped-crlf', 'packet\n0:"a\\\r\nb"', {}, false);
add('escaped-ls', 'packet\n0:"a\\\u2028b"', {}, false);
add('lone-surrogate-label', 'packet\n0:"\ud800x\udfff"', {}, false);
add('entity-label', 'packet\n0:"#35; &amp; &#65;"', {}, false);
add('html-label', 'packet\n0:"<b>bold</b><br/>next"', {}, false);
add('inline-marker-in-label', 'packet\n0:"a%%b"', {}, false);

add('title', 'packet\ntitle A packet\n0:"x"');
add('empty-title', 'packet\ntitle\n0:"x"');
add('title-at-eof', 'packet\ntitle A packet');
add('title-no-boundary', 'packet\ntitlefoo\n0:"x"');
add('repeated-title', 'packet\ntitle First\ntitle Second\n0:"x"');
add('accessibility', 'packet\naccTitle: packet title\naccDescr: packet description\n0:"x"');
add('empty-accessibility', 'packet\naccTitle:\naccDescr:\n0:"x"');
add('metadata-horizontal-space', 'packet\n\t title  spaced\t\t title \t\naccTitle:\t A  B\t\n0:"x"');
add('metadata-comments', 'packet\ntitle A %% comment\naccTitle: B %% comment\naccDescr: C %% comment\n0:"x"', {}, false);
add('multiline-description', 'packet\naccDescr {\n  first\n\n\t second  word\n}\n0:"x"');
add('multiline-description-crlf', 'packet\naccDescr { a\r\nb }\n0:"x"', {}, false);
add('multiline-description-ls', 'packet\naccDescr { a\u2028 b\u2029 c }\n0:"x"', {}, false);
add('description-whitespace-before-brace', 'packet\naccDescr\n\t{ first }\n0:"x"');
add('description-missing-brace', 'packet\naccDescr { first\n0:"x"');
add('metadata-html', 'packet\ntitle <b>Bold</b>\naccDescr: <em>description</em>\n0:"x"', {}, false);

add('comment-before-header', '%% a comment\npacket\n0:"x"', {}, false);
add('comment-after-header', 'packet\n%% a comment\n0:"x"', {}, false);
add('inline-comment', 'packet\n0:"x" %% comment\n1:"y"', {}, false);
add('empty-comment-marker', 'packet\n%%\n0:"x"', {}, false);
add('comment-without-final-newline', 'packet\n0:"x"\n%% ending', {}, false);
add('yaml-before-header', '---\ntitle: Frontmatter\n---\npacket\n0:"x"', {}, false);
add('empty-yaml-before-header', '---\n---\npacket\n0:"x"', {}, false);
add('yaml-inside-grammar', 'packet\n---\nignored: true\n---\n0:"x"', {}, false);
add('yaml-indented-opening', 'packet\n \t---\nignored: true\n---\n0:"x"', {}, false);
add('yaml-close-at-eof', 'packet\n---\nignored: true\n---', {}, false);
add('yaml-wrong-close-indent', 'packet\n---\nx\n ---\n0:"x"', {}, false);
add('yaml-close-boundary', 'packet\n---\nx\n---x\n0:"x"', {}, false);
add('yaml-missing-close', 'packet\n---\nx\n0:"x"', {}, false);
add('directive-before-header', '%%{init:{"packet":{"bitsPerRow":8}}}%%\npacket\n0-15:"x"', {}, false);
add('directive-multiline', 'packet\n%%{\nopaque text\n}%%\n0:"x"', {}, false);
add('directive-eats-required-newline', 'packet\n0:"x" %%{opaque}%%\n1:"y"', {}, false);
add('directive-after-title', 'packet\ntitle X %%{opaque}%%\n0:"x"', {}, false);
add('directive-close-boundary', 'packet\n%%{opaque}%%x\n0:"x"', {}, false);
add('directive-close-at-eof', 'packet\n%%{opaque}%%', {}, false);
add('directive-unclosed-is-comment', 'packet\n%%{opaque\n0:"x"', {}, false);
add('frontmatter-config', '---\ntitle: Configured\nconfig:\n  packet:\n    bitsPerRow: 8\n    showBits: false\n---\npacket\n0-15:"x"', {}, false);

const spaces = ['\t', '\n', '\v', '\f', '\r', ' ', '\u00a0', '\u1680',
  ...Array.from({ length: 11 }, (_, i) => String.fromCharCode(0x2000 + i)),
  '\u2028', '\u2029', '\u202f', '\u205f', '\u3000', '\ufeff',
  '\u0085', '\u180e', '\u200b', '\u2060'];
spaces.forEach((space, index) => {
  add(`space-${index}-header`, `packet${space}0:"x"`, {}, false);
  add(`space-${index}-description`, `packet\naccDescr${space}{ a${space}b }\n0:"x"`, {}, false);
});

let seed = 0x5041434b;
const random = () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed; };
for (let index = 0; index < 70; ++index) {
  const config = {
    bitsPerRow: [1, 4, 8, 16, 24, 32, 64][random() % 7],
    rowHeight: 12 + random() % 37,
    bitWidth: 8 + random() % 41,
    paddingX: random() % 7,
    paddingY: random() % 11,
    showBits: Boolean(random() & 1),
    useMaxWidth: Boolean(random() & 2),
    bitOrder: random() & 4 ? 'descending' : 'ascending',
  };
  let source = index % 2 ? 'packet\n' : 'packet-beta\n';
  if (index % 5 === 0) source += `title Random register ${index}\n`;
  let next = 0;
  const fields = 1 + random() % 8;
  for (let field = 0; field < fields; ++field) {
    const bits = 1 + random() % (config.bitsPerRow * 2 + 1);
    const address = random() & 1 ? `+${bits}` : bits === 1 ? `${next}` : `${next}-${next + bits - 1}`;
    source += `${address}:"field ${index}.${field}"\n`;
    next += bits;
  }
  add(`generated-valid-${index}`, source, config);
}
for (let index = 0; index < 30; ++index) {
  const start = 1 + random() % 40;
  const variants = [`${start}:"gap"`, `0-${start}:"first"\n${start}:"overlap"`,
    `${start}-0:"reverse"`, '0:"first"\n+0:"zero"', `0-${start} "missing colon"`,
    `0:"first" ${start}:"missing newline"`];
  add(`generated-invalid-${index}`, `packet\n${variants[index % variants.length]}\n`,
    { bitsPerRow: [4, 8, 16, 32][random() % 4] });
}

// Four hex digits per UTF-16 code unit preserve raw strings, including NUL and
// lone surrogates. '-' is empty (or an absent optional numeric AST field).
const hex = text => Array.from({ length: text.length }, (_, index) =>
  text.charCodeAt(index).toString(16).padStart(4, '0')).join('') || '-';
const number = value => value === undefined ? '-' : String(value);
const row = values => values.join('\t');
const message = error => error instanceof Error ? error.message : String(error);
const boolean = value => value ? '1' : '0';
const lines = [
  '# Mermaid Packet observations v1; UTF-16 hexadecimal string fields; - means empty/absent.',
  `# Official source ${commit}; mermaid@12.2.0; @mermaid-js/parser@2.0.1; jsdom@26.1.0.`,
  '# case: id source raw_ok title acc_title acc_descr row_height padding_x padding_y bit_width bits_per_row show_bits use_max_width descending layout_ok layout_error',
  '# block: id start end bits label; word: id row_index block_index start end bits label.',
  '# raw_error: id message. Raw parser diagnostics are recorded, not required to match native wording.',
  '# full_case: id ok title acc_title acc_descr effective_config_json error stage; full_word uses word fields.',
  '# case/block are RAW parser results. word is FULL API layout only for explicitly comparable inputs.',
  '# A layout_ok of - excludes layout comparison; full_case/full_word still preserve the separate API result.',
  '# Full API performs preprocessing, entity encoding, and sanitization; it is never a raw-parser oracle.',
];
const totals = { cases: cases.length, rawSuccess: 0, rawFailure: 0, layoutSuccess: 0,
  layoutFailure: 0, layoutExcluded: 0, fullSuccess: 0, fullFailure: 0 };

for (const item of cases) {
  const { id, source, config, compareLayout } = item;
  let ast;
  let rawError = '';
  try {
    ast = await parse('packet', source);
    ++totals.rawSuccess;
  } catch (error) {
    rawError = message(error);
    ++totals.rawFailure;
  }

  // Reinitialize and replace the SVG for each case. No layout implementation is
  // copied into this script: PacketDB is populated by the actual Mermaid API.
  document.body.innerHTML = '<svg id="probe" xmlns="http://www.w3.org/2000/svg"></svg>';
  mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', packet: config });
  let full;
  let diagram;
  let fullError = '';
  let stage = 'parse';
  try {
    diagram = await mermaid.mermaidAPI.getDiagramFromText(source);
  } catch (error) {
    fullError = message(error);
  }
  if (diagram) {
    assert.equal(diagram.type, 'packet', 'Expected the full API to select the Packet diagram.');
    stage = 'render';
    try {
      await diagram.render('probe', '12.2.0');
      full = {
        title: diagram.db.getDiagramTitle(),
        accTitle: diagram.db.getAccTitle(),
        accDescr: diagram.db.getAccDescription(),
        config: diagram.db.getConfig(),
        words: structuredClone(diagram.db.getPacket()),
      };
      stage = 'complete';
    } catch (error) {
      fullError = message(error);
    }
  }
  ++totals[full ? 'fullSuccess' : 'fullFailure'];

  const compare = Boolean(ast) && compareLayout;
  if (compare && full) {
    for (const key of ['title', 'accTitle', 'accDescr']) {
      assert.equal(full[key], ast[key] ?? '', `${id}: preprocessing changed ${key}; mark the stages separately.`);
    }
  }
  if (compare) ++totals[full ? 'layoutSuccess' : 'layoutFailure'];
  else ++totals.layoutExcluded;
  lines.push(row(['case', id, hex(source), boolean(ast), hex(ast?.title ?? ''),
    hex(ast?.accTitle ?? ''), hex(ast?.accDescr ?? ''), config.rowHeight, config.paddingX,
    config.paddingY, config.bitWidth, config.bitsPerRow, boolean(config.showBits),
    boolean(config.useMaxWidth), boolean(config.bitOrder === 'descending'),
    compare ? boolean(full) : '-', hex(compare && !full ? fullError : '')]));
  for (const block of ast?.blocks ?? []) {
    lines.push(row(['block', id, number(block.start), number(block.end), number(block.bits), hex(block.label)]));
  }
  if (rawError) lines.push(row(['raw_error', id, hex(rawError)]));
  if (compare && full) {
    full.words.forEach((word, wordIndex) => word.forEach((block, blockIndex) => {
      lines.push(row(['word', id, wordIndex, blockIndex, block.start, block.end, block.bits, hex(block.label)]));
    }));
  }
  lines.push(row(['full_case', id, boolean(full), hex(full?.title ?? ''), hex(full?.accTitle ?? ''),
    hex(full?.accDescr ?? ''), hex(full ? JSON.stringify(full.config) : ''), hex(fullError), stage]));
  full?.words.forEach((word, wordIndex) => word.forEach((block, blockIndex) => {
    lines.push(row(['full_word', id, wordIndex, blockIndex, block.start, block.end, block.bits, hex(block.label)]));
  }));
}

const expected = lines.join('\n') + '\n';
const output = path.join(root, 'tests/mermaid_cpp/fixtures/packet.tsv');
if (arguments_.includes('--write')) writeFileSync(output, expected);
else assert.equal(readFileSync(output, 'utf8').replaceAll('\r\n', '\n'), expected,
  'Packet observations changed; inspect the pinned packages and regenerate explicitly.');
dom.window.close();
console.log(JSON.stringify({ source: commit, ...totals, mode: arguments_.includes('--write') ? 'write' : 'check' }));
