// Exact flat PortListSorter / LayerSweepCrossingMinimizer observations.
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
const original=vm.runInContext(`(assignment, endpoints, fixed, seed) => {
  $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_PortConstraints();
  const graph=new LGraph(), nodes=assignment.map(()=>new LNode(graph)), ports=[];
  for(let l=0;l<=Math.max(...assignment);++l)$add_3(graph.layers,new Layer(graph));
  nodes.forEach((n,i)=>{
    $setLayer_0(n,graph.layers.array[assignment[i]]);
    $setProperty_0(n,PORT_CONSTRAINTS_0,fixed.includes(i)?FIXED_POS:FIXED_SIDE);
  });
  endpoints.forEach(([s,t],i)=>{
    const a=new LPort(),b=new LPort(),e=new LEdge();
    $setNode(a,nodes[s]);$setNode(b,nodes[t]);a.side=EAST_2;b.side=WEST_2;
    a.pos.y_0=(i*7)%23;b.pos.y_0=(i*11)%19;
    $setSource_0(e,a);$setTarget_0(e,b);ports.push(a,b);
  });
  const setLayer=$setLayer_0;
  $setLayer_0=(n,l)=>{if(!nodes.includes(n))nodes.push(n);setLayer(n,l);};
  try{$process_35(graph,new BasicProgressMonitor());}finally{$setLayer_0=setLayer;}
  for(const n of nodes.slice(assignment.length))ports.push(...n.ports.array);
  const snapshot=()=>graph.layers.array.map(l=>l.nodes.array.map(n=>nodes.indexOf(n)).join(',')).join(';')+'|'+
    nodes.map(n=>n.ports.array.map(p=>ports.indexOf(p)).join(',')||'-').join(';');
  new PortListSorter().process(graph,new BasicProgressMonitor());
  const sorted=snapshot();
  $setProperty_0(graph,RANDOM_0,new Random_0(fromDouble_0(seed)));
  const processor=new LayerSweepCrossingMinimizer(($clinit_LayerSweepCrossingMinimizer$CrossMinType(),BARYCENTER));
  $process_71(processor,graph,new BasicProgressMonitor());
  const holder=processor.graphInfoHolders?.array[0];
  const crossings=holder?$countAllCrossings(holder.crossingsCounter,$toNodeArray(graph)):0;
  return [sorted,snapshot(),crossings].join('\t');
}`,context);
const cases=[
 ['crossed',[0,0,1,1],[[0,3],[1,2]],[],1],
 ['diamond',[0,1,1,2],[[0,1],[0,2],[1,3],[2,3]],[],1],
 ['three-layers',[0,0,0,1,1,1,2,2,2],[[0,5],[1,4],[2,3],[3,8],[4,7],[5,6],[0,4],[4,6]],[],1],
 ['dense',[0,0,0,1,1,1],[[0,3],[0,4],[0,5],[1,3],[1,4],[1,5],[2,3],[2,4],[2,5]],[],1],
 ['dense-seed-42',[0,0,0,1,1,1],[[0,3],[0,4],[0,5],[1,3],[1,4],[1,5],[2,3],[2,4],[2,5]],[],42],
 ['fixed-ports',[0,0,1,1,1],[[0,2],[0,3],[0,4],[1,2],[1,3],[1,4]],[0,1,2,3,4],1],
 ['long-edges',[0,1,1,2,3],[[0,1],[0,2],[1,3],[2,3],[3,4],[0,4]],[],1],
 ['disconnected',[0,0,0,1,1,1,2],[[0,4],[1,3],[4,6]],[],1],
 ['isolated-layer',[0,0,0],[],[],1],
 ['single',[0],[],[],1],
];
const rows=['# ELK 0.9.3 flat crossing order; name, layers, node endpoints, fixed nodes, seed, sorted order, best order, crossings.'];
for(const [name,layers,edges,fixed,seed] of cases)
 rows.push([name,layers.join(','),edges.map(e=>e.join(':')).join(',')||'-',fixed.join(',')||'-',seed,original(layers,edges,fixed,seed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-order.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK crossing order: ${cases.length} original processor observations checked.`);
