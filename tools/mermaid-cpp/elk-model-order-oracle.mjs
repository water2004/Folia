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
  $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_PortConstraints();$clinit_OrderingStrategy();
  const graph=new LGraph(), nodes=assignment.map(()=>new LNode(graph)), ports=[];
  $setProperty_0(graph,CONSIDER_MODEL_ORDER_STRATEGY_0,NODES_AND_EDGES);
  for(let l=0;l<=Math.max(...assignment);++l)$add_3(graph.layers,new Layer(graph));
  nodes.forEach((n,i)=>{
    $setLayer_0(n,graph.layers.array[assignment[i]]);$setProperty_0(n,MODEL_ORDER_1,valueOf_3(i));
    $setProperty_0(n,PORT_CONSTRAINTS_0,fixed.includes(i)?FIXED_POS:FIXED_SIDE);
  });
  endpoints.forEach(([s,t],i)=>{
    const a=new LPort(),b=new LPort(),e=new LEdge();$setProperty_0(e,MODEL_ORDER_1,valueOf_3(i));
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
  new SortByInputModelProcessor().process(graph,new BasicProgressMonitor());
  const sorted=snapshot();
  const random=new Random_0(fromDouble_0(seed));$setProperty_0(graph,RANDOM_0,random);
  const processor=new LayerSweepCrossingMinimizer(($clinit_LayerSweepCrossingMinimizer$CrossMinType(),BARYCENTER));
  $process_71(processor,graph,new BasicProgressMonitor());
  const holder=processor.graphInfoHolders?.array[0];
  const crossings=holder?$countAllCrossings(holder.crossingsCounter,$toNodeArray(graph)):0;
  return [sorted,snapshot(),crossings,$nextDouble(random)].join('\t');
}`,context);
const cases=[
 ['flowchart-triangle',[0,2,1],[[0,1],[0,2],[2,1]],[],1],
 ['flowchart-fan',[0,1,1],[[0,1],[0,2]],[],1],
];
const rows=['# ELK 0.9.3 flat crossing order; name, layers, node endpoints, fixed nodes, seed, sorted order, best order, crossings, next random double.'];
for(const [name,layers,edges,fixed,seed] of cases)
 rows.push([name,layers.join(','),edges.map(e=>e.join(':')).join(',')||'-',fixed.join(',')||'-',seed,original(layers,edges,fixed,seed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-model-order.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK default model order: ${cases.length} original processor observations checked.`);
