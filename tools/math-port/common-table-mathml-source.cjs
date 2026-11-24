'use strict';

// MathML is used only to reach table attributes without a TeX profile entry.
// Native tests consume the actual compiled Source tree, not a second parser.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

assert.ok(process.argv.length === 2 ||
  (process.argv.length === 3 && process.argv[2] === '--write-fixture'),
  'usage: common-table-mathml-source.cjs [--write-fixture]');
const root = path.resolve(__dirname, '../..');
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scopePath = path.join(root, 'docs/math-port/scope.json');
const casesPath = path.join(__dirname, 'table-mathml-cases.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const cases = JSON.parse(fs.readFileSync(casesPath, 'utf8'));
assert.equal(cases.source.commit, scope.upstream.commit);
const {createMathmlSourceRuntime} = require('./common-mathml-source-runtime.cjs');
const runtime = createMathmlSourceRuntime({root, scope, scopePath,
  observeWrapper(wrapper, result) {
    if (wrapper.node.kind !== 'mtable') return;
    const {H, D, W, L} = wrapper.getTableData();
    Object.assign(result, {tableData: {H, D, W, L},
      rowRscales: wrapper.childNodes.map(row => row.getBBox().rscale)});
  }
});

function findNode(node, kind) {
  if (node.kind === kind) return node;
  for (const child of node.children) {
    const found = findNode(child, kind);
    if (found) return found;
  }
  return null;
}




async function main() {
  const records = [];
  for (const entry of cases.cases) {
    const captured = await runtime.convert(entry, cases.metrics);
    records.push({name: entry.name, mathml: entry.mathml, ...captured});
  }
  const {provenance, modules} = runtime.provenance('MathML', casesPath);

  const {writeObservations, treeMml} = require('./mathml-observation-data.cjs');
  const observations = {sourceTableCases: records.map(record => {
    const table = record.wrappers.find(w => w.kind === 'mtable');
    const tableNode = findNode(record.mml, 'mtable');
    assert.ok(table && tableNode, `${record.name}: missing compiled table`);
    const {H, D, W, L} = table.tableData;
    const {w, h, d, L: left, R: right} = table.bbox;
    assert.ok([w,h,d,left,right,L,...H,...D,...W,...table.rowRscales].every(Number.isFinite));
    const attrs = tableNode.attributes;
    return {name: record.name, metrics: {em: cases.metrics.em, ex: cases.metrics.ex,
      container_width: cases.metrics.containerWidth, display: cases.metrics.display},
      ...treeMml(record.mml), H, D, W, L, row_rscales: table.rowRscales,
      dotted_lines: (attrs.rowlines === 'dotted' ? H.length - 1 : 0) +
        (attrs.columnlines === 'dotted' ? W.length - 1 : 0),
      dotted_frames: attrs.frame === 'dotted' ? 1 : 0,
      bbox: {w,h,d,L:left,R:right}, source_svg: record.svg};
  })};
  const evidence = path.join(root, 'build/math-port/common-source/table-mathml-scene.json');
  fs.mkdirSync(path.dirname(evidence), {recursive: true});
  fs.writeFileSync(evidence, JSON.stringify({provenance, modules, records}, null, 2) + '\n');
  writeObservations('table', observations);
  process.stdout.write(`Verified ${records.length} direct MathML table Source cases\n`);
}

main().catch(error => {process.stderr.write(error.stack + '\n'); process.exitCode = 1;});
