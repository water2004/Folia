// Real upstream MathJax 4.1.3, with Folia's committed NewCM profile. The only
// per-host change is the Mermaid fragment's non-wrapping display policy.
import assert from 'node:assert/strict';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {join, resolve} from 'node:path';
import sharp from 'sharp';
import {directory, fileHash, loadMathJaxProfile} from './profile.mjs';

const destination = resolve(process.argv[2] ?? 'build/mermaid-math/mathjax-newcm');
mkdirSync(destination, {recursive: true});
const corpusPath = join(directory, 'cases.json');
const corpus = JSON.parse(readFileSync(corpusPath, 'utf8'));
assert.equal(corpus.schema, 1);
const profile = loadMathJaxProfile();
const {document, adaptor, output} = profile;
let rootMetrics = null;
const createRoot = output.createRoot;
output.createRoot = function (wrapper) {
  const result = createRoot.call(this, wrapper);
  // Read the actual upstream box after its normal calculation. SVG's printed
  // viewBox rounds to 0.1 units, so it cannot recover full-precision CSS metrics.
  rootMetrics ??= {box: {...wrapper.getOuterBBox()}, metrics: {...wrapper.metrics}};
  return result;
};
const records = [];
for (const item of corpus.cases) {
  try {
    rootMetrics = null;
    const container = document.convert(item.tex, {display: true, em: item.em,
      ex: item.em * 0.5, containerWidth: item.width});
    const raw = adaptor.serializeXML(container);
    const children = adaptor.childNodes(container);
    assert.equal(children.length, 1, 'Display fragment must have a single SVG root');
    const svg = children[0];
    assert.equal(adaptor.kind(svg), 'svg');
    const view = adaptor.getAttribute(svg, 'viewBox').trim().split(/\s+/).map(Number);
    assert.equal(view.length, 4);
    assert(view.every(Number.isFinite));
    const scale = item.em * 0.5 / (output.font.params.x_height * 1000);
    assert(rootMetrics, 'The actual MathJax root box was not observed');
    const {w, h, d, pwidth} = rootMetrics.box;
    assert(!pwidth, 'Percentage-width roots need an explicit viewport capture');
    const pxPerEm = item.em * 0.5 / output.font.params.x_height;
    const width = Math.max(w, item.em / 1000) * pxPerEm;
    const height = Math.max(h + d, item.em / 1000) * pxPerEm;
    const baseline = h * pxPerEm;
    adaptor.setAttribute(svg, 'width', `${width}px`);
    adaptor.setAttribute(svg, 'height', `${height}px`);
    const standalone = adaptor.serializeXML(svg);
    const svgPath = join(destination, item.name + '.svg');
    writeFileSync(svgPath, standalone);
    let png = null;
    // Empty TeX has a sub-pixel 16-unit SVG box. It is valid output, but a
    // rasterizer rounds it to zero pixels; keep its SVG/metrics evidence.
    if (width >= 1 && height >= 1) {
      const path = join(destination, item.name + '.png');
      const raster = await sharp(Buffer.from(standalone), {density: 144}).png().toFile(path);
      png = {sha256: fileHash(path), width: raster.width, height: raster.height};
    }
    records.push({...item, svg: standalone, raw, width, height, baseline, viewBox: view,
      scale, rootMetrics, svgSha256: fileHash(svgPath), png, error: null});
  } catch (error) {
    records.push({...item, svg: null, error: {name: error.name, message: error.message}});
  }
}
writeFileSync(join(destination, 'report.json'), JSON.stringify({schema: 1,
  reference: 'Official MathJax 4.1.3 / NewCM, committed Folia profile, display overflow fragment',
  corpusSha256: fileHash(corpusPath), lock: profile.provenance,
  node: process.version, rasterizer: sharp.versions, dynamicModules: profile.dynamicModules,
  records}, null, 2) + '\n');
console.log(JSON.stringify({destination, records: records.length,
  errors: records.filter(item => item.error).map(({name, error}) => ({name, error}))}));
