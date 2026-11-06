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
 const setNode=$setNode;$setNode=(p,n)=>{if(!nodes.includes(n))nodes.push(n);if(!ports.includes(p))ports.push(p);setNode(p,n);};
 try{$process_38(graph,new BasicProgressMonitor());}finally{$setNode=setNode;}
 const order=$toNodeArray(graph);order.forEach((layer,l)=>{layer.forEach((n,i)=>{n.layer.id_0=l;n.id_0=i;});});
 const resolver=new ForsterConstraintResolver(order),counter=new AllCrossingsCounter(order),initial=new ArrayList;
 $add_3(initial,resolver);$add_3(initial,counter);init_0(initial,order);
 const bary=nodes.map((_,i)=>(i*7%13)/2);
 nodes.forEach((n,i)=>{const state=resolver.barycenterStates[n.layer.id_0][n.id_0];state.barycenter=bary[i];state.summedWeight=bary[i];state.degree=1;});
 const result=[],crossings=[];
 for(const layer of order) {
  const sorted=layer.slice().sort((a,b)=>bary[nodes.indexOf(a)]-bary[nodes.indexOf(b)]),list=newArrayList_1(sorted);
  $processConstraints(resolver,list);result.push(list.array.map(n=>nodes.indexOf(n)).join(','));
  crossings.push($countNorthSouthPortCrossingsInLayer(counter.crossingCounter,list.array));
 }
 return [bary.join(','),result.join(';'),nodes.map(n=>resolver.barycenterStates[n.layer.id_0][n.id_0].barycenter).join(','),crossings.join(',')].join('\t');
}`,context);
const cases=[
 ['north-in-out',[0,1,2],[[0,1,2,1],[1,2,1,4]],[]],
 ['south-in-out',[0,1,2],[[0,1,2,3],[1,2,3,4]],[]],
 ['mixed-fan',[0,0,1,2,2],[[0,2,2,1],[1,2,2,3],[2,3,3,4],[2,4,1,4]],[]],
 ['fixed-north',[0,0,1,2,2],[[2,3,1,4],[0,2,2,1],[2,4,1,4],[1,2,2,1]],[2]],
];
const rows=['# ELK N/S Forster resolver and additional crossings; name,layers,edges,fixed,barycenters,order,resolved barycenters,NS crossings per layer.'];
for(const [name,layers,edges,fixed]of cases)rows.push([name,layers.join(','),edges.map(e=>e.join(':')).join(','),fixed.join(',')||'-',original(layers,edges,fixed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-order-constraints.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK N/S Forster constraints/count: ${cases.length} original pre/post cases checked.`);
