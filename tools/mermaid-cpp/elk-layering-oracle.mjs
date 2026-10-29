// Original ELK flat LGraph: cycle removal followed by NetworkSimplexLayerer.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync, writeFileSync, realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';
import vm from 'node:vm';
const require=createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url)));
const base=path.dirname(require.resolve('elkjs/package.json'));
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
for(const file of lock.elkCycle.files)
  assert.equal(createHash('sha256').update(readFileSync(path.join(base,file.path))).digest('hex'),file.sha256,file.path);
const context=vm.createContext({module:{exports:{}},exports:{},setTimeout,clearTimeout});context.global=context;
vm.runInContext(readFileSync(path.join(base,'lib/elk-worker.js'),'utf8'),context);
const original=vm.runInContext(`(count, owners, endpoints, priorities) => {
  const graph=new LGraph();
  const nodes=Array.from({length:count},()=>{const n=new LNode(graph);$add_3(graph.layerlessNodes,n);return n;});
  const ports=owners.map(owner=>{const p=new LPort();$setNode(p,nodes[owner]);return p;});
  const edges=endpoints.map(([source,target],i)=>{
    const e=new LEdge();$setSource_0(e,ports[source]);$setTarget_0(e,ports[target]);
    if(priorities.length){$clinit_LayeredOptions();$setProperty_0(e,PRIORITY_SHORTNESS_0,valueOf_3(priorities[i]));}
    return e;
  });
  const reversed=[];const reverse=$reverse_0;
  $reverse_0=(edge,adapt)=>{reversed.push(edges.indexOf(edge));reverse(edge,adapt);};
  try{$process_57(new DepthFirstCycleBreaker(),graph,new BasicProgressMonitor());}
  finally{$reverse_0=reverse;}
  $process_68(new NetworkSimplexLayerer(),graph,new BasicProgressMonitor());
  return {reversed,layers:nodes.map(n=>graph.layers.array.indexOf(n.layer)),
    layerNodes:graph.layers.array.map(layer=>layer.nodes.array.map(n=>nodes.indexOf(n)))};
}`,context);
const cases=[
  ['empty',0,[],[],[]],
  ['chain',3,[0,1,2],[[0,1],[1,2]],[]],
  ['diamond',4,[0,1,2,3],[[0,1],[0,2],[1,3],[2,3]],[]],
  ['unequal-path',5,[0,1,2,3,4],[[0,1],[1,2],[2,4],[0,3],[3,4]],[]],
  ['weighted-unequal-path',5,[0,1,2,3,4],[[0,1],[1,2],[2,4],[0,3],[3,4]],[1,1,1,1,4]],
  ['cycle',3,[0,1,2],[[0,1],[1,2],[2,0]],[]],
  ['source-before-cycle',4,[0,1,2,3],[[0,1],[1,2],[2,0],[3,1]],[]],
  ['port-order',3,[0,0,1,2],[[1,3],[0,2],[2,3],[3,2],[2,0]],[]],
  ['parallel-feedback',2,[0,1],[[0,1],[1,0],[1,0]],[]],
  ['self-disconnected',7,[0,1,2,3,4,5,6],[[0,0],[0,1],[1,0],[2,3],[3,4],[2,5],[5,4]],[]],
  // ELK itself changes algorithm execution at 40 nodes by detaching subtrees.
  ['subtree-threshold',40,Array.from({length:40},(_,i)=>i),Array.from({length:39},(_,i)=>[i,i+1]),[]],
];
const encode=items=>items.length?items.join(','):'-';
const rows=['# ELK 0.9.3 flat network-simplex layering; name, node count, port owners, endpoints, priorities, reversed edges, layers, layer node order.'];
for(const [name,count,ports,edges,priorities] of cases){
  const actual=original(count,ports,edges,priorities);
  rows.push([name,count,encode(ports),encode(edges.map(e=>e.join(':'))),encode(priorities),encode(actual.reversed),encode(actual.layers),
    actual.layerNodes.length?actual.layerNodes.map(encode).join(';'):'-'].join('\t'));
}
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-layering.tsv',import.meta.url);
const expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK flat layering: ${cases.length} original processor observations checked.`);
