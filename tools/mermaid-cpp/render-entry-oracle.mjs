// Original frontend -> registered family DB/config/styles. Widths and SVG
// algorithms remain covered by the existing family DOM and real-font oracles.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import vm from 'node:vm';
import { JSDOM } from './oracle/node_modules/jsdom/lib/api.js';
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;
const { frontend } = await import('./frontend-oracle.mjs');
const { src, strip, extract, hex } = await import('./config-reference.mjs');
const { default: mermaid } = await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks = new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/', import.meta.url);
const { diagram: packet } = await import(new URL('diagram-I5XIIV2D.mjs', chunks));
const { diagram: pie } = await import(new URL(readdirSync(chunks).find(n => /^pieDiagram-.*\.mjs$/.test(n)), chunks));
const decodeEntities = vm.runInNewContext(strip(extract(src('utils.ts'),
  'export const decodeEntities =', '\nexport const isString')) + '\ndecodeEntities');
const cases = [
  ['packet-default', 'packet\n0-7: "Header"'],
  ['packet-dark', 'packet\n0-7: "Header"', {theme:'dark'}],
  ['packet-config', '---\ntitle: Header layout\nconfig: {packet: {bitsPerRow: 8, bitWidth: 20, showBits: false, useMaxWidth: false, bitOrder: descending}}\n---\npacket\n+12: "Data"'],
  ['packet-style', 'packet\n0-7: "Data"', {themeVariables:{packet:{labelColor:'rgb(20, 30, 40)',blockFillColor:'#abcdef',titleFontSize:'20px',blockStrokeWidth:'2'}}}],
  ['packet-title', '---\ntitle: Front title\n---\npacket\ntitle Body & more\naccTitle: Accessible\naccDescr: Details\n+8: "#quot;Data#quot;"'],
  ['pie-default', 'pie\n"Tea":3\n"Coffee":2'],
  ['pie-dark', 'pie\n"Tea":3', {theme:'dark'}],
  ['pie-config', '%%{init:{"pie":{"legendPosition":"bottom","donutHole":0.5,"textPosition":0.6,"highlightSlice":"Tea","useMaxWidth":false}}}%%\npie showData\n"Tea":3\n"Tea":9\n"Coffee":2'],
  ['pie-front-title', '---\ntitle: Drinks\n---\npie\n"Tea":3'],
  ['pie-body-title', '---\ntitle: Front\n---\npie\ntitle Body\n"Tea":3'],
  ['pie-theme', 'pie\n"Tea":3', {theme:'dark',themeVariables:{fontFamily:'Arial',pie1:'rgb(1, 2, 3)',pieLegendTextSize:'21px',pieTitleTextSize:'30px',pieOuterStrokeWidth:'4px'}}],
  ['pie-font-list', 'pie\n"Tea":3', {fontFamily:'"Trebuchet MS", Arial, sans-serif'}],
  ['pie-entities', 'pie showData\ntitle A & B\n"#quot;Tea#quot; #9733;":3'],
  ['pie-incomplete', 'pie\n"Tea":'],
  ['packet-gap', 'packet\n3-8: "Gap"'],
];
const rows = [];
for (const [name, source, options = {}] of cases) {
  const site = {theme:'default', ...options};
  let result;
  try {
    const prepared = frontend(site)(source);
    // Feed the original family the already-resolved effective configuration;
    // getDiagramFromText itself does not apply extracted directives.
    mermaid.initialize({...prepared.effective, startOnLoad:false});
    const diagram = await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned, {title:prepared.title});
    const db = diagram.db;
    const config = db.getConfig();
    const theme = prepared.effective.themeVariables;
    const family = prepared.type === 'packet' ? packet : pie;
    const style = family.styles(theme).replace(/\s+/g, '');
    const display = text => {
      const svg = document.createElementNS('http://www.w3.org/2000/svg','text');
      svg.textContent = text;
      return new JSDOM('<svg xmlns="http://www.w3.org/2000/svg">'+decodeEntities(svg.outerHTML)+'</svg>',
        {contentType:'image/svg+xml'}).window.document.documentElement.textContent;
    };
    result = {type:prepared.type,title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),
      displayTitle:display(db.getDiagramTitle()), style,
      config:prepared.type === 'packet' ? config : {...config,...prepared.effective.pie},
      theme, labels:prepared.type === 'packet' ? db.getPacket().flat().map(b=>display(b.label)) : [...db.getSections().keys()].map(display),
      showData:prepared.type === 'pie' ? db.getShowData() : false};
  } catch (error) {
    if (!['pie-incomplete','packet-gap'].includes(name)) throw error;
    result = null;
  }
  rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));
}
const output = new URL('../../tests/mermaid_cpp/fixtures/render-entry.tsv', import.meta.url);
const fixture = '# Original Packet/Pie entry: name/source/site/observation (UTF-16 hex)\n'+rows.join('\n')+'\n';
if (process.argv.includes('--write')) writeFileSync(output,fixture);
else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),fixture);
console.log(`Verified ${rows.length} original Packet/Pie entry cases`);
