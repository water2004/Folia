'use strict';
// Execute MathJax 4.1.3's shipped CommonMrow/LineBBox implementation with a
// pinned source lock and record the real factory-created row geometry.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
function repositoryRoot() {
  const arg = process.argv.indexOf('--repo-root');
  if (arg >= 0 && (!process.argv[arg + 1] || process.argv.length !== arg + 2))
    throw new Error('usage: node <witness> [--repo-root <repository>]');
  const root = arg >= 0 ? path.resolve(process.argv[arg + 1]) : findRepositoryRoot();
  assert.ok(root, 'pass --repo-root <repository> or run the witness from within the repository');
  return root;
}
const root = repositoryRoot();
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot, 'src/package.json'))).version, '4.1.3');
assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot, 'mathjax-newcm-font/package.json'))).version, '4.1.3');
const sourceRoot = path.join(root, '.reference/MathJax-src/ts');
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const sourceFile = 'output/common/Wrappers/mrow.ts';
const sourceHash = '7e363c64d653d9cebd18c9e70784018c6f44b29b09bc26847b921b47e696e5d9';
assert.equal(hash(path.join(sourceRoot, sourceFile)), sourceHash, 'pinned mrow Source changed');
assert.equal(hash(path.join(sourceRoot, 'output/common/LineBBox.ts')),
  '42646988f6914b044044c4fe6edd700e6b68e1b6b16524ae0282d5715723326a',
  'pinned LineBBox Source changed');
const moduleRequire = require('node:module').createRequire(__filename);
const load = file => moduleRequire(path.join(packageRoot, 'src/cjs', file));
assert.equal(hash(path.join(packageRoot, 'src/cjs/output/common/Wrappers/mrow.js')),
  '656d226a08412290c90284fa5ab0b311b288e8aaeb33c257d8d99e20ee920776',
  'pinned CommonMrow CJS changed');
assert.equal(hash(path.join(packageRoot, 'src/cjs/output/common/LineBBox.js')),
  '5e6464e467ef82116d6933af020450bacb354d518848049188cd0dde0b2eadd2',
  'pinned LineBBox CJS changed');
const { mathjax } = load('mathjax.js');
const { TeX } = load('input/tex.js');
const { SVG } = load('output/svg.js');
const { liteAdaptor } = load('adaptors/liteAdaptor.js');
const { RegisterHTMLHandler } = load('handlers/html.js');
const { MathJaxNewcmFont } = moduleRequire(path.join(packageRoot, 'mathjax-newcm-font/cjs/svg.js'));
const adaptor = liteAdaptor({ fontSize: 16 });
RegisterHTMLHandler(adaptor);
const input = new TeX({ packages: ['base'] });
const output = new SVG({ fontData: MathJaxNewcmFont, fontCache: 'none', exFactor: .5,
  displayOverflow: 'linebreak', linebreaks: { inline: true, width: '100%' } });
const document = mathjax.document('', { InputJax: input, OutputJax: output });
let rowRecord;
let reflowRecord;
const processMath = output.processMath;
output.processMath = function (wrapper, ...args) {
  processMath.call(this, wrapper, ...args);
  const rows = [];
  let brokenRow;
  const visit = current => {
    if (current.constructor.name.includes('Mrow')) {
      const box = current.getBBox();
      if (!brokenRow && current.breakCount > 0) brokenRow = current;
      rows.push({ inferred: !!current.node.isInferred, isStack: current.isStack,
        breakCount: current.breakCount, lineCount: current.lineBBox.length,
        bbox: { w: box.w, h: box.h, d: box.d, pwidth: box.pwidth },
        lines: current.lineBBox.map(line => ({ w: line.w, h: line.h, d: line.d,
          L: line.L, R: line.R, leading: line.lineLeading ?? null })) });
    }
    for (const child of current.childNodes) visit(child);
  };
  visit(wrapper);
  rowRecord = rows;
  assert.ok(brokenRow, 'Source factory did not produce a broken row');
  for (const [name, value] of Object.entries({indentalign: 'left', indentshift: '1em',
    indentalignfirst: 'left', indentshiftfirst: '1em'})) brokenRow.node.attributes.set(name, value);
  for (const line of brokenRow.lineBBox) line.getIndentData(brokenRow.node);
  reflowRecord = [];
  for (const width of [6, 8, 10]) {
    brokenRow.setChildPWidths(false, width);
    const lefts = brokenRow.lineBBox.map(line => line.L);
    assert.ok(lefts.every(left => Math.abs(left - 1) < 1e-12), 'left indent accumulated');
    reflowRecord.push({width, lefts});
  }
};

(async () => {
  await document.convertPromise('a+b+c+d+e+f+g+h+i+j', {
    display: true, em: 16, ex: 8, containerWidth: 100,
  });
  const record = rowRecord;
  assert.ok(record?.length > 0, 'Source factory did not create mrow wrappers');
  assert.ok(record.some(row => row.breakCount > 0), 'fixture did not exercise row line breaking');
  const evidence = { sourceFile, sourceHash, tex: 'a+b+c+d+e+f+g+h+i+j', display: true, width: 100, rows: record, reflow: reflowRecord };
  const output = path.join(root, 'build/math-port/common-source/row.json');
  fs.mkdirSync(path.dirname(output), {recursive: true});
  fs.writeFileSync(output, JSON.stringify(evidence, null, 2) + '\n');
  process.stdout.write(JSON.stringify(evidence, null, 2) + '\n');
})().catch(error => { process.stderr.write(error.stack + '\n'); process.exitCode = 1; });
