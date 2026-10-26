// Exact ELK 0.9.3 OrthogonalEdgeRouter observations on placed flat graphs.
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
const original=vm.runInContext(`(assignment, positions, endpoints, dummies, seed) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_LNode$NodeType();
 const graph=new LGraph(),nodes=assignment.map(()=>new LNode(graph));
 for(let l=0;l<=Math.max(...assignment);++l)$add_3(graph.layers,new Layer(graph));
 nodes.forEach((n,i)=>{
  $setLayer_0(n,graph.layers.array[assignment[i]]);n.pos.y_0=positions[i];
  n.size_0.x_0=dummies.includes(i)?0:40+i*7;n.size_0.y_0=dummies.includes(i)?1:24;
  n.type_0=dummies.includes(i)?LONG_EDGE:NORMAL;
  n.margin.left=i%3;n.margin.right=(i+1)%4;
  n.layer.size_0.x_0=Math.max(n.layer.size_0.x_0,n.size_0.x_0+n.margin.left+n.margin.right);
 });
 const edges=endpoints.map(([s,t,sy,ty])=>{
  const a=new LPort(),b=new LPort(),e=new LEdge();
  $setNode(a,nodes[s]);$setNode(b,nodes[t]);a.side=EAST_2;b.side=WEST_2;
  a.pos.y_0=sy;b.pos.y_0=ty;$setSource_0(e,a);$setTarget_0(e,b);return e;
 });
 $setProperty_0(graph,RANDOM_0,new Random_0(fromDouble_0(seed)));
 const slots=[],route=$routeEdges_0;
 $routeEdges_0=(...args)=>{const n=route(...args);slots.push(n);return n;};
 try{$process_79(graph,new BasicProgressMonitor());}finally{$routeEdges_0=route;}
 const bends=edges.map(e=>{const points=[];for(let p=e.bendPoints.header.next_0;p!==e.bendPoints.tail;p=p.next_0)
  points.push(p.value_0.x_0+','+p.value_0.y_0);return points.join(':')||'-';});
 return [nodes.map(n=>n.pos.x_0).join(','),graph.layers.array.map(l=>l.size_0.x_0).join(','),
  slots.join(','),graph.size_0.x_0,bends.join(';')||'-'].join('\t');
}`,context);
const cases=[
 ['straight',[0,1],[0,0],[[0,1,12,12]],[],1],
 ['fan-out',[0,1,1],[20,0,70],[[0,1,5,12],[0,2,19,12]],[],1],
 ['fan-in',[0,0,1],[0,70,20],[[0,2,12,5],[1,2,12,19]],[],1],
 ['crossed',[0,0,1,1],[0,70,0,70],[[0,3,12,12],[1,2,12,12]],[],1],
 ['crossed-seed-42',[0,0,1,1],[0,70,0,70],[[0,3,12,12],[1,2,12,12]],[],42],
 ['long-edge',[0,1,1,2],[20,0,75,25],[[0,1,5,12],[0,2,19,0],[1,3,12,5],[2,3,0,19]],[2],1],
 ['disconnected',[0,0,1,1,2],[0,70,10,80,20],[[0,2,12,12],[2,4,12,12]],[],1],
];
const rows=['# ELK 0.9.3 routing; name, layers, node y, edges(source:target:source-y:target-y), dummy nodes, seed, node x, layer widths, gap slots, width, edge bends.'];
for(const [name,layers,positions,edges,dummies,seed] of cases)
 rows.push([name,layers.join(','),positions.join(','),edges.map(e=>e.join(':')).join(',')||'-',
  dummies.join(',')||'-',seed,original(layers,positions,edges,dummies,seed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-routing.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK routing: ${cases.length} original slots, bends and horizontal placement observations checked.`);
