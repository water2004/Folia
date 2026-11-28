'use strict';
// Pinned MathJax 4.1.3 SVG/NewCM geometry observation for the native Common
// container fixtures. This runs real MathML/TeX input through SVG wrappers;
// no mock wrapper geometry participates.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
function findRoot(start=__dirname){for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))if(fs.existsSync(path.join(current,'tools/math-port/profiles/scope.json'))&&fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')))return current;return null;}
const rootArg=process.argv.indexOf('--repo-root');
assert.ok(rootArg<0?process.argv.length===2:(process.argv[rootArg+1]&&process.argv.length===rootArg+2),'usage: node common-containers-geometry-source-witness.cjs [--repo-root <repository>]');
const root=rootArg<0?findRoot():path.resolve(process.argv[rootArg+1]);
assert.ok(root,'pass --repo-root <repository> or run from the repository');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const srcRoot = path.join(packageRoot, 'src/cjs');
const scopeFile = path.join(root, 'tools/math-port/profiles/scope.json');
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const lock=JSON.parse(fs.readFileSync(path.join(root,'tests/math/fixtures/common-source/containers-source-lock.json'),'utf8'));
assert.equal(digest(scopeFile), lock.scopeSha256);
assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot,'src/package.json'))).version,lock.version,'MathJax source package version');
assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot,'mathjax-newcm-font/package.json'))).version,lock.version,'NewCM package version');
const scope = JSON.parse(fs.readFileSync(scopeFile, 'utf8'));
for (const [relative, expected] of Object.entries(lock.sourceTsSha256)) {
  const entry = scope.files.find(file => file.source === `ts/${relative}`);
  assert.equal(entry?.source_sha256, expected, `scope lock for ${relative}`);
  assert.equal(digest(path.join(root, '.reference/MathJax-src/ts', relative)), expected, relative);
}
for (const [relative, expected] of Object.entries(lock.cjsSha256))
  assert.equal(digest(path.join(packageRoot, relative)), expected, relative);
for (const name of ['src', 'mathjax-newcm-font'])
  assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot, name, 'package.json'))).version, '4.1.3');

const load = file => require(path.join(srcRoot, `${file}.js`));
const { mathjax } = load('mathjax');
const { MathML } = load('input/mathml');
const { TeX } = load('input/tex');
const { SVG } = load('output/svg');
const { liteAdaptor } = load('adaptors/liteAdaptor');
const { RegisterHTMLHandler } = load('handlers/html');
const { MathJaxNewcmFont } = require(path.join(packageRoot, 'mathjax-newcm-font/cjs/svg.js'));
const adaptor = liteAdaptor({ fontSize: 16 });
RegisterHTMLHandler(adaptor);
const output = new SVG({ fontData: MathJaxNewcmFont, fontCache: 'none', exFactor: .5,
  displayOverflow: 'linebreak', linebreaks: { inline: true } });
