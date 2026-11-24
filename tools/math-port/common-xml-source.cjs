'use strict';

// Independent pinned MathML -> Common -> SVG witness for XMLNode/foreignObject.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

assert.ok(process.argv.length === 2 ||
  (process.argv.length === 3 && process.argv[2] === '--write-fixture'),
  'usage: common-xml-source.cjs [--write-fixture]');
const root = path.resolve(__dirname, '../..');
const scopePath = path.join(root, 'docs/math-port/scope.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const cases = [
  {name: 'xml-unknown-measurement-mixed-payload', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'auto', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><section class="xml-probe" data-order="a&amp;b"><span title="A &amp; B">Alpha &amp; Beta</span><!--inside--><br/><svg xmlns="http://www.w3.org/2000/svg"><text>G</text></svg></section></annotation-xml></semantics></math>'},
  {name: 'xml-force-nonzero-hdw', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'force', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><div data-mjx-hdw="1em .25em 3em"><span>F</span></div></annotation-xml></semantics></math>'},
  {name: 'xml-use-nonzero-hdw', canMeasureNodes: true,
    outputOptions: {htmlHDW: 'use', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><div data-mjx-hdw=".75em .125em 2.5em"><i>U</i></div></annotation-xml></semantics></math>'},
  {name: 'xml-ignore-hdw-measures', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'ignore', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><div data-mjx-hdw="1em .25em 3em"><b>I</b></div></annotation-xml></semantics></math>'},
  {name: 'xml-automatic-measurable-policy', canMeasureNodes: true,
    outputOptions: {htmlHDW: 'auto', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><div data-mjx-hdw=".5em .2em 2em"><span>A</span></div></annotation-xml></semantics></math>'},
  {name: 'xml-automatic-force-policy', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'auto', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><div data-mjx-hdw=".5em .2em 2em"><span>B</span></div></annotation-xml></semantics></math>'},
  {name: 'xml-scale-not-one', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'ignore', scale: 2},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><article><em>S</em></article></annotation-xml></semantics></math>'},
  {name: 'xml-scale-two-force-hdw', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'force', scale: 2},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><div data-mjx-hdw="1em .25em 3em"><span>Scale</span></div></annotation-xml></semantics></math>'},
  // Source CommonWrapper consumes the authored parent font-family style into
  // removedStyles before CommonXmlNode.getFontStyles; this case deliberately
  // records the resulting Times fallback, not a hit on the authored-family tier.
  {name: 'xml-authored-font-style-removed-times-fallback', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'ignore', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml style="font-family: ProbeFamily" encoding="application/xhtml+xml"><div>font</div></annotation-xml></semantics></math>'},
  {name: 'xml-after-preceding-math', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'force', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><mi>x</mi><semantics><annotation-xml encoding="application/xhtml+xml"><div><br/><svg xmlns="http://www.w3.org/2000/svg"><text>Y</text></svg></div></annotation-xml></semantics></mrow></math>'},
  {name: 'xml-empty-element', canMeasureNodes: false,
    outputOptions: {htmlHDW: 'ignore', scale: 1},
    mathml: '<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><annotation-xml encoding="application/xhtml+xml"><div/></annotation-xml></semantics></math>'}
].map(entry => ({...entry, metrics: {em: 16, ex: 8, container_width: 640, display: false}}));

const {createMathmlSourceRuntime} = require('./common-mathml-source-runtime.cjs');
const runtime = createMathmlSourceRuntime({root, scope, scopePath,
  captureXmlNode(node, sourceNode, adaptor) {
    assert.ok(sourceNode && adaptor, 'Source XMLNode must retain its real Lite payload and adaptor');
    function visit(current) {
      const kind = adaptor.kind(current);
      const result = {kind, attributes: {}, children: []};
      if (kind === '#text' || kind === '#comment') {
        result.text = adaptor.value(current);
        return result;
      }
      for (const attribute of adaptor.allAttributes(current))
        result.attributes[attribute.name] = attribute.value;
      result.children = adaptor.childNodes(current).map(visit);
      return result;
    }
    const payload = visit(sourceNode);
    payload.serialized = adaptor.outerHTML(sourceNode);
    return payload;
  },
  observeWrapper(wrapper, result) {
    if (wrapper.node.kind === 'annotation-xml') {
      result.annotationXmlStyles = wrapper.constructor.styles;
      return;
    }
    if (wrapper.node.kind !== 'XML') return;
    const html = wrapper.getHTML();
    const start = wrapper.jax.math.start.node;
    const measurementParent = wrapper.adaptor.parent(start);
    const bbox = wrapper.getBBox();
    result.xml = {
      bbox: Object.fromEntries(['w', 'h', 'd', 'scale', 'rscale'].map(key => [key, bbox[key]])),
      metrics: {em: wrapper.metrics.em, ex: wrapper.metrics.ex, scale: wrapper.metrics.scale,
        family: wrapper.metrics.family || null},
      htmlHDW: wrapper.jax.options.htmlHDW,
      canMeasureNodes: wrapper.adaptor.canMeasureNodes,
      outputScale: wrapper.jax.options.scale,
      styles: wrapper.getFontStyles(),
      getHTML: wrapper.adaptor.outerHTML(html),
      wrapperStyles: wrapper.constructor.styles,
      measurementParent: {startKind: wrapper.adaptor.kind(start),
        parentKind: measurementParent ? wrapper.adaptor.kind(measurementParent) : null}
    };
  }
});




function find(node, kind, output = []) {
  if (node.kind === kind) output.push(node);
  node.children.forEach(child => find(child, kind, output));
  return output;
}
function validate(records) {
  for (const record of records) {
    const xml = find(record.mml, 'XML');
    const wrappers = record.wrappers.filter(wrapper => wrapper.kind === 'XML');
    assert.ok(xml.length > 0, `${record.name}: compiled Source MML has XMLNode`);
    assert.equal(xml.length, wrappers.length, `${record.name}: each XMLNode wrapper observed`);
    assert.ok(record.svg.includes('<foreignObject'), `${record.name}: Source emits foreignObject`);
    assert.ok(wrappers.every(wrapper => wrapper.xml?.getHTML.includes('mjx-html')),
      `${record.name}: capture actual CommonXmlNode.getHTML result`);
    const annotationWrappers = record.wrappers.filter(wrapper => wrapper.kind === 'annotation-xml');
    assert.ok(annotationWrappers.length > 0 &&
      annotationWrappers.every(wrapper => wrapper.annotationXmlStyles),
      `${record.name}: capture actual Source annotation-xml constructor.styles`);
    if (record.name === 'xml-unknown-measurement-mixed-payload') {
      const payload = xml[0].xml;
      assert.ok(payload.serialized.includes('section') && payload.serialized.includes('data-order="a&amp;b"'),
        'Source frozen XML payload retains ordered attributes and escaped text');
      assert.ok(payload.serialized.includes('<!--inside-->') && payload.serialized.includes('<br'),
        'nested comments and HTML void elements are in Source payload');
      assert.ok(find(payload, 'svg').length && find(payload, 'text').length,
        'mixed SVG namespace subtree remains nested in the Source payload');
      assert.ok(Object.values(wrappers[0].xml.bbox).slice(0, 3).every(value => value === 0),
        'actual pinned Lite nodeBBox makes unknown content dimensions zero');
    }
    if (record.name === 'xml-force-nonzero-hdw' || record.name === 'xml-use-nonzero-hdw' ||
        record.name === 'xml-automatic-force-policy' || record.name === 'xml-scale-two-force-hdw')
      assert.ok(wrappers[0].xml.bbox.w > 0 && wrappers[0].xml.bbox.h > 0,
        `${record.name}: Source HDW supplies nonzero dimensions`);
    assert.equal(wrappers[0].xml.outputScale, record.outputOptions.scale,
      `${record.name}: actual Source output option scale is recorded`);
    assert.ok(Number.isFinite(wrappers[0].xml.metrics.scale) && wrappers[0].xml.metrics.scale > 0,
      `${record.name}: actual Source wrapper metrics scale is recorded`);
    if (record.name === 'xml-authored-font-style-removed-times-fallback') {
      const annotation = find(record.mml, 'annotation-xml')[0];
      assert.equal(annotation.attributes.style, 'font-family: ProbeFamily',
        'Source MML retains the authored parent font style');
      assert.equal(wrappers[0].xml.styles['font-family'], 'Times',
        'Source removes the authored family before the XML wrapper font-style tier');
      assert.equal(wrappers[0].xml.metrics.family, null,
        'Source metrics family is unset, so the actual adaptor Times fallback applies');
    }
    if (record.name === 'xml-after-preceding-math') {
      const firstGlyph = record.svg.indexOf('data-c=');
      const foreign = record.svg.indexOf('<foreignObject');
      assert.ok(firstGlyph >= 0 && foreign > firstGlyph,
        'Source foreignObject follows preceding math content');
    }
    assert.ok(record.measurementParent?.startKind && record.measurementParent.parentKind,
      `${record.name}: actual math.start.node parent is recorded`);
  }
}

async function main() {
  const records = [];
  for (const entry of cases) {
    const metrics = {em: entry.metrics.em, ex: entry.metrics.ex,
      containerWidth: entry.metrics.container_width, display: entry.metrics.display};
    records.push({...entry, ...await runtime.convert(entry, metrics)});
  }
  validate(records);
  const {provenance, modules} = runtime.provenance('MathML', __filename);

  const {writeObservations, sourceCase, treeMml} = require('./mathml-observation-data.cjs');
  const styles = rules => Object.entries(rules).flatMap(([selector, declarations]) =>
    Object.entries(declarations).map(([property, value]) => ({selector, property,
      kind: typeof value === 'number' ? 'number' : 'text',
      text: typeof value === 'string' ? value : '', number: typeof value === 'number' ? value : 0})));
  const observations = {
    sourceXmlStyleEntries: styles(records[0].wrappers.find(w => w.kind === 'XML').xml.wrapperStyles),
    sourceAnnotationXmlStyleEntries: styles(records[0].wrappers.find(w => w.kind === 'annotation-xml').annotationXmlStyles),
    sourceXmlCases: records.map(r => {
      const wrappers = r.wrappers.filter(w => w.kind === 'XML');
      const payloads = treeMml(r.mml).payloads;
      assert.equal(wrappers.length, payloads.length);
      return {source: sourceCase(r), hdw: {auto: 'automatic', use: 'use', force: 'force', ignore: 'ignore'}[r.outputOptions.htmlHDW],
        scale: r.outputOptions.scale, can_measure_nodes: r.canMeasureNodes,
        start_kind: r.measurementParent.startKind, start_parent_kind: r.measurementParent.parentKind,
        payloads, observations: wrappers.map(({xml: x}) => ({...x.bbox,
          family: x.styles['font-family'], font_size: x.styles['font-size'],
          html: x.getHTML, wrapper_styles: JSON.stringify(x.wrapperStyles), source_html_hdw: x.htmlHDW,
          can_measure_nodes: x.canMeasureNodes, output_scale: x.outputScale,
          metrics_scale: x.metrics.scale, em: x.metrics.em, ex: x.metrics.ex,
          metrics_family: x.metrics.family || ''}))};
    })
  };
  const evidencePath = path.join(root, 'build/math-port/common-source/xml-mathml-scene.json');
  fs.mkdirSync(path.dirname(evidencePath), {recursive: true});
  fs.writeFileSync(evidencePath, JSON.stringify({provenance, modules, cases: records}, null, 2) + '\n');
  writeObservations('xml', observations);
  process.stdout.write(`Verified ${records.length} independent XML Source cases\n`);
}

main().catch(error => {process.stderr.write(error.stack + '\n'); process.exitCode = 1;});
