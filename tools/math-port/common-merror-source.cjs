'use strict';

// Independent pinned MathML -> SVG source witness for the SVG merror wrapper.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

assert.ok(process.argv.length === 2 ||
  (process.argv.length === 3 && process.argv[2] === '--write-fixture'),
  'usage: common-merror-source.cjs [--write-fixture]');

const root = path.resolve(__dirname, '../..');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const cases = [
  {name: 'merror-basic-text', mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><merror><mrow><mi>x</mi><mo>+</mo><mi>y</mi></mrow></merror></math>'},
  {name: 'merror-empty-title', mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><merror title=""><mi>x</mi></merror></math>'},
  {name: 'merror-escaped-title', mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><merror title="Title &amp; &quot;quoted&quot; &lt;tag&gt;"><mi>x</mi></merror></math>'},
  {name: 'merror-multiple-children', mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><merror><mi>a</mi><mo>+</mo><mi>b</mi></merror></math>'},
  {name: 'merror-nested', mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><merror title="outer"><mrow><mi>a</mi><merror title="inner"><mrow><mi>x</mi><mo>+</mo><mi>y</mi></mrow></merror></mrow></merror></math>'},
  {name: 'merror-authored-blue', mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><merror><mi mathcolor="blue">x</mi><mi mathcolor="blue">y</mi></merror></math>'},
  {name: 'merror-linked-nested-svg-child', mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><merror><mrow href="https://example.test/merror"><mo stretchy="true" minsize="5em">(</mo><mi>x</mi><mo stretchy="true" minsize="5em">)</mo></mrow></merror></math>'},
].map(entry => ({...entry, metrics: {em: 16, ex: 8, container_width: 640, display: false}}));

const {createMathmlSourceRuntime} = require('./common-mathml-source-runtime.cjs');
const runtime = createMathmlSourceRuntime({root, scope, scopePath,
  observeWrapper(wrapper, result) {
    if (wrapper.node.kind !== 'merror') return;
    const box = wrapper.getBBox();
    result.merror = {
      bbox: {w: box.w, h: box.h, d: box.d, L: box.L, R: box.R},
      title: wrapper.node.attributes.get('title') || '',
      styles: wrapper.constructor.styles
    };
  }
});

function findNodes(node, kind, output = []) {
  if (node.kind === kind) output.push(node);
  node.children.forEach(child => findNodes(child, kind, output));
  return output;
}

const expectedStyles = {
  'g[data-mml-node="merror"] > g': {fill: 'red', stroke: 'red'},
  'g[data-mml-node="merror"] > rect[data-background]': {fill: 'yellow', stroke: 'none'}
};

function validate(records) {
  for (const record of records) {
    const merrors = findNodes(record.mml, 'merror');
    const wrapperRecords = record.wrappers.filter(wrapper => wrapper.kind === 'merror');
    assert.equal(wrapperRecords.length, merrors.length, `${record.name}: Source merror wrappers captured`);
    assert.ok(merrors.length > 0, `${record.name}: Source merror MML node present`);
    assert.ok(wrapperRecords.every(wrapper => wrapper.merror &&
      Object.values(wrapper.merror.bbox).every(Number.isFinite)),
    `${record.name}: Source merror bbox and class styles captured`);
    for (const wrapper of wrapperRecords)
      assert.deepEqual(wrapper.merror.styles, expectedStyles,
        `${record.name}: actual SvgMerror class style rules remain pinned`);
    const backgrounds = (record.svg.match(/<rect\b[^>]*data-background/g) || []).length;
    assert.equal(backgrounds, merrors.length, `${record.name}: one Source background rect per merror`);
    if (record.name === 'merror-empty-title')
      assert.equal((record.svg.match(/<title\b/g) || []).length, 0,
        'empty Source title is omitted');
    if (record.name === 'merror-escaped-title') {
      assert.ok(record.svg.includes('<title>Title & "quoted" <tag></title>'),
        'Source LiteAdaptor serializes the captured title text verbatim');
      assert.equal(merrors[0].attributes.title, 'Title & "quoted" <tag>',
        'captured Source MML retains the authored title value');
    }
    if (record.name === 'merror-multiple-children') {
      assert.equal(findNodes(merrors[0], 'mi').length, 2,
        'multiple authored identifier children remain in Source MML');
      assert.equal(findNodes(merrors[0], 'mo').length, 1,
        'multiple authored operator children remain in Source MML');
    }
    if (record.name === 'merror-nested')
      assert.equal(merrors.length, 2, 'nested Source merror remains nested');
    if (record.name === 'merror-authored-blue') {
      const blueChildren = findNodes(merrors[0], 'mi')
        .filter(node => node.attributes.mathcolor === 'blue');
      assert.equal(blueChildren.length, 2, 'authored blue child colors remain in Source MML');
      assert.ok((record.svg.match(/fill="blue"/g) || []).length >= 2,
        'authored SVG blue fill survives the red merror class selector');
    }
    if (record.name === 'merror-linked-nested-svg-child') {
      assert.ok(record.svg.includes('href="https://example.test/merror"'),
        'Source child link remains authored');
      assert.ok((record.svg.match(/<svg\b/g) || []).length > 1,
        'Source assembled operator child is emitted as nested SVG');
    }
  }
}





async function main() {
  const records = [];
  for (const entry of cases)
    records.push({...entry, ...await runtime.convert(entry, {
      em: entry.metrics.em, ex: entry.metrics.ex,
      containerWidth: entry.metrics.container_width, display: entry.metrics.display
    })});
  validate(records);
  const {provenance, modules} = runtime.provenance('MathML', __filename);

  const {writeObservations, sourceCase} = require('./mathml-observation-data.cjs');
  const observedStyles = records.flatMap(r => r.wrappers.filter(w => w.kind === 'merror').map(w => w.merror.styles));
  assert.ok(observedStyles.length);
  assert.deepEqual(observedStyles[0], expectedStyles);
  const observations = {
    sourceMerrorStyleRules: Object.entries(observedStyles[0]).map(([selector, paint]) =>
      ({selector, fill: paint.fill, stroke: paint.stroke})),
    sourceMerrorCases: records.map(record => {
      const errors = findNodes(record.mml, 'merror');
      const observed = record.wrappers.filter(w => w.kind === 'merror');
      const unused = new Set(observed);
      const boxes = errors.map(node => {
        const match = observed.find(w => unused.has(w) && w.merror.title === (node.attributes.title || ''));
        assert.ok(match, `${record.name}: missing observed merror`); unused.delete(match);
        const b = match.merror.bbox;
        return {width: b.w, height: b.h, depth: b.d, left: b.L, right: b.R};
      });
      assert.equal(unused.size, 0);
      assert.ok(boxes.length > 0 && boxes.length <= 2);
      return {source: sourceCase(record), box_count: boxes.length,
        boxes: [...boxes, ...Array.from({length: 2 - boxes.length}, () =>
          ({width: 0, height: 0, depth: 0, left: 0, right: 0}))]};
    })
  };
  const evidencePath = path.join(root, 'build/math-port/common-source/merror-mathml-scene.json');
  fs.mkdirSync(path.dirname(evidencePath), {recursive: true});
  fs.writeFileSync(evidencePath, JSON.stringify({provenance, modules, cases: records}, null, 2) + '\n');
  writeObservations('merror', observations);
  process.stdout.write(`Verified ${records.length} direct MathML merror Source cases\n`);
}

main().catch(error => {process.stderr.write(error.stack + '\n'); process.exitCode = 1;});