let active = null;
const originalProcessMath = output.processMath;
const box = value => Object.fromEntries(['w', 'h', 'd', 'ic', 'sk'].map(key => [key, value[key] ?? 0]));
output.processMath = function (wrapper, node) {
  const result = originalProcessMath.apply(this, arguments);
  if (active) active.wrappers = Array.from(this.nodeMap.values(), item => {
    const record = { kind: item.node.kind, text: item.node.getText?.() ?? null,
      bbox: box(item.getBBox()), children: item.childNodes.map(child => child.node.kind) };
    if (item.node.kind === 'mpadded') Object.assign(record, {
      dimensions: item.getDimens(), align: item.getChildAlign(0), wrapWidth: item.getWrapWidth(0),
      childBBox: item.childNodes[0] ? box(item.childNodes[0].getOuterBBox()) : null,
    });
    if (item.node.kind === 'semantics') Object.assign(record, {
      breakCount: item.breakCount, embellished: !!item.node.isEmbellished,
    });
    if (item.node.kind === 'TeXAtom') record.childIC = item.childNodes[0]?.getBBox().ic ?? 0;
    return record;
  });
  return result;
};
async function observe(name, input, source) {
  const document = mathjax.document('', { InputJax: input, OutputJax: output });
  active = { name, wrappers: [] };
  try { await document.convertPromise(source, { em: 16, ex: 8, containerWidth: 1280 }); }
  catch (error) { active.error = { name: error.name, message: String(error.message) }; }
  const record = active;
  active = null;
  return record;
}
function selected(record, kind, predicate = () => true) {
  const result = record.wrappers.find(item => item.kind === kind && predicate(item));
  assert.ok(result, `${record.name}: missing ${kind} wrapper`);
  return result;
}
(async () => {
  const records = [];
  records.push(await observe('mpadded-relative', new MathML(),
    '<math display="block"><mpadded width="+50%" height="-50%" depth="+25%" lspace="0.25em" voffset="-0.125em" data-align="right"><mrow><mi mathvariant="italic">x</mi></mrow></mpadded></math>'));
  records.push(await observe('mpadded-overflow', new MathML(),
    '<math><mpadded width="0em" data-overflow="linebreak"><mrow><mi mathvariant="italic">x</mi></mrow></mpadded></math>'));
  records.push(await observe('semantics-presentation', new MathML(),
    '<math><semantics><mi mathvariant="italic">x</mi><annotation-xml encoding="application/xml"><x/></annotation-xml></semantics></math>'));
  records.push(await observe('semantics-embellished-mo', new MathML(),
    '<math><semantics><mo linebreak="goodbreak">+</mo><annotation-xml encoding="application/xml"><x/></annotation-xml></semantics></math>'));
  records.push(await observe('mi-italic-bang', new MathML(),
    '<math><mi mathvariant="italic">!</mi></math>'));
  records.push(await observe('tex-atom', new TeX({ packages: ['base'] }),
    '\\mathord{\\mathit{!}}'));

  const relative = selected(records[0], 'mpadded');
  assert.deepEqual(relative.dimensions, [0.442, 0.011, 0.572, -0.221, 0.002749999999999999,
    0.2859999999999999, 0.25, -0.125, 0.2859999999999999]);
  assert.deepEqual(relative.bbox, { w: 0.8579999999999999, h: 0.221, d: 0.013749999999999998, ic: 0, sk: 0 });
  const overflow = selected(records[1], 'mpadded');
  assert.deepEqual(overflow.bbox, { w: 0, h: 0.442, d: 0.011, ic: 0, sk: 0 });
  const semantics = selected(records[2], 'semantics');
  assert.deepEqual(semantics.bbox, { w: 0.572, h: 0.442, d: 0.011, ic: 0, sk: 0 });
  assert.equal(semantics.breakCount, 0);
  assert.equal(semantics.embellished, false);
  const embellished = selected(records[3], 'semantics');
  assert.deepEqual(embellished.bbox, { w: 0.778, h: 0.583, d: 0.083, ic: 0, sk: 0 });
  assert.equal(embellished.breakCount, 0);
  assert.equal(embellished.embellished, true);
  const italicBang = selected(records[4], 'mi', item => item.text === '!');
  assert.deepEqual(italicBang.bbox, { w: 0.375, h: 0.716, d: 0, ic: 0.068, sk: 0 });
  const atom = selected(records[5], 'TeXAtom');
  assert.deepEqual(atom.bbox, { w: 0.375, h: 0.716, d: 0, ic: 0, sk: 0 });
  assert.equal(atom.childIC, 0);

  // Keep compact, reviewable observations alongside the locked live observer.
  const compact = records.map(record => ({ name: record.name, error: record.error ?? null,
    targets: record.name.startsWith('mpadded') ? [selected(record, 'mpadded')] :
      record.name.startsWith('semantics') ? [selected(record, 'semantics')] :
      record.name === 'mi-italic-bang' ? [selected(record, 'mi', item => item.text === '!')] :
      [selected(record, 'TeXAtom')] }));
  const expectedFile = path.join(root,'tests/math/fixtures/common-source/containers-source-geometry-expected.json');
  assert.deepEqual(compact, JSON.parse(fs.readFileSync(expectedFile, 'utf8')),
    'pinned MathJax SVG/NewCM geometry observations changed');
  const output=path.join(root,'build/math-port/common-source/containers-geometry-source.json');
  fs.mkdirSync(path.dirname(output),{recursive:true});
  fs.writeFileSync(output,JSON.stringify(compact,null,2)+'\n');
  process.stdout.write(`verified MathML/TeX to SVG/NewCM source geometry (${compact.length} cases); evidence ${path.relative(root,output)}\n`);
})().catch(error => { process.stderr.write(error.stack + '\n'); process.exitCode = 1; });
