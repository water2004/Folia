// Execute the unchanged complete Dagre layout; boxes are explicit inputs, not font estimates.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync, realpathSync } from 'node:fs';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
const require = createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json', import.meta.url)));
const base = path.dirname(require.resolve('dagre-d3-es/package.json'));
const lock = JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json', import.meta.url)));
for (const file of [...lock.dagreRank.files,...lock.dagreLayout.files]) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(base,file.path))).digest('hex'), file.sha256, file.path);
}
const load = name => import(pathToFileURL(path.join(base,'src',name)));
const {Graph} = await load('graphlib/index.js');
// Load unchanged private layout helpers in their original module scope.
const source = readFileSync(path.join(base,'src/dagre/layout.js'),'utf8');
const dependency = createRequire(path.join(base,'package.json'));
const helpers = source.replace(/from '([^']+)'/g, (_,name) =>
  `from '${pathToFileURL(name.startsWith('.') ? path.resolve(base,'src/dagre',name) : dependency.resolve(name)).href}'`) +
  '\nexport {buildLayoutGraph,runLayout};';
const layout = await import('data:text/javascript;base64,'+Buffer.from(helpers).toString('base64'));
const hex = text => Array.from(text, c => c).length === 0 ? '-' :
  Array.from({length:text.length},(_,i)=>text.charCodeAt(i).toString(16).padStart(4,'0')).join('');
const decode = text => text === '-' ? '' : text.match(/.{4}/g).map(x=>String.fromCharCode(parseInt(x,16))).join('');
const inputs = readFileSync(new URL('../../tests/mermaid_cpp/fixtures/flowchart.tsv',import.meta.url),'utf8')
  .trim().split(/\r?\n/).filter(s=>!s.startsWith('#')).map(s=>s.split('\t'));
const rows = ['# Dagre 7.0.14 full layout; source, site, node/edge boxes and result as UTF-16 hex JSON.'];
for (const [name,source,site,dataHex] of inputs) {
  const data = JSON.parse(decode(dataHex));
  if (!data || name === 'group-edge') continue; // Mermaid cluster-endpoint adaptation is a later stage.
  // Supplied measurement inputs: ranking depends on nonzero label boxes, not font estimates.
  const sizes = data.edges.map(e=>e.label ? {width:48,height:18} : {width:0,height:0});
  const nodeSizes=data.nodes.map((n,i)=>({width:n.isGroup?0:60+i*11,height:n.isGroup?0:30+i*3}));
  const input = new Graph({multigraph:true,compound:true}).setGraph({rankdir:data.direction,ranksep:data.rankSpacing,nodesep:data.nodeSpacing});
  data.nodes.forEach((n,i)=>input.setNode(n.id,nodeSizes[i]));
  for (const n of data.nodes) if (n.parent) input.setParent(n.id,n.parent);
  data.edges.forEach((e,i)=>input.setEdge(e.start,e.end,{minlen:e.minlen,weight:1,...sizes[i],labelpos:'c',original:i},e.id));
  const g = layout.buildLayoutGraph(input);
  // original is observation metadata, never read by the original algorithms.
  data.edges.forEach((e,i)=>g.edge(e.start,e.end,e.id).original=i);
  let ordered;
  layout.runLayout(g,(name,run)=>{ const value=run(); if(name.trim()==='order') ordered=data.nodes.map(n=>({rank:g.node(n.id).rank??null,order:g.node(n.id).order??null})); return value; });
  const result={width:g.graph().width,height:g.graph().height,
    nodes:data.nodes.map((n,i)=>({id:n.id,...ordered[i],x:g.node(n.id).x,y:g.node(n.id).y,width:g.node(n.id).width,height:g.node(n.id).height})),
    edges:data.edges.map(e=>{const a=g.edge(e.start,e.end,e.id);return {points:a.points,x:a.x??null,y:a.y??null};})};
  rows.push([name,source,site,hex(JSON.stringify({nodes:nodeSizes,edges:sizes})),hex(JSON.stringify(result))].join('\t'));
}
const output = new URL('../../tests/mermaid_cpp/fixtures/dagre-layout.tsv',import.meta.url);
const expected = rows.join('\n')+'\n';
if (process.argv.includes('--write')) writeFileSync(output,expected);
else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`Dagre layout: ${rows.length-1} original pipeline observations checked.`);
