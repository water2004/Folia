// Original hierarchy sweep selection and bidirectional parent/dummy transfer.
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
const original=vm.runInContext(`(assignment,roles,endpoints,fixed) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_LNode$NodeType();$clinit_PortSide();$clinit_PortConstraints();
 const graph=new LGraph(),outer=new LGraph(),parent=new LNode(outer),parentPorts=[];
 $setProperty_0(parent,PORT_CONSTRAINTS_0,fixed?FIXED_ORDER:FIXED_SIDE);
 const nodes=roles.map(role=>{const n=new LNode(graph);n.type_0=role?EXTERNAL_PORT:NORMAL;
  if(role){const p=new LPort();$setNode(p,parent);p.side=role===1?WEST_2:EAST_2;
   $setProperty_0(p,INSIDE_CONNECTIONS,true);$setProperty_0(p,PORT_DUMMY,n);$setProperty_0(n,ORIGIN_0,p);parentPorts.push(p);}return n;});
 for(let l=0;l<=Math.max(...assignment);++l)$add_3(graph.layers,new Layer(graph));
 nodes.forEach((n,i)=>$setLayer_0(n,graph.layers.array[assignment[i]]));
 endpoints.forEach(([s,t])=>{const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);a.side=EAST_2;b.side=WEST_2;$setSource_0(e,a);$setTarget_0(e,b);});
 parent.ports.array.sort((a,b)=>a.side.ordinal-b.side.ordinal||parentPorts.indexOf(b)-parentPorts.indexOf(a));
 const order=$toNodeArray(graph),decider=new LayerSweepTypeDecider({lGraph:graph,hasParent:true,parent_0:parent,currentNodeOrder:order,crossMinimizer:{isDeterministic:()=>false}});
 order.forEach((layer,l)=>{decider.initAtLayerLevel(l,order);layer.forEach((n,i)=>decider.initAtNodeLevel(l,i,order));});
 const bottom=$useBottomUp(decider),initial=parent.ports.array.slice(),out=[];
 for(const forward of [true,false]) {
  parent.ports.array=initial.slice();const current=order.map(l=>l.slice()),near=forward?0:current.length-1,far=forward?current.length-1:0;
  let entered=false;if(current[near][0].type_0===EXTERNAL_PORT){current[near]=$sortPortDummiesByPortPositions(parent,current[near],forward?WEST_2:EAST_2);entered=true;}
  out.push(+entered,current[near].map(n=>nodes.indexOf(n)).join(','));
  current[far].reverse();$sortPortsByDummyPositionsInLastLayer(current,parent,forward);
  out.push(parent.ports.array.map(p=>parentPorts.indexOf(p)).join(','));
 }
 return [+bottom,initial.map(p=>parentPorts.indexOf(p)).join(','),...out].join('\t');
}`,context);
const cases=[
 ['through-two',[0,0,1,1,2,2],[1,1,0,0,2,2],[[0,3],[1,2],[2,4],[3,5]],false],
 ['internal-dominated',[0,1,1,1,2,2,2,3,3,1,1,2,2],[1,0,0,0,0,0,0,2,2,0,0,0,0],[[0,1],[1,4],[2,5],[2,6],[3,5],[3,6],[4,7],[5,8],[9,11],[10,12]],false],
 ['one-port-per-side',[0,1,2],[1,0,2],[[0,1],[1,2]],false],
 ['fixed-parent',[0,0,1,1,2,2],[1,1,0,0,2,2],[[0,3],[1,2],[2,4],[3,5]],true],
 ['only-output-boundaries',[0,0,1,1],[0,0,2,2],[[0,3],[1,2]],false],
 ['only-input-boundaries',[0,0,1,1],[1,1,0,0],[[0,3],[1,2]],false],
];
const rows=['# ELK hierarchical decider/transfer; name,layers,roles,edges,fixed,bottom-up,parent-order,enter-forward,near-forward,parent-forward,enter-backward,near-backward,parent-backward.'];
for(const [name,layers,roles,edges,fixed]of cases)rows.push([name,layers.join(','),roles.join(','),edges.map(e=>e.join(':')).join(','),+fixed,original(layers,roles,edges,fixed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-hierarchy-order.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK hierarchy ordering: ${cases.length} original decider and bidirectional transfer cases checked.`);
