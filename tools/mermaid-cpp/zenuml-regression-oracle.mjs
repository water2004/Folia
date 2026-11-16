// @zenuml/core 3.49.2 supplies the parser and CSS-defined vertical layout.
// No DOM, estimated font widths, or native renderer output participates here.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const source = resolve(process.env.FOLIA_ZENUML_SOURCE ?? 'build/mermaid-cpp/zenuml-source');
assert.equal(createHash('sha512').update(readFileSync(resolve(source, 'core-3.49.2.tgz'))).digest('base64'),
  'qeQMapuE0XETML2mRw2MIIrMCjcZBhRDSwW10//q8XQ+VyExzkAn39VCBwxIbbPdTBt6gJnaBBixF9A4hsIxcA==');
const bundle = readFileSync(resolve(source, 'package/dist/zenuml.esm.mjs'), 'utf8');
assert(bundle.includes('this.participantModels = cr(t), this.ownableMessages = qa(t)'));
assert(bundle.includes('class Z8 {'));
const instrumented = resolve(source, 'package/dist/folia-regression-oracle.mjs');
writeFileSync(instrumented, bundle + `
export function regressionModel(source) {
  const root = R8(source);
  if (!root || root.parser._syntaxErrors) throw new Error('Invalid official ZenUML input');
  const vertical = new Z8(root);
  return {
    title: root.title?.()?.content?.() ?? '',
    participants: cr(root), messages: qa(root),
    vertical: vertical.entries().map(([, coordinate]) => coordinate),
    height: vertical.getTotalHeight()
  };
}
`);
// The package's unrelated UI preference initialization reads these browser
// globals during import; parser/layout execution does not read either value.
globalThis.location = { href: 'file://folia-zenuml-regression', search: '' };
globalThis.localStorage = { getItem: () => null };
const { regressionModel } = await import(pathToFileURL(instrumented).href);
const corpus = JSON.parse(readFileSync('tests/mermaid_regression/cases/zenuml.json', 'utf8'));
const hex = value => Array.from({ length: value.length }, (_, index) =>
  value.charCodeAt(index).toString(16).padStart(4, '0')).join('');
const records = corpus.cases.map(test => {
  let model;
  try { model = regressionModel(test.source.replace(/^\s*zenuml/, '')); }
  catch (error) { throw new Error(`${test.id}: ${error.message}`, { cause: error }); }
  return [test.id, hex(test.source), hex(JSON.stringify(model))].join('\t');
});
const fixture = records.join('\n') + '\n';
const target = 'tests/mermaid_cpp/fixtures/zenuml-regression.tsv';
if (process.argv.includes('--write')) writeFileSync(target, fixture);
else assert.equal(readFileSync(target, 'utf8').replaceAll('\r\n', '\n'), fixture);
console.log(`ZenUML: ${records.length} official parser and vertical-layout regressions checked.`);
