'use strict';

// Source-only witness for MathJax 4.1.3 LiteParser/LiteAdaptor behavior.
// It records observed Lite semantics; this is not a browser DOM conformance suite.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const cjsRoot = path.join(packageRoot, 'cjs');
const expectedCommit = 'fb987178c2d279a99b0db5ea02e751691703955e';
const expectedSourceInputs = {
  '.reference/MathJax-src/ts/adaptors/lite/Parser.ts': 'f3c15592f359cdca1a3c2729c230f8de862e8b67be5ae8f1d2d387e8482fe5d9',
  '.reference/MathJax-src/ts/adaptors/liteAdaptor.ts': '6aded576671ae8cf4b1146cc82c793628fa593abdc00d05e3e8880c53fe6a214',
  '.reference/MathJax-src/ts/adaptors/lite/Document.ts': 'eeb005e7c8c65be48e57b77da7a0e05f0b36b8718a0c8f4d46830d6984e80221',
  '.reference/MathJax-src/ts/adaptors/lite/Element.ts': 'd3fc9147fa7bf658fd2eb523784edc0940f66c6223d61bdd3b9d7f6b5e53bfeb',
  '.reference/MathJax-src/ts/adaptors/lite/Text.ts': '542c2534c5a4b663fe261c84e30d3fdac59196e362ee7f9c33d4f0faa25d0d59',
  '.reference/MathJax-src/ts/adaptors/lite/List.ts': '627e5f65fac939f54bdf7a6c6cac143c303762405c732069ccebfa4720bb55d2',
  '.reference/MathJax-src/ts/adaptors/lite/Window.ts': '4c97e935379a8305420ccdfcbec67b99ac64d75f6191d95c1e67494e563fcfdc',
  '.reference/MathJax-src/ts/util/Entities.ts': '53c5c38ee612b0b99cea8231726a09ecbd64be2201959dcf4c2da695855a0f76',
  '.reference/MathJax-src/testsuite/tests/input/tex/Html.test.ts': '29f5a9c0501417e6e1278d1daf1bec75d131f8ecb8068659aa7a747c8f8712c6',
  '.reference/MathJax-src/testsuite/tests/input/tex/TexHtml.test.ts': 'f6c3f886f0c26ca61b25705cf2bec26582d4517e64c440bc5f21b0da284edd28'
};
const expectedCjsInputs = {
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/adaptors/lite/Document.js': 'c679ae2dc90a3c8fdd699d762079fc50f75aed5f48775859ebfb3ee050b69abc',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/adaptors/lite/Element.js': '81626bfcd19aaf7b5d8a0c461d5f9c9f3a3d3c150b283c7641f9c38e6fcca9c6',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/adaptors/lite/List.js': '6d2e9aa67581b15ce657c60081cd1958b882b928e4e1fb016f75e30d8d82e8a2',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/adaptors/lite/Parser.js': '38874dadf30ee0bd60184dde9bdf29a1be9d45755893b2a025d8e1b0190ad8c0',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/adaptors/lite/Text.js': 'aab6de43d689d449401990932909c895bc29b76e28bcf22158d6f543a2059ffd',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/adaptors/lite/Window.js': '9feeac0e2f5802cf0e590bd72b2a98cabde368103790fdb50a657155efae9caf',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/adaptors/liteAdaptor.js': 'e9879de720f356cf8f54c0960102b1e40b4eafc1e34a14481aa896e1e99bd76f',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/adaptors/NodeMixin.js': 'd331d73f054c46dfd3122c41bd6762389935b68c2e89bf1a4011cc70ea42ef3a',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/components/version.js': '6745428310a6a0e35b9a8fc9fab9d89f181bf5ca8ccc27c169efce14824f9d81',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/DOMAdaptor.js': '153998de1ae9f156cc9a5c2323f8840fc10010004ea1dfd2e25c09547e4de15d',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/HandlerList.js': '499ad4c1b81cbb52abdb74b124e3a691405464881fca347927a8d9d31647a59a',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/mathjax.js': '17431ff42f26bfca6cc2baee68acb09d551fec8824f392290c5ebf0350df4ec6',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/AsyncLoad.js': '938fa1d48bf71ccc6b081a4d03f1acbe2b500acb235a2e449d2faf42a626ab0c',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/context.js': 'c3933cc87801c3a87a4a68b19ed845532750d70c14e19dca8252f2416d4a05f7',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/Entities.js': 'a616936550aa115ad994c5f1f7d3bfb50cb818c2876beaae6b60a6e58a647d07',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/Options.js': '76f3df8a3e899fba1d0944d5b79818b586c1d9f95cc7021c395fbb10295db660',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/PrioritizedList.js': '787bd21f3634312af666917d8f140145864f98d09ede1bdde3c957d28ec2bc25',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/Retries.js': '0ffe30ec3d65f1d5edfdbb505e5b76cf5b66ec1a920216ab53595df8e666b549',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/Styles.js': '842ae4900f4075463e0863d7419909f0f398b5ec98f85c2c951588fc5d4de5c2',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/entities/opf.js': 'dba93c5032c3714b526485341a5dc38747fbd93af674b79f0d48e9a6c8e0dfec',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/entities/z.js': '56f43554c35559c43feb5472fdb798cef4d81eef9371afb59b6ef6131e3160bd',
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util/entities/n.js': 'a05ef3431421dabb d4ffb7dba302541e0ab9f75d0736bba21994f88ace792fc1'.replaceAll(' ', '')
};

