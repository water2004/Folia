// Original N/S preprocessor topology and orthogonal postprocessor geometry.
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
const original=vm.runInContext(`(assignment,endpoints,fixed) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_LNode$NodeType();$clinit_PortSide();$clinit_PortConstraints();$clinit_EdgeRouting();
 const graph=new LGraph(),nodes=assignment.map(()=>new LNode(graph)),ports=[],edges=[];
 $setProperty_0(graph,EDGE_ROUTING,ORTHOGONAL);
 for(let l=0;l<=Math.max(...assignment);++l)$add_3(graph.layers,new Layer(graph));
 nodes.forEach((n,i)=>{$setLayer_0(n,graph.layers.array[assignment[i]]);$setProperty_0(n,PORT_CONSTRAINTS_0,fixed.includes(i)?FIXED_POS:FIXED_SIDE);});
 const sides=[null,NORTH_3,EAST_2,SOUTH_2,WEST_2];
 endpoints.forEach(([s,t,ss,ts])=>{const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);a.side=sides[ss];b.side=sides[ts];$setSource_0(e,a);$setTarget_0(e,b);ports.push(a,b);edges.push(e);});
 new PortListSorter().process(graph,new BasicProgressMonitor());
 const setNode=$setNode;$setNode=(p,n)=>{if(!nodes.includes(n))nodes.push(n);if(!ports.includes(p))ports.push(p);setNode(p,n);};
 try{$process_38(graph,new BasicProgressMonitor());}finally{$setNode=setNode;}
 const random=new Random_0(fromDouble_0(1));$setProperty_0(graph,RANDOM_0,random);
 const processor=new LayerSweepCrossingMinimizer(($clinit_LayerSweepCrossingMinimizer$CrossMinType(),BARYCENTER));
 $process_71(processor,graph,new BasicProgressMonitor());
 nodes.slice(0,assignment.length).forEach((n,i)=>{n.size_0.x_0=50+5*i;n.size_0.y_0=38+2*i;$setProperty_0(n,PORT_CONSTRAINTS_0,FIXED_POS);});
 ports.slice(0,2*endpoints.length).forEach((p,i)=>{
  p.pos.x_0=p.side===EAST_2?p.owner.size_0.x_0:p.side===WEST_2?0:10+3*i;
  p.pos.y_0=p.side===NORTH_3?-5:p.side===SOUTH_2?p.owner.size_0.y_0+5:p.owner.size_0.y_0/2;
  p.owner.margin.top_0=Math.max(p.owner.margin.top_0,-p.pos.y_0);p.owner.margin.bottom=Math.max(p.owner.margin.bottom,p.pos.y_0-p.owner.size_0.y_0);
 });
 $setProperty_0(graph,SPACINGS,new Spacings(graph));$setProperty_0(graph,NODE_PLACEMENT_FAVOR_STRAIGHT_EDGES_0,false);
 $process_77(new BKNodePlacer(),graph,new BasicProgressMonitor());$process_33(graph,new BasicProgressMonitor());$process_79(graph,new BasicProgressMonitor());
 const point=p=>p.x_0+','+p.y_0,positions=nodes.map(n=>point(n.pos)).join(';');
 $process_37(graph,new BasicProgressMonitor());
 const paths=edges.map(e=>{const bends=[];for(let p=e.bendPoints.header.next_0;p!==e.bendPoints.tail;p=p.next_0)bends.push(point(p.value_0));
  return [point($getAbsoluteAnchor(e.source)),point($getAbsoluteAnchor(e.target)),bends.join(':')||'-'].join('|');}).join(';');
 return [positions,[graph.size_0.x_0,graph.size_0.y_0,graph.offset.y_0].join(','),paths,$nextDouble(random)].join('\t');
}`,context);
const cases=[
 ['north-in-out',[0,1,2],[[0,1,2,1],[1,2,1,4]],[]],
 ['south-in-out',[0,1,2],[[0,1,2,3],[1,2,3,4]],[]],
 ['mixed-fan',[0,0,1,2,2],[[0,2,2,1],[1,2,2,3],[2,3,3,4],[2,4,1,4]],[]],
 ['fixed-north',[0,0,1,2,2],[[2,3,1,4],[0,2,2,1],[2,4,1,4],[1,2,2,1]],[2]],
];
const rows=['# ELK NS placement/routing; name,layers,edges,fixed,raw positions,width/height/y offset,paths,next random double.'];
for(const [name,layers,edges,fixed]of cases)rows.push([name,layers.join(','),edges.map(e=>e.join(':')).join(','),fixed.join(',')||'-',original(layers,edges,fixed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-north-south-layout.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK N/S placement/routing: ${cases.length} original pre/post cases checked.`);
