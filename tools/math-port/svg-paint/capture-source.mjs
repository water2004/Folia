// Capture only the pinned official MathJax SVG, layout boxes and stylesheet.
// This process never loads native output or decides whether a test passed.
import assert from 'node:assert/strict';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {parseArgs} from 'node:util';
import {fileHash, loadMathJaxProfile} from '../../mermaid-cpp/math-oracle/profile.mjs';
import {sourceStyles} from './source-paint.mjs';

const directory = dirname(fileURLToPath(import.meta.url));
const {values} = parseArgs({options: {
  input: {type: 'string', default: join(directory, 'cases.json')},
  output: {type: 'string', default: 'build/math-svg-paint/source'},
}});
const corpus = JSON.parse(readFileSync(values.input, 'utf8'));
assert.equal(corpus.schema, 1);
assert(corpus.cases.length && corpus.modes.length);
const profile = loadMathJaxProfile(), {adaptor, document, output} = profile;
const destination = resolve(values.output);
mkdirSync(destination, {recursive: true});
let rootBox;
const createRoot = output.createRoot;
output.createRoot = function (wrapper) {
  const result = createRoot.call(this, wrapper);
  rootBox ??= {...wrapper.getOuterBBox()};
  return result;
};
const records = [];
for (const item of corpus.cases) for (const mode of corpus.modes) {
  assert.match(item.name, /^[A-Za-z0-9_-]+$/);
  assert(['inline', 'display', 'fragment'].includes(mode));
  const name = item.name + '--' + mode, record = {name, case: item.name, mode};
  try {
    rootBox = null;
    output.options.displayOverflow = mode === 'fragment' ? 'overflow' : 'linebreak';
    output.options.linebreaks = {...output.options.linebreaks, inline: mode !== 'fragment'};
    const container = document.convert(item.tex, {display: mode !== 'inline', em: item.em ?? 16,
      ex: (item.em ?? 16) * .5, containerWidth: item.width ?? 1280});
    const roots = adaptor.childNodes(container).filter(node => adaptor.kind(node) === 'svg');
    Object.assign(record, {original: adaptor.serializeXML(container),
      roots: roots.map(root => adaptor.serializeXML(root)), rootBox,
      xHeight: output.font.params.x_height, styles: sourceStyles(profile)});
    writeFileSync(join(destination, name + '.xml'), record.original);
  } catch (error) {
    record.error = {name: error.name, message: error.message};
  }
  records.push(record);
}
const report = {schema: 1, corpusSha256: fileHash(values.input), sourceLock: profile.provenance,
  reference: 'Pinned MathJax 4.1.3 / NewCM with actual host CSS', records};
writeFileSync(join(destination, 'report.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({destination, records: records.length,
  captureErrors: records.filter(row => row.error).map(row => row.name)}));
process.exitCode = records.some(row => row.error) ? 1 : 0;
