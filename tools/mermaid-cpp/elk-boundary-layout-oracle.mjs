// Original local child-graph ordering, external-node BK and boundary routing.
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
const original=vm.runInContext(`(roles,endpoints) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_LNode$NodeType();$clinit_LayerConstraint();$clinit_LayerConstraintPreprocessor();$clinit_PortConstraints();$clinit_PortSide();$clinit_GraphProperties();
 const graph=new LGraph();$getProperty(graph,GRAPH_PROPERTIES).add_2(EXTERNAL_PORTS);$setProperty_0(graph,PORT_CONSTRAINTS_0,FREE);
 const margin=new ElkMargin();margin.top_0=margin.bottom=margin.left=margin.right=12;$setProperty_0(graph,SPACING_PORTS_SURROUNDING,margin);
 graph.padding.top_0=graph.padding.bottom=graph.padding.left=graph.padding.right=12;
 const nodes=roles.map((role,i)=>{const n=new LNode(graph);n.type_0=role?EXTERNAL_PORT:NORMAL;
  if(role){$setProperty_0(n,LAYERING_LAYER_CONSTRAINT_0,role===1?FIRST_SEPARATE_0:LAST_SEPARATE_0);$setProperty_0(n,PORT_CONSTRAINTS_0,FIXED_POS);
   $setProperty_0(n,EXT_PORT_SIDE,role===1?WEST_2:EAST_2);$setProperty_0(n,EXT_PORT_SIZE,new KVector());$setProperty_0(n,PORT_BORDER_OFFSET,5);}
  else {n.size_0.x_0=50+i*5;n.size_0.y_0=38+i*2;}$add_3(graph.layerlessNodes,n);return n;});
 const edges=endpoints.map(([s,t])=>{const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);$setSource_0(e,a);$setTarget_0(e,b);return e;});
 $process_57(new DepthFirstCycleBreaker(),graph,new BasicProgressMonitor());$process_32(graph,new BasicProgressMonitor());
 $process_68(new NetworkSimplexLayerer(),graph,new BasicProgressMonitor());$process_31(graph,new BasicProgressMonitor());
 const setLayer=$setLayer_0;$setLayer_0=(n,l)=>{if(!nodes.includes(n))nodes.push(n);setLayer(n,l);};
 try{$process_35(graph,new BasicProgressMonitor());}finally{$setLayer_0=setLayer;}
 $process_43(graph,new BasicProgressMonitor());new PortListSorter().process(graph,new BasicProgressMonitor());
 $setProperty_0(graph,RANDOM_0,new Random_0(fromDouble_0(1)));
 $process_71(new LayerSweepCrossingMinimizer(($clinit_LayerSweepCrossingMinimizer$CrossMinType(),BARYCENTER)),graph,new BasicProgressMonitor());
 $process_25(graph,new BasicProgressMonitor());$setProperty_0(graph,SPACINGS,new Spacings(graph));$setProperty_0(graph,NODE_PLACEMENT_FAVOR_STRAIGHT_EDGES_0,false);
 $process_77(new BKNodePlacer(),graph,new BasicProgressMonitor());$process_33(graph,new BasicProgressMonitor());$process_79(graph,new BasicProgressMonitor());
 $process_16(new HierarchicalPortOrthogonalEdgeRouter(),graph,new BasicProgressMonitor());
 const point=p=>p.x_0+','+p.y_0;
 const exported=roles.map((r,i)=>r?point(getExternalPortPosition(graph,nodes[i],0,0)):'-').join(';');
 const layers=graph.layers.array.map(l=>l.nodes.array.map(n=>nodes.indexOf(n)).join(',')).join(';');
 const positions=nodes.map(n=>point(n.pos)).join(';');
 $setProperty_0(graph,UNNECESSARY_BENDPOINTS_0,true);$process_34(graph,new BasicProgressMonitor());$process_45(graph,new BasicProgressMonitor());
 const paths=edges.map(e=>{const bends=[];for(let p=e.bendPoints.header.next_0;p!==e.bendPoints.tail;p=p.next_0)bends.push(point(p.value_0));
  return [point($getAbsoluteAnchor(e.source)),point($getAbsoluteAnchor(e.target)),bends.join(':')||'-'].join('|');}).join(';');
 return [layers,positions,[graph.size_0.x_0,graph.size_0.y_0,graph.offset.y_0].join(','),exported,paths].join('\t');
}`,context);
const cases=[
 ['through-chain',[0,0,1,2],[[2,0],[0,1],[1,3]]],
 ['boundary-fan',[0,0,1,1,2,2],[[2,0],[3,1],[0,1],[0,4],[1,5]]],
 ['boundary-long',[0,0,0,1,2,2],[[3,0],[0,1],[1,2],[0,4],[2,5]]],
 ['boundary-only',[1,2],[[0,1]]],
];
const rows=['# ELK local boundary layout; name,roles,endpoints,layers,raw node positions,content width/height/y offset,parent port positions,original segment paths.'];
for(const [name,roles,edges]of cases)rows.push([name,roles.join(','),edges.map(e=>e.join(':')).join(','),original(roles,edges)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-boundary-layout.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK boundary layout: ${cases.length} original local ordering/placement/routing cases checked.`);
