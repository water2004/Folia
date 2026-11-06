// Actual official renderer with explicit measurement inputs. These fixtures
// validate layout arithmetic, NOT font measurement or browser font fidelity.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { JSDOM } from './oracle/node_modules/jsdom/lib/api.js';
const sourceRoot = new URL('../../.reference/mermaid-js/', import.meta.url);
const lock = JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json', import.meta.url)));
for (const file of lock.files) assert.equal(createHash('sha256')
  .update(readFileSync(new URL(file.path, sourceRoot))).digest('hex'), file.sha256);
const version = JSON.parse(readFileSync(new URL('./oracle/node_modules/mermaid/package.json', import.meta.url))).version;
assert.equal(version, '12.2.0');
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;
const { default: mermaid } = await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const hex = text => Array.from({length: text.length}, (_,i) =>
  text.charCodeAt(i).toString(16).padStart(4,'0')).join('') || '-';
const translation = element => (element.getAttribute('transform') ?? 'translate(0,0)')
  .slice(10,-1).split(/[, ]+/).map(Number);
const cases = [];
for (const position of ['right','left','top','bottom','center']) {
  for (const [name,source,widths,title] of [
    ['empty','pie',[],0],
    ['single','pie\n"Only":1',[80],0],
    ['mixed','pie\n"A":10\n"B":20\n"C":70',[12,120,35],44],
    ['long-title','pie\ntitle Long title\n"A":1\n"B":1',[60,40],1300],
    ['show-data','pie showData\n"A":0.1\n"B":0.2',[95,115],0],
    ['filtered','pie\n"tiny":0.99\n"boundary":1\n"rest":98.01',[50,80,45],0],
    ['zero','pie\n"A":0\n"B":0',[12.25,41.75],700],
    ['duplicates','pie showData\n"same":1\n"same":9\n"other":3',[101,83],20],
  ]) cases.push({name:`${position}-${name}`,position,source,widths,title});
}
const lines = ['# Official Pie renderer with injected width inputs; NOT a font measurement oracle.',
  '# name source-utf16 position measured-widths title-width success viewbox-x width height pie-x pie-y legends slices'];
for (const item of cases) {
  mermaid.initialize({startOnLoad:false,securityLevel:'strict',pie:{legendPosition:item.position}});
  const diagram = await mermaid.mermaidAPI.getDiagramFromText('pie');
  diagram.db.clear();
  await diagram.parser.parse(item.source);
  document.body.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" id="probe"></svg>';
  let reads = 0;
  dom.window.Element.prototype.getBoundingClientRect = function() {
    ++reads;
    if (this.classList.contains('pieTitleText')) return {width:item.title};
    const index = [...document.querySelectorAll('.legend text')].indexOf(this);
    assert(index >= 0 && index < item.widths.length, 'Unexpected renderer measurement');
    return {width:item.widths[index]};
  };
  await diagram.render('probe','12.2.0');
  assert.equal(reads,item.widths.length+1);
  const svg = document.querySelector('svg');
  const [vx,,width,height] = svg.getAttribute('viewBox').split(' ').map(Number);
  const [px,py] = translation(svg.querySelector('g > g'));
  const legends = [...svg.querySelectorAll('.legend')].map(g =>
    [...translation(g),hex(g.querySelector('text').textContent)].join(','));
  const sections = [...diagram.db.getSections().keys()];
  const slices = [...svg.querySelectorAll('.slice')].map(node =>
    [sections.indexOf(node.__data__.data.label),node.textContent].join(','));
  const valid = [vx,width,height,px,py].every(Number.isFinite);
  lines.push([item.name,hex(item.source),item.position,item.widths.join(',')||'-',item.title,
    valid?1:0,vx,width,height,px,py,legends.join(';')||'-',slices.join(';')||'-'].join('\t'));
}
const output = lines.join('\n')+'\n';
const file = new URL('../../tests/mermaid_cpp/fixtures/pie-layout.tsv',import.meta.url);
if(process.argv[2]==='--write') writeFileSync(file,output);
else {
  assert.equal(process.argv[2],'--check');
  assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),output);
}
dom.window.close();
console.log(`Verified ${cases.length} official Pie layouts with explicit measurement inputs.`);
