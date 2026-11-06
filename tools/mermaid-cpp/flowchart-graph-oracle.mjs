// Original Mermaid graph preparation, before DOM measurement or Dagre layout.
import assert from 'node:assert/strict';
import {readFileSync, writeFileSync, realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import {pathToFileURL} from 'node:url';
import vm from 'node:vm';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window=dom.window;globalThis.document=dom.window.document;
const {src, withoutImports, hex}=await import('./config-reference.mjs');
const {frontend}=await import('./frontend-oracle.mjs');
const require = createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json', import.meta.url)));
const graphlib = await import(pathToFileURL(require.resolve('dagre-d3-es/src/graphlib/index.js')));
const log = {debug(){}, info(){}, trace(){}, warn(){}, error(){}};
const clusters = vm.runInNewContext(withoutImports(src('rendering-util/layout-algorithms/dagre/mermaid-graphlib.js'))
  .replace(/^export /gm,'')+'\n({clear,adjustClustersAndEdges})', {graphlib,log});
const prepare = vm.runInNewContext(withoutImports(src('rendering-util/layout-algorithms/dagre/index.js').split('export const render =')[0])
  .replace(/^export /gm,'')+'\nprepareLayoutForDagre',
  {graphlib,log,structuredClone,...clusters,createCommonLayoutRenderer(){}});
const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const cases = [
  ['plain','flowchart TD\nA-->B'],
  ['independent','flowchart TD\nsubgraph g[Group]\ndirection LR\nA-->B\nend\ng-->C'],
  ['incoming','flowchart LR\nC-->g\nsubgraph g[Group]\ndirection BT\nA-->B\nend'],
  ['external-child','flowchart TD\nsubgraph g[Group]\ndirection LR\nA-->B\nend\nB-->C\ng-->D'],
  ['incoming-external','flowchart LR\nsubgraph g[Group]\nA-->B\nend\nC-->A\nD-->g'],
  ['siblings','flowchart TD\nsubgraph g[One]\nA-->B\nend\nsubgraph h[Two]\ndirection RL\nC-->D\nend\ng-->h'],
  ['nested-independent','flowchart TD\nsubgraph g[Outer]\ndirection LR\nsubgraph h[Inner]\ndirection BT\nA-->B\nend\nh-->C\nend\ng-->D'],
  ['nested-external','flowchart LR\nsubgraph g[Outer]\nsubgraph h[Inner]\nA-->B\nend\nB-->C\nend\nC-->D\ng-->E'],
  ['anchor-sibling','flowchart TD\nsubgraph g[Outer]\nsubgraph h[Inner]\nA-->B\nend\nC\nend\nC-->D\ng-->E'],
  ['parallel','flowchart TD\nsubgraph g[Group]\nA-->B\nend\ng-->C\ng-->|again|C\nB-->D'],
  ['numeric','flowchart TD\nsubgraph g[Group]\n10-->2\nend\n2-->C\ng-->D'],
  ['loop','flowchart TD\nA-->|retry|A\nA-->B'],
  ['loop-in-group','flowchart LR\nsubgraph g[Group]\ndirection TB\nA-->|retry|A\nA-->B\nend\ng-->C'],
  ['group-loop','flowchart TD\nsubgraph g[Group]\nA-->B\nend\ng-->|retry|g'],
  ['inherit-spacing','flowchart LR\nsubgraph g[Group]\nA-->B\nend',
    {flowchart:{inheritDir:true,nodeSpacing:30,rankSpacing:70}}],
];
function observe(g) {
  return {direction:g.graph().rankdir,nodeSpacing:g.graph().nodesep,rankSpacing:g.graph().ranksep,
    nodes:g.nodes().map(id=>{const n=g.node(id);return {id,parent:g.parent(id)??'',
      label:(n.clusterData??n).label??'',shape:(n.clusterData??n).shape??'',
      graph:n.clusterNode?observe(n.graph):null};}),
    edges:g.edges().map(e=>{const a=g.edge(e);return {name:e.name,start:e.v,end:e.w,id:a.id,label:a.label,
      fromCluster:a.fromCluster??'',toCluster:a.toCluster??'',
      loopId:a.selfLoop?.id??'',loopOrder:a.selfLoop?.order??null};})};
}
const rows=['# Original prepareLayoutForDagre/mermaid-graphlib; source, site and graph tree as UTF-16 hex JSON.'];
for(const [name,source,options={}] of cases) {
  const site={theme:'default',...options};const p=frontend(site)(source);
  mermaid.initialize({...p.effective,startOnLoad:false});
  const d=await mermaid.mermaidAPI.getDiagramFromText(p.code.cleaned,{title:p.title});
  const data=d.db.getData();data.direction=d.db.getDirection();
  clusters.clear();const {graph}=prepare(data);
  rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(observe(graph)))].join('\t'));
}
dom.window.close();
const output=new URL('../../tests/mermaid_cpp/fixtures/flowchart-graph.tsv',import.meta.url);
const expected=rows.join('\n')+'\n';
if(process.argv.includes('--write')) writeFileSync(output,expected);
else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`Flowchart graph preparation: ${cases.length} original cluster/self-loop observations checked.`);
