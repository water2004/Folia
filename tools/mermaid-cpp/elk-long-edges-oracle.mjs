// Original ELK cycle removal, layering and LongEdgeSplitter graph observations.
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
const original=vm.runInContext(`(count, owners, endpoints, thickness) => {
  const graph=new LGraph();
  const nodes=Array.from({length:count},(_,i)=>{const n=new LNode(graph);n.size_0.x_0=80+i*10;n.size_0.y_0=32+i;$add_3(graph.layerlessNodes,n);return n;});
  const ports=owners.map(owner=>{const p=new LPort();$setNode(p,nodes[owner]);return p;});
  const edges=endpoints.map(([source,target],i)=>{
    const e=new LEdge();$setSource_0(e,ports[source]);$setTarget_0(e,ports[target]);
    $clinit_LayeredOptions();$setProperty_0(e,EDGE_THICKNESS_0,thickness[i]);return e;
  });
  const logical=new Map(edges.map((e,i)=>[e,i]));
  $process_57(new DepthFirstCycleBreaker(),graph,new BasicProgressMonitor());
  $process_68(new NetworkSimplexLayerer(),graph,new BasicProgressMonitor());
  const setLayer=$setLayer_0;
  $setLayer_0=(n,l)=>{if(!nodes.includes(n))nodes.push(n);setLayer(n,l);};
  try{$process_35(graph,new BasicProgressMonitor());}finally{$setLayer_0=setLayer;}
  $clinit_InternalProperties_1();$clinit_PortSide();
  for(const n of nodes.slice(count)){
    ports.push(...n.ports.array);
    const segment=n.ports.array[1].outgoingEdges.array[0];
    const parent=$getProperty(n,ORIGIN_0);
    logical.set(segment,logical.get(parent));edges.push(segment);
  }
  const index=(array,item)=>item==null?-1:array.indexOf(item);
  const list=items=>items.length?items.join(','):'-';
  const nodeRecords=nodes.map((n,i)=>{
    const dummy=n.type_0===LONG_EDGE;
    return [dummy?1:0,dummy?-1:i,index(graph.layers.array,n.layer),n.size_0.x_0,n.size_0.y_0,
      dummy?1:0,index(edges,$getProperty(n,ORIGIN_0)),index(ports,$getProperty(n,LONG_EDGE_SOURCE)),
      index(ports,$getProperty(n,LONG_EDGE_TARGET))].join(',')+':'+list(n.ports.array.map(p=>ports.indexOf(p)));
  });
  const side=p=>p.side===WEST_2?4:p.side===EAST_2?2:0;
  const portRecords=ports.map(p=>[index(nodes,p.owner),side(p),p.pos.x_0,p.pos.y_0].join(',')+':'+
    list(p.incomingEdges.array.map(e=>edges.indexOf(e)))+':'+list(p.outgoingEdges.array.map(e=>edges.indexOf(e))));
  const edgeRecords=edges.map(e=>[logical.get(e),index(ports,e.source),index(ports,e.target),$getProperty(e,EDGE_THICKNESS_0),
    $booleanValue(castToBoolean($getProperty(e,REVERSED)))?1:0].join(','));
  return 'N['+nodeRecords.join(';')+']P['+portRecords.join(';')+']E['+edgeRecords.join(';')+']L['+
    graph.layers.array.map(l=>list(l.nodes.array.map(n=>nodes.indexOf(n)))).join(';')+']';
}`,context);
const cases=[
  ['adjacent',2,[0,1],[[0,1]],[1]],
  ['long-shortcut',4,[0,1,2,3],[[0,1],[1,2],[2,3],[0,3]],[1,1,1,3.5]],
  ['parallel-long',4,[0,1,2,3],[[0,1],[1,2],[2,3],[0,3],[0,3]],[1,1,1,3,1]],
  ['reversed-long',4,[0,1,2,3],[[0,1],[1,2],[2,3],[3,0]],[1,1,1,2]],
  ['port-ordered-long',4,[0,0,1,2,3],[[1,4],[0,2],[2,3],[3,4],[0,4]],[1,1,1,1,2]],
];
const rows=['# ELK 0.9.3 LongEdgeSplitter; name, node count, port owners, endpoints, thickness, exact graph snapshot.'];
for(const [name,count,ports,edges,thickness] of cases)
  rows.push([name,count,ports.join(','),edges.map(e=>e.join(':')).join(','),thickness.join(','),original(count,ports,edges,thickness)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-long-edges.tsv',import.meta.url);
const expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK long edges: ${cases.length} original processor observations checked.`);