const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const commit = execFileSync('git', ['-C', sourceRoot, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
assert.equal(commit, expectedCommit, 'MathJax Source commit mismatch');
for (const [relative, expected] of Object.entries(expectedSourceInputs))
  assert.equal(digest(path.join(root, relative)), expected, `Pinned Source input changed: ${relative}`);
for (const [relative, expected] of Object.entries(expectedCjsInputs))
  assert.equal(digest(path.join(root, relative)), expected, `Pinned CJS input changed: ${relative}`);

const { VERSION } = require(path.join(cjsRoot, 'components/version.js'));
assert.equal(VERSION, '4.1.3');
const { liteAdaptor } = require(path.join(cjsRoot, 'adaptors/liteAdaptor.js'));
const { mathjax } = require(path.join(cjsRoot, 'mathjax.js'));
const { handleRetriesFor } = require(path.join(cjsRoot, 'util/Retries.js'));
const entities = require(path.join(cjsRoot, 'util/Entities.js'));
const adaptor = liteAdaptor();

const snapshotNode = node => {
  if (!node) return null;
  const kind = adaptor.kind(node);
  if (kind === '#text' || kind === '#comment') return { kind, value: adaptor.value(node) };
  return {
    kind,
    attributes: adaptor.allAttributes(node).map(({ name, value }) => [name, value]),
    children: adaptor.childNodes(node).map(snapshotNode)
  };
};
const snapshotDocument = doc => ({
  kind: doc.kind,
  doctype: adaptor.doctype(doc),
  root: snapshotNode(adaptor.root(doc)),
  head: snapshotNode(adaptor.head(doc)),
  body: snapshotNode(adaptor.body(doc))
});
const parseObservation = (html, format = 'text/html') => {
  try {
    const doc = adaptor.parse(html, format);
    return {
      document: snapshotDocument(doc),
      bodyText: adaptor.textContent(doc.body),
      bodyInnerHTML: adaptor.innerHTML(doc.body),
      rootHTML: adaptor.outerHTML(doc.root),
      rootXML: adaptor.serializeXML(doc.root)
    };
  } catch (error) {
    return { error: { name: error.name || '', message: String(error.message || error) } };
  }
};

const htmlInputs = [
  ['fragment-inline-text', 'before<p id="p">A &amp; B</p>after'],
  ['ordered-attributes-entity-values', "<p z='last' data-a=unquoted title=\"A &amp; B\" empty disabled>text</p>"],
  ['duplicate-and-case-fold', '<DIV ID="first" id="second" DATA-X="a&#65;">x</DIV>'],
  ['comments-doctype-and-pi', '<?xml version="1.0"?><!DOCTYPE html><!--lead--><p>x<!--mid--></p><!--tail-->'],
  ['explicit-document-normalization', '<!DOCTYPE html><html lang="en"><head><title>A &amp; B</title></head><body><p>x</p></body></html>'],
  ['html-root-without-head', '<html><body><i>only body</i></body></html>'],
  ['body-only-root', '<body><b>body only</b></body>'],
  ['head-only-root', '<head><title>head only</title></head>'],
  ['fragment-multiple-elements', '<h1>A</h1><p>B</p><p>C</p>'],
  ['doctype-after-leading-comment', '<!--lead--><!DOCTYPE html><p>x</p>'],
  ['self-closing-elements', '<p>a<br/><img src=x><input disabled>z</p>'],
  ['slash-on-nonvoid', '<div/>tail<span>text</span>'],
  ['pcdata-title-textarea', '<title>A &amp; <b>B</b></title><textarea>C &lt; <i>D</i></textarea>'],
  ['pcdata-style-script-option', '<style>a < b &amp; c</style><script>if (a < b) x()</script><option>A &amp; B</option>'],
  ['pcdata-fieldset', '<fieldset>A <b>B</b> &amp; C</fieldset>tail'],
  ['pcdata-unclosed', '<textarea>A <b>B</textarea>after'],
  ['mismatched-close-tags', '<div><p>A</div><span>B</span>'],
  ['unclosed-nesting', '<div><p><b>x'],
  ['malformed-attributes', '<p a==x b = "q" c="unterminated d=e>t</p>'],
  ['processing-instructions-removed', 'a<?probe x?><p>b</p><?again?>c'],
  ['foreign-content-no-namespace-engine', '<svg viewBox="0 0 1 1"><title>x</title><path d="M0 0"/></svg>'],
  ['numeric-and-basic-entities', '<p>&amp; &lt; &gt; &quot; &#65; &#x1F600;</p>'],
  ['unknown-numeric-entity-edge', '<p>&#0; &#xD800; &#999999999999999999999;</p>'],
  ['unknown-entity-left-literal', '<p>&notARealLiteEntity;</p>'],
  ['empty-input', ''],
  ['text-only-input', 'plain &amp; text'],
  ['newlines-tabs-and-tag-spacing', '\n<p\tclass = "a b"\n data-x = x >line\nnext</p>'],
  ['adjacent-comments-and-empty-tags', '<!--a--><i></i><!--b--><br><!--c-->'],
  ['preformatted-like-elements', '<pre>  a\n  b &amp; c</pre><code><x>y</code>'],
  ['alternate-format-argument-is-ignored', '<p>A<br>B</p>', 'application/xhtml+xml']
];
const unknownParserCase = htmlInputs.find(([name]) => name === 'unknown-entity-left-literal');
const records = htmlInputs.filter(([name]) => name !== 'unknown-entity-left-literal').map(([name, html, format]) => ({
  id: `parse/${name}`, area: 'parse/tree/serialize', input: { html, format: format || 'text/html' },
  observation: parseObservation(html, format)
}));

const queryHTML = '<main><p id="one" class="a b" data-v="x">A<span>B</span></p><p id="two" class="b">C</p></main>';
const queryDoc = adaptor.parse(queryHTML);
const queryRoot = queryDoc.body;
const pathOf = target => {
  if (!target) return null;
  const parts = [];
  let node = target;
  while (node && node !== queryRoot) {
    const parent = adaptor.parent(node);
    if (!parent) return null;
    parts.unshift(adaptor.childNodes(parent).indexOf(node));
    node = parent;
  }
  return node === queryRoot ? parts : null;
};
const selectors = ['#one', '.b', 'p', '[data-v="x"]', 'main', 'span', 'p span', '[class="b"]', ':first-child'];
records.push({
  id: 'queries/simple-selector-contract', area: 'query',
  input: { html: queryHTML, selectors },
  observation: {
    selectors: selectors.map(selector => [selector, pathOf(adaptor.getElement(selector, queryRoot))]),
    allP: adaptor.tags(queryRoot, 'p').map(pathOf),
    namespacedSearch: adaptor.tags(queryRoot, 'p', 'urn:no-namespaces').map(pathOf),
    idSearch: pathOf(adaptor.elementById(queryRoot, 'two')),
    classSearch: adaptor.elementsByClass(queryRoot, 'b').map(pathOf),
    attributeSearch: adaptor.elementsByAttribute(queryRoot, 'data-v', 'x').map(pathOf)
  }
});

const mutationHTML = '<div id="parent"><b id="old">AB</b><!--c--><i id="tail">T</i></div><section id="other"></section>';
const mutationDoc = adaptor.parse(mutationHTML);
const parent = adaptor.getElement('#parent', mutationDoc.body);
const other = adaptor.getElement('#other', mutationDoc.body);
const old = adaptor.getElement('#old', mutationDoc.body);
const textNode = adaptor.childNode(old, 0);
const mutationSteps = [{ step: 'initial', tree: snapshotDocument(mutationDoc) }];
const shallowClone = adaptor.clone(parent, false);
const deepClone = adaptor.clone(parent, true);
mutationSteps.push({ step: 'clone-shallow-and-deep', shallow: snapshotNode(shallowClone), deep: snapshotNode(deepClone) });
const inserted = adaptor.node('em', { id: 'inserted', title: 'new' }, [adaptor.text('I')]);
adaptor.insert(inserted, old);
mutationSteps.push({ step: 'insert-before-old', tree: snapshotDocument(mutationDoc) });
adaptor.append(other, inserted);
mutationSteps.push({ step: 'move-by-append', tree: snapshotDocument(mutationDoc), oldParent: snapshotNode(parent), newParent: snapshotNode(other) });
const removed = adaptor.remove(inserted);
mutationSteps.push({ step: 'remove', detached: snapshotNode(removed), tree: snapshotDocument(mutationDoc) });
const replacement = adaptor.node('strong', { id: 'replacement' }, [adaptor.text('R')]);
const replaced = adaptor.replace(replacement, old);
mutationSteps.push({ step: 'replace', detached: snapshotNode(replaced), tree: snapshotDocument(mutationDoc) });
const splitSource = adaptor.text('abcd');
adaptor.append(parent, splitSource);
const splitTail = adaptor.split(splitSource, 2);
mutationSteps.push({ step: 'split-text-at-2', tree: snapshotDocument(mutationDoc), left: adaptor.value(splitSource), right: adaptor.value(splitTail) });
for (const [label, offset] of [['negative', -1], ['fractional', 1.5], ['nan', NaN], ['past-end', 9]]) {
  const probe = adaptor.text('wxyz');
  const edgeParent = adaptor.node('div');
  adaptor.append(edgeParent, probe);
  let result;
  try {
    const tail = adaptor.split(probe, offset);
    result = { threw: false, left: adaptor.value(probe), right: adaptor.value(tail), tree: snapshotNode(edgeParent),
      leftParent: !!adaptor.parent(probe), rightParent: !!adaptor.parent(tail) };
  } catch (error) { result = { threw: true, name: error.name, message: error.message,
    valueAfter: adaptor.value(probe), tree: snapshotNode(edgeParent), parentAfter: !!adaptor.parent(probe) }; }
  mutationSteps.push({ step: `split-attached-${label}`, offset: Number.isNaN(offset) ? 'NaN' : offset, result });
}
const parentless = adaptor.text('orphan');
let parentlessFailure;
try { adaptor.split(parentless, 3); parentlessFailure = { threw: false }; }
catch (error) { parentlessFailure = { threw: true, name: error.name, message: error.message, valueAfter: adaptor.value(parentless), parentAfter: !!adaptor.parent(parentless) }; }
mutationSteps.push({ step: 'parentless-split-mutates-before-failure', ...parentlessFailure });
const comment = adaptor.childNode(parent, 1);
const cloneObservation = node => {
  try { const clone = adaptor.clone(node, true); return { clone: snapshotNode(clone), parent: !!adaptor.parent(clone) }; }
  catch (error) { return { error: { name: error.name, message: error.message } }; }
};
mutationSteps.push({ step: 'text-comment-clone-parent-difference', text: cloneObservation(textNode), comment: cloneObservation(comment) });
const identityTarget = adaptor.node('u', { id: 'identity-target' }, [adaptor.text('x')]);
adaptor.append(parent, identityTarget);
const identityBefore = { child: adaptor.childNode(identityTarget, 0), parent: adaptor.parent(identityTarget) };
const identityReplacement = adaptor.node('b', {}, []);
const identityOld = adaptor.replace(identityReplacement, identityTarget);
mutationSteps.push({ step: 'replace-identity-and-lifecycle', sameOld: identityOld === identityTarget,
  detachedParent: adaptor.parent(identityOld) === null, replacementInParent: adaptor.parent(identityReplacement) === null,
  childIdentitySurvives: adaptor.childNode(identityOld, 0) === identityBefore.child, oldParentBeforeWasNull: identityBefore.parent === null });
records.push({
  id: 'mutations/clone-insert-move-remove-replace-split', area: 'mutation/tree',
  input: { html: mutationHTML },
  observation: mutationSteps
});

const styleHTML = '<p id="styled" style="color: red; margin-left: 2px">x&amp;y</p>';
const styleDoc = adaptor.parse(styleHTML);
const styled = adaptor.getElement('#styled', styleDoc.body);
const styleBefore = { raw: adaptor.allStyles(styled), color: adaptor.getStyle(styled, 'color'), marginLeft: adaptor.getStyle(styled, 'margin-left') };
adaptor.setStyle(styled, 'color', 'blue');
adaptor.setStyle(styled, 'font-weight', 'bold');
records.push({
  id: 'style/get-set-and-order', area: 'style/attributes/serialize',
  input: { html: styleHTML },
  observation: {
    before: styleBefore,
    after: { raw: adaptor.allStyles(styled), color: adaptor.getStyle(styled, 'color'), fontWeight: adaptor.getStyle(styled, 'font-weight'),
      attributes: adaptor.allAttributes(styled).map(({ name, value }) => [name, value]) },
    html: adaptor.outerHTML(styled), xml: adaptor.serializeXML(styled)
  }
});

const parser = adaptor.parser;
const serializationHTML = '<math><mi title="a &amp; &quot;">x &lt; y &amp; z</mi></math>';
records.push({
  id: 'serialization/protection/xml-namespaces', area: 'serialize/protection',
  input: { html: serializationHTML },
  observation: (() => {
    const doc = adaptor.parse(serializationHTML);
    const math = adaptor.getElement('math', doc.body);
    const mi = adaptor.getElement('mi', doc.body);
    return {
      html: adaptor.outerHTML(math), xml: adaptor.serializeXML(math),
      innerHTML: adaptor.innerHTML(mi), xmlInner: parser.serializeInner(adaptor, mi, true),
      protectHTML: ['&<>', '"\''].map(value => parser.protectHTML(value)),
      protectAttributeHtml: ['&<>', '"\''].map(value => parser.protectAttribute(value, false)),
      protectAttributeXml: ['&<>', '"\''].map(value => parser.protectAttribute(value, true))
    };
  })()
});

const retryEvents = [];
mathjax.asyncLoad = file => {
  retryEvents.push(file);
  const resolved = path.resolve(cjsRoot, file);
  assert.ok(resolved.startsWith(path.join(cjsRoot, 'util/entities') + path.sep), `Refusing non-entity Source load: ${file}`);
  if (!fs.existsSync(resolved)) return Promise.reject(new Error(`Missing pinned entity shard: ${file}`));
  return Promise.resolve(require(resolved));
};
async function entityRetryRecord() {
  const html = '<p>&Aopf; &zLiteWitnessUnknown;</p>';
  let attempts = 0;
  const doc = await handleRetriesFor(() => {
    ++attempts;
    return adaptor.parse(html);
  });
  return {
    id: 'entities/async-load-and-parser-retry', area: 'entity/retry', input: { html },
    observation: {
      parseAttempts: attempts, loadedFiles: retryEvents.slice(), translatedText: adaptor.textContent(doc.body),
      document: snapshotDocument(doc), loadedEntityKeys: ['Aopf', 'zLiteWitnessUnknown'].map(name => [name, entities.entities[name] || null])
    }
  };
}

function measurementRecord() {
  const html = '<p id="sized">漢</p>';
  const doc = adaptor.parse(html);
  const element = adaptor.getElement('#sized', doc.body);
  const text = adaptor.childNode(element, 0);
  const observe = node => {
    const capture = action => {
      try { return action(); }
      catch (error) { return { error: { name: error.name || '', message: String(error.message || error) } }; }
    };
    return {
      fontSize: capture(() => adaptor.fontSize(node)), fontFamily: capture(() => adaptor.fontFamily(node)),
      nodeSize: capture(() => adaptor.nodeSize(node)), nodeBBox: capture(() => adaptor.nodeBBox(node))
    };
  };
  const elementObservation = observe(element);
  elementObservation.nodeSizeEm2 = (() => {
    try { return adaptor.nodeSize(element, 2); }
    catch (error) { return { error: { name: error.name || '', message: String(error.message || error) } }; }
  })();
  elementObservation.nodeSizeLocalFalse = (() => {
    try { return adaptor.nodeSize(element, 1, false); }
    catch (error) { return { error: { name: error.name || '', message: String(error.message || error) } }; }
  })();
  return {
    id: 'measurement/NodeMixin-text-and-element', area: 'measurement/NodeMixin',
    input: { html }, observation: { element: elementObservation, text: observe(text) }
  };
}

async function entityEdgeRecords() {
  const out = [];
  const unknown = '<p>&notARealLiteEntity;</p>';
  retryEvents.length = 0;
  let attempts = 0;
  const unknownDoc = await handleRetriesFor(() => { ++attempts; return adaptor.parse(unknown); });
  out.push({ id: 'entities/unknown-name-with-present-shard', input: { html: unknown }, observation: {
    attempts, loadedFiles: retryEvents.slice(), text: adaptor.textContent(unknownDoc.body)
  }});
  out.push({ id: 'parse/unknown-entity-left-literal', area: 'parse/tree/serialize',
    input: { html: unknownParserCase[1], format: 'text/html' }, observation: {
      document: snapshotDocument(unknownDoc), bodyText: adaptor.textContent(unknownDoc.body),
      bodyInnerHTML: adaptor.innerHTML(unknownDoc.body), rootHTML: adaptor.outerHTML(unknownDoc.root),
      rootXML: adaptor.serializeXML(unknownDoc.root)
    }});
  const disabled = require(path.join(cjsRoot, 'util/Entities.js'));
  disabled.options.loadMissingEntities = false;
  const disabledText = disabled.translate('&notARealLiteEntity;');
  disabled.options.loadMissingEntities = true;
  out.push({ id: 'entities/loading-disabled', input: { text: '&notARealLiteEntity;' }, observation: { text: disabledText, loaderCalls: [] } });
  let missingShard;
  try { await mathjax.asyncLoad('./util/entities/liteWitnessMissingShard.js'); missingShard = { rejected: false }; }
  catch (error) { missingShard = { rejected: true, name: error.name || '', message: String(error.message || error) }; }
  out.push({ id: 'entities/missing-shard-loader-rejection', input: { path: './util/entities/liteWitnessMissingShard.js' }, observation: missingShard });
  const entityPath = path.join(cjsRoot, 'util/Entities.js');
  const shardsDir = path.join(cjsRoot, 'util/entities') + path.sep;
  for (const key of Object.keys(require.cache)) if (key === entityPath || key.startsWith(shardsDir)) delete require.cache[key];
  const freshEntities = require(entityPath);
  mathjax.asyncLoad = file => {
    retryEvents.push(file);
    const resolved = path.resolve(cjsRoot, file);
    if (!fs.existsSync(resolved)) return Promise.reject(new Error(`Missing pinned entity shard: ${file}`));
    assert.ok(resolved.startsWith(shardsDir));
    return Promise.resolve(require(resolved));
  };
  async function translateWithRetries(input) {
    let tries = 0;
    for (; tries < 5; ++tries) {
      try { return { value: freshEntities.translate(input), error: null, tries: tries + 1 }; }
      catch (error) {
        if (!(error.retry instanceof Promise)) return { value: '', error: { name: error.name, message: error.message }, tries: tries + 1 };
        await error.retry;
      }
    }
    throw new Error('Source entity retry did not finish');
  }
  retryEvents.length = 0;
  const numericFirst = await translateWithRetries('&#1114112;&nle;');
  out.push({ id: 'entities/invalid-numeric-before-missing-shard-retry', input: '&#1114112;&nle;', observation: { ...numericFirst, loadedFiles: retryEvents.slice() } });
  retryEvents.length = 0;
  const shardFirst = await translateWithRetries('&nle;&#1114112;');
  out.push({ id: 'entities/missing-shard-retry-before-invalid-numeric', input: '&nle;&#1114112;', observation: { ...shardFirst, loadedFiles: retryEvents.slice() } });
  return out;
}

async function main() {
  records.push(measurementRecord(), await entityRetryRecord(), ...await entityEdgeRecords());
  const loadedCjs = Object.keys(require.cache)
    .filter(file => path.resolve(file).startsWith(cjsRoot + path.sep))
    .map(file => path.relative(root, file).replaceAll('\\', '/')).sort();
  const expectedLoadedCjs = Object.keys(expectedCjsInputs)
    .filter(file => !file.endsWith('/util/entities/opf.js') && !file.endsWith('/util/entities/z.js')).sort();
  assert.deepEqual(loadedCjs, expectedLoadedCjs, 'Loaded Lite CJS dependency closure changed');
  for (const file of loadedCjs) {
    assert.ok(Object.hasOwn(expectedCjsInputs, file), `Unpinned CJS dependency was loaded: ${file}`);
  }
  const htmlTestCases = file => {
    const source = fs.readFileSync(path.join(root, file), 'utf8');
    return [...source.matchAll(/\b(?:it|test)\(\s*(['"])(.*?)\1/gs)].map(match => match[2]);
  };
  const upstreamSuites = {
    'Html.test.ts': htmlTestCases('.reference/MathJax-src/testsuite/tests/input/tex/Html.test.ts'),
    'TexHtml.test.ts': htmlTestCases('.reference/MathJax-src/testsuite/tests/input/tex/TexHtml.test.ts')
  };
  assert.equal(upstreamSuites['Html.test.ts'].length, 17);
  assert.equal(upstreamSuites['TexHtml.test.ts'].length, 7);
  const report = {
    reference: { mathjaxVersion: VERSION, mathjaxCommit: commit },
    inputs: {
      source: Object.entries(expectedSourceInputs).map(([file, sha256]) => ({ file, sha256 })),
      cjsRuntime: loadedCjs.map(file => ({ file, sha256: expectedCjsInputs[file] }))
    },
    upstreamSuiteReferences: {
      note: 'These are existing integration case names, not direct LiteParser unit tests.',
      counts: { Html: upstreamSuites['Html.test.ts'].length, TexHtml: upstreamSuites['TexHtml.test.ts'].length },
      cases: upstreamSuites
    },
    scope: {
      sourceBehaviorOnly: true,
      records: records.length,
      parserHtmlInputs: htmlInputs.length,
      areas: ['HTML parser subset', 'ordered attributes', 'text/comments', 'entity decoding and retry',
        'Source PCDATA table (including title/textarea)', 'document normalization/doctype', 'clone/insert/remove/replace/split',
        'simple query selectors', 'HTML/XML serialization', 'protection helpers', 'style parsing and edits'],
      limitations: ['LiteParser documents that it handles most well-formed HTML, not all browser parsing rules.',
        'No claim of browser DOM, CSSOM, namespace, scripting, or general selector conformance.',
        'RCDATA cases record the behavior of the Source PCDATA table, not a browser RCDATA implementation.']
    },
    records
  };
  const {utf16Field, binary64, writeTables} = require('./fixture-data.cjs');
  const kindFor = id => id.startsWith('parse/') ? 'parse' : id.startsWith('queries/') ? 'query' :
    id.startsWith('mutations/') ? 'mutation' : id.startsWith('style/') ? 'style' :
      id.startsWith('serialization/') ? 'serialization' : id.startsWith('measurement/') ? 'measurement' : 'entity';
  const queryRecord = report.records.find(row => row.id === 'queries/simple-selector-contract');
  const selectors = queryRecord.input.selectors;
  const mutation = report.records.find(row => row.id === 'mutations/clone-insert-move-remove-replace-split');
  const measurement = report.records.find(row => row.id === 'measurement/NodeMixin-text-and-element');
  const elementMetric = measurement.observation.element;
  const textMetric = measurement.observation.text;
  const number = value => Number.isFinite(value) ? value : 0;
  const point = name => typeof elementMetric.nodeBBox?.[name] === 'number'
    ? number(elementMetric.nodeBBox[name]) : 0;
  let selectorCursor = 0;
  const cases = report.records.map(row => {
    const string = value => typeof value === 'string' ? value : '';
    const expectedText = typeof row.observation?.translatedText === 'string' ? row.observation.translatedText :
      string(row.observation?.text);
    const expectedDocument = row.observation?.document ? JSON.stringify(row.observation.document) : '';
    const selectorOffset = row.id === 'queries/simple-selector-contract' ? selectorCursor : 0;
    const selectorCount = row.id === 'queries/simple-selector-contract' ? selectors.length : 0;
    if (selectorCount) selectorCursor += selectorCount;
    return [utf16Field(row.id), kindFor(row.id),
      ...[string(row.input?.html), string(row.input?.format), string(row.input?.text), string(row.input?.path),
        string(row.input), expectedText, expectedDocument].map(utf16Field),
      selectorOffset, selectorCount, utf16Field(JSON.stringify(row.observation))];
  });
  const loads = report.records.flatMap(row => Array.isArray(row.observation?.loadedFiles)
    ? row.observation.loadedFiles.map((file, order) => [utf16Field(row.id), order, utf16Field(file)]) : []);
  const definitions = {
    "selectors": {columns: ["value_utf16"],
      rows: selectors.map(value => [utf16Field(value)])},
    "cases": {columns: ["id_utf16", "kind_kind", "html_utf16", "format_utf16", "text_utf16", "path_utf16", "rawInput_utf16", "expectedText_utf16", "expectedDocument_utf16", "selectorOffset_integer", "selectorCount_integer", "expectedObservation_utf16"],
      rows: cases},
    "mutations": {columns: ["name_utf16", "expected_utf16"],
      rows: mutation.observation.map(row => [utf16Field(row.step), utf16Field(JSON.stringify(row))])},
    "element-metric": {columns: ["fontSize_binary64", "fontFamily_utf16", "width_binary64", "height_binary64", "left_binary64", "top_binary64", "right_binary64", "bottom_binary64"],
      rows: [[binary64(number(elementMetric.fontSize)), utf16Field(elementMetric.fontFamily),
        ...[number(elementMetric.nodeSize[0]), number(elementMetric.nodeSize[1]),
          point('left'), point('top'), point('right'), point('bottom')].map(binary64)]]},
    "entity-loads": {columns: ["recordId_utf16", "order_integer", "file_utf16"],
      rows: loads},
    "text-error": {columns: ["throws_boolean", "name_utf16", "message_utf16"],
      rows: [[+Boolean(textMetric.nodeSize.error), utf16Field(textMetric.nodeSize.error?.name || ''),
        utf16Field(textMetric.nodeSize.error?.message || '')]]},
    "counts": {columns: ["fixtures_integer", "mutations_integer"],
      rows: [[report.records.length, mutation.observation.length]]},
  };
  const output = path.join(root, 'tests/math/fixtures/adaptors/lite');
  writeTables(output, definitions);
  const fixtureSha256 = crypto.createHash('sha256');
  for (const name of Object.keys(definitions)) fixtureSha256.update(fs.readFileSync(path.join(output, `${name}.tsv`)));
  console.log(JSON.stringify({ accepted: true, version: VERSION, commit, records: records.length,
    parserHtmlInputs: htmlInputs.length, upstreamCases: 24, cjsInputs: loadedCjs.length,
    fixtureSha256: fixtureSha256.digest('hex'), output: path.relative(root, output) }));

}

main().catch(error => {
  console.error(error && error.stack ? error.stack : String(error));
  process.exitCode = 1;
});
