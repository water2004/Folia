// Official Mermaid v13 is deliberately isolated from Folia's v12 renderer.
// Its MathJax plugin is a real, unmodified reference for integration semantics.
import assert from 'node:assert/strict';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {join, resolve} from 'node:path';
import mermaid from 'mermaid';
import {math} from '@mermaid-js/plugin-math';
import {inlineSpans, layoutLabel, tableMeasurer} from '@mermaid-js/core/text';
import {streamWriter, writeFragment} from '@mermaid-js/core/svg';
import sharp from 'sharp';
import {directory, fileHash, verifyLock} from './profile.mjs';

const destination = resolve(process.argv[2] ?? 'build/mermaid-math/official-v13');
mkdirSync(destination, {recursive: true});
const lock = verifyLock();
const corpusPath = join(directory, 'diagrams.json');
const corpus = JSON.parse(readFileSync(corpusPath, 'utf8'));
assert.equal(corpus.schema, 1);
await mermaid.registerExternalDiagrams([math]);
await mermaid.preload({math: true});
const records = [];
for (const item of corpus.diagrams) {
  mermaid.initialize({fontFamily: 'Arial', fontSize: 16, theme: 'default',
    output: {cssVariables: false}, ...item.config});
  const result = await mermaid.render('math_' + item.name, item.source);
  assert(result.svg.startsWith('<svg '));
  const svgPath = join(destination, item.name + '.svg');
  const pngPath = join(destination, item.name + '.png');
  writeFileSync(svgPath, result.svg);
  const raster = await sharp(Buffer.from(result.svg), {density: 144}).png().toFile(pngPath);
  records.push({...item, width: result.width, height: result.height,
    warnings: result.warnings, svgSha256: fileHash(svgPath), pngSha256: fileHash(pngPath),
    raster: {width: raster.width, height: raster.height}});
}

// Exercise the same public provider and label compositor used by the official
// diagrams, retaining exact line metrics rather than reverse-engineering pixels.
const labels = [];
for (const source of ['before $$x^2$$ after', '$$x^2$$ and $$\\frac{1}{2}$$',
  '**before** $$x^2$$ *after*', 'first $$x^2$$<br/>second $$\\frac{1}{2}$$']) {
  const warnings = [];
  const font = {family: 'Arial', size: 16, weight: 400};
  const inline = inlineSpans(math.inline, warning => warnings.push(warning));
  const label = layoutLabel(source, font, 200, tableMeasurer, {inline, markdown: true});
  labels.push({source, font, label, warnings});
}

// Save standalone boxes from the actual official plugin, using its writer.
// They use TeX font. NewCM parity is checked by capture-mathjax.mjs separately.
const fragmentRecords = [];
for (const tex of ['x^2', '\\frac{1}{2}', '\\sum_{i=0}^{n}i',
  '\\begin{pmatrix}a&b\\\\c&d\\end{pmatrix}']) {
  const warnings = [], font = {family: 'Arial', size: 16, weight: 400};
  const box = math.inline[0].box(tex, {font, measurer: tableMeasurer,
    metrics: tableMeasurer.metrics(font), ex: 8, warn: warning => warnings.push(warning), scratch: {}});
  assert(box, JSON.stringify(warnings));
  const writer = streamWriter();
  writer.open('svg', {xmlns: 'http://www.w3.org/2000/svg', width: box.width,
    height: box.height, viewBox: `0 0 ${box.width} ${box.height}`});
  writeFragment(writer, box.node);
  writer.close();
  const svg = writer.finish();
  const name = `plugin-fragment-${fragmentRecords.length}`;
  writeFileSync(join(destination, name + '.svg'), svg);
  await sharp(Buffer.from(svg), {density: 144}).png().toFile(join(destination, name + '.png'));
  fragmentRecords.push({name, tex, width: box.width, height: box.height, baseline: box.baseline});
}
const report = {schema: 1, reference: 'Official Mermaid 13.0.0-alpha.3 + plugin-math 13.0.0-alpha.3',
  purpose: 'Math integration semantics and SVG/raster evidence; v13 graph coordinates are not a v12 layout oracle',
  corpusSha256: fileHash(corpusPath), lock, node: process.version, rasterizer: sharp.versions,
  records, labels, fragments: fragmentRecords};
writeFileSync(join(destination, 'report.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({destination, diagrams: records.length, fragments: fragmentRecords.length,
  warnings: records.filter(record => record.warnings.length).map(({name, warnings}) => ({name, warnings}))}));
