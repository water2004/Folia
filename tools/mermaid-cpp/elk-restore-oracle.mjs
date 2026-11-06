// Exact original LongEdgeJoiner and ReversedEdgeRestorer path observations.
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
const original=vm.runInContext(`(assignment,endpoints,reversed) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_LNode$NodeType();
 const graph=new LGraph(),nodes=assignment.map(()=>new LNode(graph)),ports=[];
 for(let l=0;l<=Math.max(...assignment);++l)$add_3(graph.layers,new Layer(graph));
 nodes.forEach((n,i)=>{$setLayer_0(n,graph.layers.array[assignment[i]]);n.type_0=NORMAL;n.size_0.x_0=40+i*5;n.size_0.y_0=24+i;});
 const originalEdges=endpoints.map(([s,t])=>{
  const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);
  $setSource_0(e,a);$setTarget_0(e,b);ports.push(a,b);return e;
 });
 reversed.forEach(e=>$reverse_0(originalEdges[e],false));
 $process_43(graph,new BasicProgressMonitor());
 const setLayer=$setLayer_0;
 $setLayer_0=(n,l)=>{if(!nodes.includes(n))nodes.push(n);setLayer(n,l);};
 try{$process_35(graph,new BasicProgressMonitor());}finally{$setLayer_0=setLayer;}
 const segments=[...originalEdges];
 for(const n of nodes.slice(assignment.length)){ports.push(...n.ports.array);segments.push(n.ports.array[1].outgoingEdges.array[0]);}
 nodes.forEach((n,i)=>{n.pos.x_0=graph.layers.array.indexOf(n.layer)*100;n.pos.y_0=i*30;
  n.ports.array.forEach(p=>{p.pos.x_0=p.side===EAST_2?n.size_0.x_0:0;p.pos.y_0=n.size_0.y_0/2;});});
 segments.forEach(e=>{const a=$getAbsoluteAnchor(e.source),b=$getAbsoluteAnchor(e.target),x=(a.x_0+b.x_0)/2;
  $add_7(e.bendPoints,new KVector_1(x,a.y_0));$add_7(e.bendPoints,new KVector_1(x,b.y_0));});
 $process_34(graph,new BasicProgressMonitor());$process_45(graph,new BasicProgressMonitor());
 const point=p=>p.x_0+','+p.y_0;
 return originalEdges.map(e=>{const bends=[];for(let p=e.bendPoints.header.next_0;p!==e.bendPoints.tail;p=p.next_0)bends.push(point(p.value_0));
  return [nodes.indexOf(e.source.owner),nodes.indexOf(e.target.owner),ports.indexOf(e.source),ports.indexOf(e.target),
   point($getAbsoluteAnchor(e.source)),point($getAbsoluteAnchor(e.target)),bends.join(':')||'-'].join('|');}).join(';');
}`,context);
const cases=[
 ['adjacent',[0,1],[[0,1]],[]],
 ['long',[0,1,2,3],[[0,1],[1,2],[2,3],[0,3]],[]],
 ['feedback-long',[0,1,2,3],[[0,1],[1,2],[2,3],[3,0]],[3]],
 ['parallel-long',[0,1,2,3],[[0,3],[0,3],[3,0]],[2]],
];
const rows=['# ELK restored paths; name, layers, original endpoints, reversed edges, routes(source-node|target-node|source-port|target-port|start|end|bends).'];
for(const [name,layers,edges,reversed] of cases)
 rows.push([name,layers.join(','),edges.map(e=>e.join(':')).join(','),reversed.join(',')||'-',original(layers,edges,reversed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-restore.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK restoration: ${cases.length} original joined/feedback path observations checked.`);
