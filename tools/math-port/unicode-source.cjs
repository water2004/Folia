'use strict';
// Independent Source MML and error observations for MathJax 4.1.3 Unicode.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const pinned = 'fb987178c2d279a99b0db5ea02e751691703955e';
const unicodeCjsSha256 = '49632603b6d951624795673cd43d631f0c57c6e571d09e08e4abb6e925e6ae9c';
const referenceRoot = path.join(root, '.reference/MathJax-src');
const revision = require('node:child_process').execFileSync(
    'git', ['-C', referenceRoot, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
assert.equal(revision, pinned, 'MathJax Source checkout must remain pinned');
for (const file of [
    path.join(referenceRoot, 'ts/input/tex/unicode/UnicodeConfiguration.ts'),
    path.join(sourceRoot, 'input/tex/unicode/UnicodeConfiguration.js'),
    path.join(sourceRoot, 'input/tex/unicode/UnicodeConfiguration.js.map'),
]) {
    assert.ok(fs.existsSync(file), file);
}

const load = name => require(path.join(sourceRoot, name));
const { TeX } = load('input/tex.js');
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
const { AbstractMathItem } = load('core/MathItem.js');
const { NodeFactory } = load('input/tex/NodeFactory.js');
require(path.join(sourceRoot, 'input/tex/newcommand/NewcommandConfiguration.js'));
const unicodeConfigPath = require.resolve(path.join(sourceRoot,
    'input/tex/unicode/UnicodeConfiguration.js'));
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(unicodeConfigPath)).digest('hex'),
    unicodeCjsSha256, 'the executed UnicodeConfiguration CJS must match the reviewed Source build');
require(unicodeConfigPath);

function snapshot(rootNode) {
    let result = '';
    const add = value => { value = String(value); result += `${value.length}:${value}`; };
    function visit(node) {
        if (!node) { add('null'); return; }
        add(node.kind);
        const entries = Object.entries(node.attributes?.getAllAttributes?.() ?? {})
            .sort(([left], [right]) => left.localeCompare(right));
        add(entries.length);
        for (const [key, value] of entries) {
            add(key); add(value === null ? 'null' : typeof value); add(value ?? '');
        }
        const unicode = node.getProperty?.('unicode');
        add(unicode === undefined ? '' : typeof unicode);
        add(unicode === undefined ? '' : String(unicode));
        add(node.isToken ? node.getText() : '');
        const children = node.isToken ? [] : node.childNodes;
        add(children.length);
        children.forEach(visit);
    }
    visit(rootNode);
    return result;
}

function fixture({ freshModule = true } = {}) {
    if (freshModule) {
        delete require.cache[unicodeConfigPath];
        require(unicodeConfigPath);
    }
    let error = null;
    const jax = new TeX({
        packages: ['base', 'newcommand', 'unicode'], maxMacros: 1000,
        formatError(j, exception) { error = exception; return j.formatError(exception); },
    });
    jax.setMmlFactory(new MmlFactory());
    return {
        jax,
        compile(input) {
            error = null;
            try {
                const tree = jax.compile(new AbstractMathItem(input, jax, true), {});
                return { tree, error: error && { id: error.id, message: String(error.message) } };
            } catch (exception) {
                return { tree: null, error: { id: exception.id ?? exception.name,
                    message: String(exception.message) } };
            }
        },
    };
}

const historical = [
    ['unicode-decimal', String.raw`\unicode{65}`],
    ['unicode-hex', String.raw`\unicode{0x1F600}`],
    ['unicode-height-font', String.raw`\unicode[1.2,0.3][Example Font]{65}`],
    ['unicode-bad-font', String.raw`\unicode[Bad;Font]{65}`],
    ['unicode-bad-value', String.raw`\unicode{xyz}`],
    ['raw-unicode', String.raw`\U{1F600}`],
    ['raw-unicode-invalid', String.raw`\U{110000}`],
    ['char-decimal', String.raw`\char65`],
    ['char-hex', String.raw`\char"41`],
    ['char-octal', String.raw`\char'101`],
    ['char-control-sequence', String.raw`\char` + '`\\alpha'],
];
const historicalSource = fs.readFileSync(path.join(root,
    'tools/math-port/tex-next-seven-original-oracle.cjs'), 'utf8');
