// Original inline center-label processors and LABEL-aware BK geometry.
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
const original=vm.runInContext(`(count,endpoints,measurements,direction) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_LNode$NodeType();
 $clinit_Direction_0();$clinit_EdgeLabelPlacement();$clinit_LabelSide();
 const graph=new LGraph(),nodes=Array.from({length:count},()=>new LNode(graph));
 $setProperty_0(graph,DIRECTION,direction===0?RIGHT_6:direction===1?DOWN_1:UP_1);
 $setProperty_0(graph,SPACING_EDGE_LABEL_0,2);$setProperty_0(graph,SPACING_LABEL_LABEL,3);
 nodes.forEach(n=>{n.type_0=NORMAL;$add_3(graph.layerlessNodes,n);});
 const edges=endpoints.map(([s,t])=>{const a=new LPort(),b=new LPort(),e=new LEdge();
  $setNode(a,nodes[s]);$setNode(b,nodes[t]);$setSource_0(e,a);$setTarget_0(e,b);return e;});
 const labels=measurements.map(([e,w,h],i)=>{const l=new LLabel();l.size_0.x_0=w;l.size_0.y_0=h;
  $setProperty_0(l,EDGE_LABELS_PLACEMENT,CENTER_5);$setProperty_0(l,EDGE_LABELS_INLINE_0,true);$add_3(edges[e].labels,l);return l;});
 $process_57(new DepthFirstCycleBreaker(),graph,new BasicProgressMonitor());
 $clinit_LabelDummyInserter();$process_26(graph,new BasicProgressMonitor());nodes.push(...graph.layerlessNodes.array.slice(count));
 const segments=[...edges];for(const n of nodes.slice(count))segments.push(n.ports.array[1].outgoingEdges.array[0]);
 const inserted=nodes.slice(count).map(n=>[nodes.indexOf(n),n.size_0.x_0,n.size_0.y_0].join(',')).join(';');
 $process_68(new NetworkSimplexLayerer(),graph,new BasicProgressMonitor());
 const before=graph.layers.array.map(l=>l.nodes.array.map(n=>nodes.indexOf(n)).join(',')).join(';');
 $process_43(graph,new BasicProgressMonitor());
 const beforeSplit=nodes.length;const setLayer=$setLayer_0;$setLayer_0=(n,l)=>{if(!nodes.includes(n)){nodes.push(n);}setLayer(n,l);};
 try{$process_35(graph,new BasicProgressMonitor());}finally{$setLayer_0=setLayer;}
 for(const n of nodes.slice(beforeSplit))segments.push(n.ports.array[1].outgoingEdges.array[0]);$process_28(new LabelDummySwitcher(),graph,new BasicProgressMonitor());
 const layers=graph.layers.array.map(l=>l.nodes.array.map(n=>nodes.indexOf(n)).join(',')).join(';');
 for(const n of nodes)if(n.type_0===LABEL)$applyLabelSide_1(n,BELOW);
 nodes.forEach((n,i)=>{if(i<count){n.size_0.x_0=40+i*5;n.size_0.y_0=24+i;}
  n.ports.array.forEach(p=>{p.pos.x_0=p.side===EAST_2?n.size_0.x_0:0;if(i<count)p.pos.y_0=n.size_0.y_0/2;});});
 $setProperty_0(graph,SPACINGS,new Spacings(graph));$setProperty_0(graph,NODE_PLACEMENT_FAVOR_STRAIGHT_EDGES_0,false);
 $process_77(new BKNodePlacer(),graph,new BasicProgressMonitor());
 nodes.forEach(n=>{n.pos.x_0=graph.layers.array.indexOf(n.layer)*100;});
 const geometry=nodes.map(n=>[n.type_0===NORMAL?0:n.type_0===LONG_EDGE?1:2,n.size_0.x_0,n.size_0.y_0,
  n.pos.y_0,n.ports.array[0]?.pos.y_0??0].join(',')).join(';');
 const point=p=>p.x_0+','+p.y_0;
 segments.forEach(e=>{const a=$getAbsoluteAnchor(e.source),b=$getAbsoluteAnchor(e.target),x=(a.x_0+b.x_0)/2;
  $add_7(e.bendPoints,new KVector_1(x,a.y_0));$add_7(e.bendPoints,new KVector_1(x,b.y_0));});
 $process_34(graph,new BasicProgressMonitor());$process_27(graph,new BasicProgressMonitor());$process_45(graph,new BasicProgressMonitor());
 const restored=labels.map(l=>point(l.pos)).join(';');
 const routes=edges.map(e=>{const bends=[];for(let p=e.bendPoints.header.next_0;p!==e.bendPoints.tail;p=p.next_0)bends.push(point(p.value_0));
  return [nodes.indexOf(e.source.owner),nodes.indexOf(e.target.owner),point($getAbsoluteAnchor(e.source)),point($getAbsoluteAnchor(e.target)),bends.join(':')].join('|');}).join(';');
 return [inserted,before,layers,geometry,restored,routes].join('\t');
}`,context);
const cases=[
 ['single',2,[[0,1]],[[0,36,14]],0],
 ['empty-inline',2,[[0,1]],[[0,0,0]],0],
 ['multiple-horizontal',2,[[0,1]],[[0,36,14],[0,18,10]],0],
 ['multiple-down',2,[[0,1]],[[0,36,14],[0,18,10]],1],
 ['multiple-up',2,[[0,1]],[[0,36,14],[0,18,10]],2],
 ['long-median',5,[[0,1],[1,2],[2,3],[3,4],[0,4]],[[4,48,16]],0],
 ['feedback',4,[[0,1],[1,2],[2,3],[3,0]],[[3,42,15]],0],
 ['parallel-labels',2,[[0,1],[0,1]],[[0,40,14],[1,20,10]],0],
];
const rows=['# ELK labels; name,count,endpoints,labels(edge,width,height),direction,inserted,layering,switched layers,node(kind,width,height,y,portY),label XY,restored paths.'];
for(const [name,count,edges,labels,dir]of cases)
 rows.push([name,count,edges.map(e=>e.join(':')).join(','),labels.map(l=>l.join(':')).join(','),dir,original(count,edges,labels,dir)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-labels.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK inline labels: ${cases.length} original insertion/switch/placement/restoration observations checked.`);


