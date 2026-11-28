'use strict';

const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
assert.ok(process.argv.length === 2 ||
  (process.argv.length === 3 && process.argv[2] === '--write-fixture'),
  'usage: common-semantics-fenced-mathml-source.cjs [--write-fixture]');

const root = path.resolve(__dirname, '../..');
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const casesPath = path.join(__dirname, 'common-semantics-fenced-mathml-cases.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const cases = JSON.parse(fs.readFileSync(casesPath, 'utf8'));
assert.equal(cases.source.commit, scope.upstream.commit);
const {createMathmlSourceRuntime} = require('./common-mathml-source-runtime.cjs');
const runtime = createMathmlSourceRuntime({root, scope, scopePath, captureWrapperBBoxes: false});

function findNodes(node, kind, result = []) {
  if (node.kind === kind) result.push(node);
  node.children.forEach(child => findNodes(child, kind, result));
  return result;
}

function validateSource(records) {
  for (const entry of cases.cases) {
    const record = records.find(item => item.name === entry.name);
    assert.ok(record && record.svg, `${entry.name}: Source SVG missing`);
    if (entry.group === 'semantics') {
      const semantics = findNodes(record.mml, 'semantics');
      assert.ok(semantics.length > 0, `${entry.name}: Source MML semantics node missing`);
      const hiddenTexts = findNodes(record.mml, 'text').map(node => node.text || '');
      const marker = entry.mathml.match(/SOURCE-HIDDEN-SEMANTICS-[A-Z]+/)?.[0];
      assert.ok(marker && hiddenTexts.includes(marker), `${entry.name}: annotation TextNode capture missing`);
      assert.ok(!record.svg.includes(marker), `${entry.name}: hidden annotation leaked into SVG`);
      if (entry.name === 'semantics-presentation-hidden-annotations') {
        const xmlAnnotations = findNodes(record.mml, 'annotation-xml');
        assert.equal(xmlAnnotations.length, 1, 'Source must preserve the empty annotation-xml node');
        assert.equal(xmlAnnotations[0].children.length, 0, 'annotation-xml payload must remain empty');
      }
      if (entry.name === 'semantics-scaled-nested-presentation') {
        const style = findNodes(record.mml, 'mstyle').find(node => node.attributes.mathsize === '150%');
        assert.ok(style && findNodes(style, 'mrow').length > 0,
          'scaled semantics presentation must retain nested mstyle/mrow structure');
      }
      if (entry.name.startsWith('semantics-forced-break-')) {
        const sub = findNodes(record.mml, 'msub');
        const expectedStyle = entry.name.includes('before') ? 'before' : 'after';
        assert.ok(sub.some(node => findNodes(node, 'mo').some(mo =>
          mo.attributes.linebreak === 'newline' && mo.attributes.linebreakstyle === expectedStyle)),
          `${entry.name}: Source MML must retain the forced break on an embellished operator`);
        const lineboxes = [...record.svg.matchAll(/<g data-mjx-linebox="true"[^>]*>/g)];
        assert.ok(record.svg.includes('data-mjx-linestack="true"') && lineboxes.length > 1,
          `${entry.name}: forced inline break must produce multiple Source line boxes`);
        const firstLine = record.svg.slice(lineboxes[0].index, lineboxes[1].index);
        const secondLine = record.svg.slice(lineboxes[1].index);
        const operatorFirst = firstLine.includes('data-c="2B"');
        const identifierFirst = firstLine.includes('data-c="1D465"');
        assert.ok(expectedStyle === 'before'
          ? identifierFirst && !operatorFirst && secondLine.includes('data-c="2B"')
          : operatorFirst && secondLine.includes('data-c="1D465"'),
        `${entry.name}: Source line boxes must reflect linebreakstyle=${expectedStyle}`);
      }
    } else {
      const fenced = findNodes(record.mml, 'mfenced');
      assert.ok(fenced.length > 0, `${entry.name}: Source MML mfenced node missing`);
      if (entry.name === 'mfenced-custom-repeated-separator') {
        assert.ok(record.svg.includes('data-c="5B"') && record.svg.includes('data-c="5D"') &&
          record.svg.includes('data-c="3B"'), 'custom fence/separator glyphs missing from Source');
        assert.equal((record.svg.match(/data-c="7C"/g) || []).length, 2,
          'last custom separator must repeat for remaining Source gaps');
      }
      if (entry.name === 'mfenced-empty-separator')
        assert.ok(!record.svg.includes('data-c="2C"'), 'empty separators must emit no comma');
      if (entry.name === 'mfenced-absent-open-close')
        assert.ok(!record.svg.includes('data-c="28"') && !record.svg.includes('data-c="29"'),
          'empty open/close strings must emit no Source fence glyphs');
      if (entry.name === 'mfenced-nested')
        assert.ok((record.svg.match(/data-c="28"/g) || []).length === 1 &&
          (record.svg.match(/data-c="5B"/g) || []).length === 1,
          'nested Source fences must retain both delimiter pairs');
      if (entry.name === 'mfenced-tall-fraction-content')
        assert.ok(findNodes(record.mml, 'mfrac').length === 1,
          'tall fenced input must retain its actual fraction subtree');
      if (entry.name === 'mfenced-empty-children')
        assert.equal(fenced[0].children.length, 0, 'empty mfenced input must remain childless');
    }
  }
}






async function main() {
  const records = [];
  for (const entry of cases.cases) {
    const captured = await runtime.convert(entry, {
      em: entry.metrics.em, ex: entry.metrics.ex,
      containerWidth: entry.metrics.container_width, display: entry.metrics.display
    });
    records.push({...entry, ...captured});
  }
  validateSource(records);
  const {provenance, modules} = runtime.provenance('MathML', casesPath);

  const {writeObservations, sourceCase} = require('./mathml-observation-data.cjs');
  const observations = {
    sourceSemanticsCases: records.filter(r => r.group === 'semantics').map(sourceCase),
    sourceMfencedCases: records.filter(r => r.group === 'mfenced').map(sourceCase)
  };
  const evidencePath = path.join(root, 'build/math-port/common-source/semantics-fenced-mathml-scene.json');
  fs.mkdirSync(path.dirname(evidencePath), {recursive: true});
  fs.writeFileSync(evidencePath, JSON.stringify({provenance, modules, records}, null, 2) + '\n');
  writeObservations('semantics-fenced', observations);
  process.stdout.write(`Verified ${records.length} direct MathML semantics/fenced Source cases\n`);
}

main().catch(error => {process.stderr.write(error.stack + '\n'); process.exitCode = 1;});
