// Execute the original Dagre rank pipeline, stopping before edge normalization/order.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync, realpathSync } from 'node:fs';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
const require = createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json', import.meta.url)));
const base = path.dirname(require.resolve('dagre-d3-es/package.json'));
const lock = JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json', import.meta.url)));
for (const file of lock.dagreRank.files) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(base,file.path))).digest('hex'), file.sha256, file.path);
}
const load = name => import(pathToFileURL(path.join(base,'src',name)));
const {Graph} = await load('graphlib/index.js');
const acyclic = await load('dagre/acyclic.js');
const nesting = await load('dagre/nesting-graph.js');
const util = await load('dagre/util.js');
const {rank} = await load('dagre/rank/index.js');
// Load unchanged private layout helpers in their original module scope.
const source = readFileSync(path.join(base,'src/dagre/layout.js'),'utf8');
const dependency = createRequire(path.join(base,'package.json'));
const helpers = source.replace(/from '([^']+)'/g, (_,name) =>
  `from '${pathToFileURL(name.startsWith('.') ? path.resolve(base,'src/dagre',name) : dependency.resolve(name)).href}'`) +
  '\nexport {buildLayoutGraph,makeSpaceForEdgeLabels,removeSelfEdges,injectEdgeLabelProxies,assignRankMinMax,removeEdgeLabelProxies};';
const layout = await import('data:text/javascript;base64,'+Buffer.from(helpers).toString('base64'));
const hex = text => Array.from(text, c => c).length === 0 ? '-' :
  Array.from({length:text.length},(_,i)=>text.charCodeAt(i).toString(16).padStart(4,'0')).join('');
const decode = text => text === '-' ? '' : text.match(/.{4}/g).map(x=>String.fromCharCode(parseInt(x,16))).join('');
const inputs = readFileSync(new URL('../../tests/mermaid_cpp/fixtures/flowchart.tsv',import.meta.url),'utf8')
  .trim().split(/\r?\n/).filter(s=>!s.startsWith('#')).map(s=>s.split('\t'));
const rows = ['# Dagre 7.0.14 rank pipeline; source, site, measured edge sizes and result as UTF-16 hex JSON.'];
for (const [name,source,site,dataHex] of inputs) {
  const data = JSON.parse(decode(dataHex));
  if (!data || name === 'group-edge') continue; // Mermaid cluster-endpoint adaptation is a later stage.
  // Supplied measurement inputs: ranking depends on nonzero label boxes, not font estimates.
  const sizes = data.edges.map(e=>e.label ? {width:48,height:18} : {width:0,height:0});
  const input = new Graph({multigraph:true,compound:true}).setGraph({rankdir:data.direction,ranksep:data.rankSpacing});
  for (const n of data.nodes) input.setNode(n.id,{});
  for (const n of data.nodes) if (n.parent) input.setParent(n.id,n.parent);
  data.edges.forEach((e,i)=>input.setEdge(e.start,e.end,{minlen:e.minlen,weight:1,...sizes[i],labelpos:'c',original:i},e.id));
  const g = layout.buildLayoutGraph(input);
  // original is observation metadata, never read by the original algorithms.
  data.edges.forEach((e,i)=>g.edge(e.start,e.end,e.id).original=i);
  layout.makeSpaceForEdgeLabels(g); layout.removeSelfEdges(g); acyclic.run(g);
  nesting.run(g); rank(util.asNonCompoundGraph(g));
  layout.injectEdgeLabelProxies(g); util.removeEmptyRanks(g); nesting.cleanup(g);
  util.normalizeRanks(g); layout.assignRankMinMax(g); layout.removeEdgeLabelProxies(g);
  const result = {
    rankSpacing:g.graph().ranksep,
    nodes:data.nodes.map(n=>({id:n.id,rank:g.node(n.id).rank ?? null,
      minRank:g.node(n.id).minRank ?? null,maxRank:g.node(n.id).maxRank ?? null})),
    edges:data.edges.map((e,i)=> {
      const edge = g.edges().find(e=>g.edge(e).original===i);
      const label = edge ? g.edge(edge) : null;
      return {reversed:!!label?.reversed,self:e.start===e.end,labelRank:label?.labelRank ?? null};
    }),
  };
  rows.push([name,source,site,hex(JSON.stringify(sizes)),hex(JSON.stringify(result))].join('\t'));
}
const output = new URL('../../tests/mermaid_cpp/fixtures/dagre-rank.tsv',import.meta.url);
const expected = rows.join('\n')+'\n';
if (process.argv.includes('--write')) writeFileSync(output,expected);
else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`Dagre ranks: ${rows.length-1} original pipeline observations checked.`);