assert.ok(historicalSource.includes("['char-control-sequence',String.raw`\\char`+\"`\\\\alpha\""));
assert.equal(historical.length, 11);
const inputs = [...historical];
inputs.push(
    ['unicode-font-cache-set', String.raw`\unicode[Example Font]{65}`],
    ['unicode-font-cache-read', String.raw`\unicode{65}`],
    ['unicode-font-cache-replace', String.raw`\unicode[Second Font]{65}`],
    ['unicode-font-cache-read-replaced', String.raw`\unicode{65}`],
    ['unicode-empty-font', String.raw`\unicode[]{65}`],
    ['unicode-dimension-no-font', String.raw`\unicode[1.2,0.3]{66}`],
    ['unicode-dimension-with-font', String.raw`\unicode[1.2,0.3][Metric Font]{67}`],
    ['unicode-hex-lower', String.raw`\unicode{0xff}`],
    ['unicode-hex-upper', String.raw`\unicode{0X41}`],
    ['unicode-zero', String.raw`\unicode{0}`],
    ['unicode-surrogate', String.raw`\unicode{0xD800}`],
    ['unicode-supplementary', String.raw`\unicode{0x1F600}`],
    ['unicode-too-large', String.raw`\unicode{0x110000}`],
    ['unicode-semicolon', String.raw`\unicode[A;B]{65}`],
    ['unicode-bad-hex', String.raw`\unicode{0xGG}`],
    ['unicode-empty', String.raw`\unicode{}`],
    ['raw-nul-following', String.raw`\U{0}x`],
    ['raw-surrogate-following', String.raw`\U{D800}x`],
    ['raw-max', String.raw`\U{10FFFF}x`],
    ['raw-over-max', String.raw`\U{110000}x`],
    ['raw-lowercase', String.raw`\U{abc}`],
    ['raw-space', String.raw`\U{ 41 }x`],
    ['char-zero-following', String.raw`\char0x`],
    ['char-seven-decimal-digits', String.raw`\char1234567x`],
    ['char-eight-decimal-digits', String.raw`\char12345678x`],
    ['char-hex-max-width', String.raw`\char"ABCDEFx`],
    ['char-hex-lowercase', String.raw`\char"abx`],
    ['char-octal-max-width', String.raw`\char'7777777x`],
    ['char-octal-overflow', String.raw`\char'77777777x`],
    ['char-backtick-emoji', String.raw`\char` + '`😀x'],
    ['char-backtick-surrogate', String.raw`\char` + '`\uD800x'],
    ['char-backtick-newline', String.raw`\char` + '`\nx'],
    ['char-backtick-space', String.raw`\char` + '` x'],
    ['char-backtick-cs', String.raw`\char` + '`\\alpha x'],
    ['char-backtick-invalid-cs', String.raw`\char` + '`\\alphaBeta x'],
    ['unicode-bold-font', String.raw`\mathbf{\unicode[Bold Font]{65}}`],
    ['unicode-italic-font', String.raw`\mathit{\unicode[Italic Font]{65}}`],
    ['unicode-alias', String.raw`\let\savedUnicode=\unicode \savedUnicode{65}`],
    ['raw-alias', String.raw`\let\savedRaw=\U \savedRaw{1F600}`],
    ['char-alias', String.raw`\let\savedChar=\char \savedChar"41`],
);

const rows = [];
for (const [name, input] of inputs) {
    const f = fixture();
    const { tree, error } = f.compile(input);
    rows.push({ name, input, snapshot: tree ? snapshot(tree) : '',
        error: error?.id ?? '', message: error?.message ?? '' });
}

