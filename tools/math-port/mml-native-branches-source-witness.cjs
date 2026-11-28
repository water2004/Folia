#!/usr/bin/env node
'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2)) {
  throw new Error('usage: node mml-native-branches-source-witness.cjs [--repo-root <repository>]');
}
const root = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) : findRepositoryRoot();
assert.ok(root, 'pass --repo-root <repository> or run this script from within the repository');
const bundle = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const cjsFiles = [
  ['cjs/core/MmlTree/MmlNode.js', '86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45'],
  ['cjs/core/MmlTree/MmlFactory.js', 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3'],
  ['cjs/core/MmlTree/MML.js', '380332b7f0cc13104130affb65532ebca2ac214ccd897b93edda551cea443684'],
  ['cjs/core/MmlTree/MmlNodes/HtmlNode.js', 'cd10798f07fbdafb6f2a375b85904a5fdcf0c546cc95ddc955cae5e411ce3eef'],
];
for (const [file, expected] of cjsFiles) {
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(bundle, file))).digest('hex');
  assert.equal(actual, expected, `${file} does not match pinned bundle`);
}
assert.equal(JSON.parse(fs.readFileSync(path.join(bundle, 'package.json'), 'utf8')).version, '4.1.3');
const { MmlFactory } = require(path.join(bundle, 'cjs/core/MmlTree/MmlFactory.js'));
const { AbstractMmlTokenNode, AbstractMmlLayoutNode, AbstractMmlBaseNode } = require(path.join(bundle, 'cjs/core/MmlTree/MmlNode.js'));
const factory = new MmlFactory();
const make = kind => factory.create(kind);
const token = make('mi');
const text = make('text');
text.setText('A😀');
token.appendChild(text);
const html = make('html');
const htmlXml = { tag: 'b', body: '<b/>' };
const adaptor = {
  clone: value => ({ ...value, cloned: true }),
  serializeXML: value => value.body || value.tag,
  getAttribute: () => { throw new Error('missing data-mjx-hdw'); },
  node: (tag, attrs, children) => ({ tag, attrs, children }),
  outerHTML: value => `<span><${value.children[0].tag}/></span>`,
  textContent: () => 'html text',
  kind: value => value.tag,
};
html.setHTML(htmlXml, adaptor);
token.appendChild(html);
html.verifyTree({ fullErrors: true });
const visited = [];
token.walkTree(node => visited.push(node.kind));
const emptyToken = make('mi');
const emptyText = make('text');
emptyText.setText('');
emptyToken.appendChild(emptyText);
const layout = make('math');
const base = make('msubsup');
base.appendChild(make('mi'));
base.appendChild(make('mn'));
base.appendChild(make('mn'));
const baseResult = base.setTeXclass(null);
const sourceText = make('text');
sourceText.setText('A😀');
const textCopy = sourceText.copy();
let clonedXmlCalls = 0;
const xml = make('XML');
xml.setXML({ body: '<p/>' }, { clone(value) { clonedXmlCalls++; return { ...value, cloneMarker: true }; }, serializeXML: value => value.body });
const xmlCopy = xml.copy();
const htmlParent = make('mrow');
html.verifyTree({ fullErrors: true });
const invalidHtml = make('html');
htmlParent.appendChild(invalidHtml);
const verifyOptions = { fullErrors: true };
let verifyCall;
invalidHtml.mError = (...args) => { verifyCall = args; return null; };
invalidHtml.verifyTree(verifyOptions);
const result = {
  defaults: {
    token: AbstractMmlTokenNode.defaults,
    layout: AbstractMmlLayoutNode.defaults,
    base: AbstractMmlBaseNode.defaults,
  },
  token: { text: token.getText(), empty: token.isEmpty, walk: visited, emptyTextToken: emptyToken.isEmpty },
  layout: { core: layout.core().kind, coreMO: layout.coreMO().kind, arity: layout.arity, spacelike: layout.isSpacelike },
  base: { core: base.core().kind, coreMO: base.coreMO().kind, result: baseResult.kind, texClass: base.texClass },
  text: { string: sourceText.toString(), copy: textCopy.getText() },
  xml: { payload: xmlCopy.getXML().body, cloned: xmlCopy.getXML().cloneMarker, adaptorCopied: xmlCopy.adaptor !== null, cloneCalls: clonedXmlCalls },
  html: { wrapper: html.getHTML().tag, serialized: html.getSerializedHTML(), text: html.textContent(), string: html.toString(), validParent: html.parent.kind, verify: verifyCall },
};
const expected = {
  defaults: {
    token: { mathbackground: '_inherit_', mathcolor: '_inherit_', mathsize: '_inherit_', dir: '_inherit_', mathvariant: 'normal' },
    layout: { mathbackground: '_inherit_', mathcolor: '_inherit_', mathsize: '_inherit_', dir: '_inherit_' },
    base: { mathbackground: '_inherit_', mathcolor: '_inherit_', mathsize: '_inherit_', dir: '_inherit_' },
  },
  token: { text: 'A😀html text', empty: false, walk: ['mi'], emptyTextToken: true },
  layout: { core: 'inferredMrow', coreMO: 'inferredMrow', arity: -1, spacelike: true },
  base: { core: 'mi', coreMO: 'mi', result: 'mi', texClass: 0 },
  text: { string: 'A😀', copy: 'A😀' },
  xml: { payload: '<p/>', cloned: true, adaptorCopied: false, cloneCalls: 1 },
  html: {
    wrapper: 'span', serialized: '<span><b/></span>', text: 'html text', string: 'HTML=<span>...</span>', validParent: 'mi',
    verify: ['HTML can only be a child of a token element', verifyOptions, true],
  },
};
assert.deepEqual(result, expected);
const output = path.join(root, 'build/math-port/typed-mml-branches/source-witness.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify({ output: path.relative(root, output), ...result }, null, 2));





