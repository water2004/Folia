// Original state continuity across flat crossing, point ports, BK and routing.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,realpathSync} from 'node:fs';
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
const original=vm.runInContext(`(assignment,endpoints,seed) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_LNode$NodeType();$clinit_PortConstraints();
 const graph=new LGraph(),nodes=assignment.map(()=>new LNode(graph)),edges=[];
 for(let l=0;l<=Math.max(...assignment);++l)$add_3(graph.layers,new Layer(graph));
 nodes.forEach((n,i)=>{$setLayer_0(n,graph.layers.array[assignment[i]]);n.type_0=NORMAL;n.size_0.x_0=50+i*7;n.size_0.y_0=38+i*2;});
 endpoints.forEach(([s,t])=>{
  const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);
  $setSource_0(e,a);$setTarget_0(e,b);edges.push(e);
 });
 const margin=new ElkMargin();margin.top_0=margin.bottom=margin.left=margin.right=12;
 $setProperty_0(graph,SPACING_PORTS_SURROUNDING,margin);
 $process_43(graph,new BasicProgressMonitor());
 const setLayer=$setLayer_0;
 $setLayer_0=(n,l)=>{if(!nodes.includes(n))nodes.push(n);setLayer(n,l);};
 try{$process_35(graph,new BasicProgressMonitor());}finally{$setLayer_0=setLayer;}
 for(const n of nodes.slice(assignment.length))edges.push(n.ports.array[1].outgoingEdges.array[0]);
 new PortListSorter().process(graph,new BasicProgressMonitor());
 const random=new Random_0(fromDouble_0(seed));$setProperty_0(graph,RANDOM_0,random);
 $process_71(new LayerSweepCrossingMinimizer(($clinit_LayerSweepCrossingMinimizer$CrossMinType(),BARYCENTER)),graph,new BasicProgressMonitor());
 const orderState=random.seedhi*2**24+random.seedlo;
 $process_25(graph,new BasicProgressMonitor());
 $setProperty_0(graph,SPACINGS,new Spacings(graph));
 $setProperty_0(graph,NODE_PLACEMENT_FAVOR_STRAIGHT_EDGES_0,false);
 $process_77(new BKNodePlacer(),graph,new BasicProgressMonitor());
 graph.layers.array.forEach(l=>{l.size_0.x_0=Math.max(...l.nodes.array.map(n=>n.size_0.x_0));});
 $process_79(graph,new BasicProgressMonitor());
 const bends=edges.map(e=>{const points=[];for(let p=e.bendPoints.header.next_0;p!==e.bendPoints.tail;p=p.next_0)
  points.push(p.value_0.x_0+','+p.value_0.y_0);return points.join(':')||'-';});
 return [orderState,random.seedhi*2**24+random.seedlo,
  graph.layers.array.map(l=>l.nodes.array.map(n=>nodes.indexOf(n)).join(',')).join(';'),
  nodes.map(n=>n.pos.x_0).join(','),nodes.map(n=>n.pos.y_0).join(','),graph.size_0.x_0,bends.join(';')].join('\t');
}`,context);
const cases=[
 ['diamond',[0,1,1,2],[[0,1],[0,2],[1,3],[2,3]],1],
 ['dense',[0,0,0,1,1,1],[[0,3],[0,4],[0,5],[1,3],[1,4],[1,5],[2,3],[2,4],[2,5]],1],
 ['dense-42',[0,0,0,1,1,1],[[0,3],[0,4],[0,5],[1,3],[1,4],[1,5],[2,3],[2,4],[2,5]],42],
 ['long-shortcut',[0,1,2,3],[[0,1],[1,2],[2,3],[0,3]],1],
];
const rows=['# ELK flat phase continuity; name, layers, endpoints, seed, order RNG state, routing RNG state, ordered layers, node x, node y, width, edge bends.'];
for(const [name,layers,edges,seed] of cases)
 rows.push([name,layers.join(','),edges.map(e=>e.join(':')).join(','),seed,original(layers,edges,seed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-pipeline.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK pipeline: ${cases.length} original continuous RNG/geometry observations checked.`);