const recoveryRows = [];
const recoverySequences = [
    [String.raw`\unicode{bad}`, String.raw`\unicode{65}`],
    [String.raw`\U{110000}`, String.raw`\U{41}`],
    [String.raw`\char x`, String.raw`\char65`],
];
for (let group = 0; group < recoverySequences.length; ++group) {
    const f = fixture();
    for (const input of recoverySequences[group]) {
        const result = f.compile(input);
        recoveryRows.push({ group, input, snapshot: result.tree ? snapshot(result.tree) : '',
            error: result.error?.id ?? '', message: result.error?.message ?? '' });
    }
}

const creatorSource = String.raw`\unicode[Creator Font]{67}`;
const creatorFixture = fixture();
const creatorRequests = [];
creatorFixture.jax.parseOptions.nodeFactory.setCreators({
    token(factory, kind, definition, text) {
        if (kind === 'mtext') creatorRequests.push({ kind, text: text ?? '',
            fontfamily: definition.fontfamily ?? '' });
        return NodeFactory.createToken(factory, kind, definition, text);
    },
});
const creatorResult = creatorFixture.compile(creatorSource);
assert.ok(creatorResult.tree && !creatorResult.error);
assert.equal(creatorRequests.length, 1);

// Source's module-scoped cache is shared by TeX instances until its actual
// configuration module is reloaded, which models a fresh loaded registry.
const shared = fixture();
const sharedRows = [];
for (const input of [String.raw`\unicode[Shared Font]{945}`, String.raw`\unicode{945}`,
    String.raw`\unicode[Replaced Font]{945}`, String.raw`\unicode{945}`]) {
    const result = shared.compile(input);
    sharedRows.push({ input, snapshot: result.tree ? snapshot(result.tree) : '',
        error: result.error?.id ?? '' });
    if (sharedRows.length === 1) shared.jax.reset();
}
const secondJax = fixture({ freshModule: false });
const secondJaxResult = secondJax.compile(String.raw`\unicode{945}`);
sharedRows.push({ input: String.raw`\unicode{945}`,
    snapshot: secondJaxResult.tree ? snapshot(secondJaxResult.tree) : '',
    error: secondJaxResult.error?.id ?? '' });
const fresh = fixture();
const freshResult = fresh.compile(String.raw`\unicode{945}`);
sharedRows.push({ input: String.raw`\unicode{945}`, snapshot:
    freshResult.tree ? snapshot(freshResult.tree) : '', error: freshResult.error?.id ?? '' });

const {utf16Field, byteField, writeTables} = require('./fixture-data.cjs');
const fixturePath = path.join(root, 'tests/math/fixtures/packages/unicode');
writeTables(fixturePath, {
  "creator-input": {columns: ["source_utf16", "snapshot_utf16"],
    rows: [[utf16Field(creatorSource), utf16Field(snapshot(creatorResult.tree))]]},
  "creator-requests": {columns: ["kind_utf16", "text_utf16", "fontfamily_utf16"],
    rows: creatorRequests.map(r => [r.kind, r.text, r.fontfamily].map(utf16Field))},
  "cases": {columns: ["name_utf16", "source_utf16", "snapshot_utf16", "error_utf16", "message_utf16"],
    rows: rows.map(r => [r.name, r.input, r.snapshot, r.error, r.message].map(utf16Field))},
  "registry": {columns: ["source_utf16", "snapshot_utf16", "error_utf16"],
    rows: sharedRows.map(r => [r.input, r.snapshot, r.error].map(utf16Field))},
  "recovery": {columns: ["group_integer", "source_utf16", "snapshot_utf16", "error_utf16", "message_utf16"],
    rows: recoveryRows.map(r => [r.group, ...[r.input, r.snapshot, r.error, r.message].map(utf16Field)])},
});
console.log(JSON.stringify({ cases: rows.length, historical: historical.length,
    trees: rows.filter(row => !row.error).length,
    errors: rows.filter(row => row.error).length,
    registryCases: sharedRows.length, recoverySteps: recoveryRows.length,
    sourceCommit: revision, unicodeCjsSha256,
    fixture: path.relative(root, fixturePath) }));
