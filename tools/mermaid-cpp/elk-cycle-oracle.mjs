// Execute unchanged ELK 0.9.3 layered graph and DepthFirstCycleBreaker.
// This establishes feedback-edge selection, not a finished ELK layout.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync, writeFileSync, realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';
import vm from 'node:vm';
const require = createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json', import.meta.url)));
const base = path.dirname(require.resolve('elkjs/package.json'));
const lock = JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json', import.meta.url)));
for (const file of lock.elkCycle.files)
  assert.equal(createHash('sha256').update(readFileSync(path.join(base,file.path))).digest('hex'), file.sha256, file.path);
const context = vm.createContext({module:{exports:{}}, exports:{}, setTimeout, clearTimeout});
context.global = context;
vm.runInContext(readFileSync(path.join(base,'lib/elk-worker.js'),'utf8'),context);
// The original module is unmodified; expose only a harness over its graph API.
const original = vm.runInContext(`(nodeCount, owners, endpoints) => {
  const graph = new LGraph();
  const nodes = Array.from({length:nodeCount}, () => {
    const n = new LNode(graph); $add_3(graph.layerlessNodes,n); return n;
  });
  const ports = owners.map(owner => {const p=new LPort(); $setNode(p,nodes[owner]); return p;});
  const edges = endpoints.map(([source,target]) => {
    const e=new LEdge(); $setSource_0(e,ports[source]); $setTarget_0(e,ports[target]); return e;
  });
  const reversalOrder=[];
  const reverse=$reverse_0;
  $reverse_0=(edge, adapt) => {reversalOrder.push(edges.indexOf(edge)); reverse(edge,adapt);};
  try { $process_57(new DepthFirstCycleBreaker(),graph,new BasicProgressMonitor()); }
  finally { $reverse_0=reverse; }
  return reversalOrder;
}`,context);
const cases = [
  ['empty',0,[],[]],
  ['class-chain',3,[0,1,2],[[0,1],[1,2]]],
  ['class-cycle',3,[0,1,2],[[0,1],[1,2],[2,0]]],
  ['state-source-before-cycle',4,[0,1,2,3],[[0,1],[1,2],[2,0],[3,1]]],
  ['port-order',3,[0,0,1,2],[[1,3],[0,2],[2,3],[3,2],[2,0]]],
  ['parallel-feedback',2,[0,1],[[0,1],[1,0],[1,0]]],
  ['self-and-disconnected',5,[0,1,2,3,4],[[0,0],[0,1],[1,0],[2,3],[3,2]]],
];
const encode = items => items.length ? items.join(',') : '-';
const rows=['# ELK 0.9.3 DepthFirstCycleBreaker; name, node count, port owners, port endpoints, reversal order.'];
for (const [name,nodes,ports,edges] of cases) {
  const result=original(nodes,ports,edges);
  rows.push([name,nodes,encode(ports),encode(edges.map(e=>e.join(':'))),encode(result)].join('\t'));
}
const fixture = new URL('../../tests/mermaid_cpp/fixtures/elk-cycle.tsv',import.meta.url);
const expected = rows.join('\n')+'\n';
if (process.argv.includes('--write')) writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK feedback edges: ${cases.length} original processor observations checked.`);
