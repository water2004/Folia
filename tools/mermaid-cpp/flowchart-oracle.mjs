// Observe the released original Jison parser, FlowDB and layout-input adapter.
// No native parser or replacement DB is used to generate these expectations.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { JSDOM } from './oracle/node_modules/jsdom/lib/api.js';
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;
const { frontend } = await import('./frontend-oracle.mjs');
const { read, hex } = await import('./config-reference.mjs');
const { default: mermaid } = await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks = new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/', import.meta.url);
const map = JSON.parse(readFileSync(new URL(readdirSync(chunks).find(n => n.endsWith('.mjs.map') &&
  readFileSync(new URL(n, chunks), 'utf8').includes('/flowDb.ts')), chunks)));
const dbIndex = map.sources.findIndex(n => n.endsWith('/flowDb.ts'));
assert(dbIndex >= 0);
assert.equal(map.sourcesContent[dbIndex].replaceAll('\r\n', '\n'),
  read('packages/mermaid/src/diagrams/flowchart/flowDb.ts'));
const cases = [
  ['edge-ids', 'flowchart LR\nA first@--> B\nA first@--> B\nB named@-- next --> C'],
  ['edge-ids-group', 'flowchart LR\nA & B group@--> C & D'],
  ['interpolation', 'flowchart TD\nA-->B-->C\nlinkStyle 0 interpolate linear stroke:#123\nlinkStyle 1 interpolate basis'],
  ['interpolation-default', 'flowchart LR\nA-->B\nlinkStyle default interpolate linear\nB-->C\nlinkStyle default interpolate basis\nC-->D'],
  ['markdown', 'flowchart LR\nA["`Read **bold** and *italic*`"] -->|"`next **operation**`"| B["`Save ***both***\nthen finish`"]'],
  ['markdown-infix', 'flowchart LR\nA -- "`read **input**`" --> B'],
  ['basic', 'graph TD; A[Start] --> B{Ready?}; B -->|yes| C(End); B -->|no| A'],
  ['empty', 'flowchart TD\n'],
  ['no-direction', 'flowchart\nA-->B'],
  ['directions', 'graph RL\nA-->B'],
  ['bottom-up', 'graph BT\nA-->B'],
  ['shapes', 'flowchart LR\nA((Circle)) --> B([Stadium]) --> C[[Subroutine]] --> D[(Store)]\nE{{Hexagon}} --> F>Flag] --> G(((Double)))\nH[/Lean/] --> I[\\Other\\]\nJ[/Trap\\] --> K[\\Inverse/]'],
  ['quoted-unicode', 'flowchart TD\nA["开始 (input)"] -->|"完成"| B["结果 😀"]'],
  ['redeclare', 'graph TD\nA[First]-->B\nA(Last)\nA-->B\nB-->B'],
  ['grouped-chain', 'flowchart LR\nA & B --> C & D --> E'],
  ['strokes', 'flowchart TD\nA --- B\nB -.-> C\nC ==> D\nD ~~~ E\nE <---> F\nF o--o G\nG x--x H\nA ----> H'],
  ['infix-labels', 'graph TD\nA -- yes --> B\nB -. maybe .-> C\nC == done ==> D'],
  ['subgraph', 'flowchart TD\nsubgraph group[Processing]\ndirection LR\nA-->B\nend\nB-->C'],
  ['nested', 'flowchart LR\nsubgraph outer[Outer]\nsubgraph inner[Inner]\nA-->B\nend\nB-->C\nend\nC-->D'],
  ['group-edge', 'graph TD\nsubgraph g[Group]\nA-->B\nend\ng-->C'],
  ['implicit-group', 'graph TD\nsubgraph Some title\nA-->B\nend'],
  ['styles', 'flowchart TD\nA[Start]:::good-->B\nclassDef good fill:#aaf,color:#111\nclassDef default stroke:#333\nclass B good\nstyle A stroke-width:2px\nlinkStyle 0 stroke:#f00'],
  ['default-link-style', 'graph TD\nA-->B-->C\nlinkStyle default stroke:#123\nlinkStyle 1 stroke-width:3px'],
  ['metadata', '---\ntitle: Workflow\n---\nflowchart TD\naccTitle: Accessible flow\naccDescr: A simple process\nA-->B'],
  ['configured', '---\nconfig:\n  flowchart: {nodeSpacing: 30, rankSpacing: 70, padding: 12, curve: linear, inheritDir: true}\n---\nflowchart LR\nsubgraph g[Group]\nA-->B\nend', {theme:'dark'}],
  ['comments-crlf', 'graph TD\r\n%% process\r\nA[Begin]-->B[End]\r\n'],
  ['unfinished-node', 'flowchart TD\nA[Start'],
  ['unfinished-edge', 'flowchart TD\nA-->'],
  ['unfinished-subgraph', 'flowchart TD\nsubgraph g\nA-->B'],
  ['bad-style-index', 'graph TD\nA-->B\nlinkStyle 2 stroke:red'],
];
const rows = ['# Original Mermaid 12.2.0 Flowchart parser/DB/layout input; UTF-16 hex JSON.'];
let successes = 0;
for (const [name, source, options = {}] of cases) {
  const site = {theme:'default', ...options};
  let result = null;
  try {
    const prepared = frontend(site)(source);
    mermaid.initialize({...prepared.effective, startOnLoad:false});
    const diagram = await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned, {title:prepared.title});
    const db = diagram.db, data = db.getData();
    result = {
      direction: db.getDirection(), title: db.getDiagramTitle(),
      accTitle: db.getAccTitle(), accDescription: db.getAccDescription(),
      nodeSpacing: data.config.flowchart.nodeSpacing, rankSpacing: data.config.flowchart.rankSpacing,
      nodes: data.nodes.map(n => ({id:n.id,label:n.label,labelType:n.labelType,shape:n.shape,
        parent:n.parentId ?? '',dir:n.dir ?? '',group:n.isGroup,padding:n.padding,
        classes:n.cssClasses,styles:n.cssStyles ?? [],compiled:n.cssCompiledStyles})),
      edges: data.edges.map(e => ({id:e.id,start:e.start,end:e.end,label:e.label,labelType:e.labelType,
        type:e.type,stroke:e.thickness,minlen:e.minlen,arrowStart:e.arrowTypeStart,arrowEnd:e.arrowTypeEnd,
        curve:e.curve,styles:e.style,compiled:e.cssCompiledStyles})),
    };
    ++successes;
  } catch (error) {
    assert(name.startsWith('unfinished-') || name === 'bad-style-index', `${name}: ${error.stack}`);
  }
  rows.push([name, hex(source), hex(JSON.stringify(site)), hex(JSON.stringify(result))].join('\t'));
}
const output = new URL('../../tests/mermaid_cpp/fixtures/flowchart.tsv', import.meta.url);
const expected = rows.join('\n') + '\n';
if (process.argv.includes('--write')) writeFileSync(output, expected);
else assert.equal(readFileSync(output, 'utf8').replaceAll('\r\n','\n'), expected);
dom.window.close();
console.log(`Flowchart: ${successes} successful layout inputs and ${cases.length-successes} parse/DB failures checked.`);
