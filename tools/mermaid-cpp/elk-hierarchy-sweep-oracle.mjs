// Original recursive LayerSweepCrossingMinimizer on prepared compound graphs.
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
const original=vm.runInContext(`(specs,seed) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_LNode$NodeType();$clinit_PortSide();$clinit_PortConstraints();$clinit_GraphProperties();$clinit_HierarchyHandling();
 const all=specs.map(spec=>{
  const graph=new LGraph(),nodes=spec.layers.map(()=>new LNode(graph)),ports=[];
  $setProperty_0(graph,RANDOM_0,new Random_0(fromDouble_0(seed)));$setProperty_0(graph,HIERARCHY_HANDLING,INCLUDE_CHILDREN);
  for(let l=0;l<=Math.max(...spec.layers);++l)$add_3(graph.layers,new Layer(graph));
  nodes.forEach((n,i)=>{$setLayer_0(n,graph.layers.array[spec.layers[i]]);$setProperty_0(n,PORT_CONSTRAINTS_0,FIXED_SIDE);});
  spec.edges.forEach(([s,t])=>{const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);a.side=EAST_2;b.side=WEST_2;$setSource_0(e,a);$setTarget_0(e,b);ports.push(a,b);});
  return {graph,nodes,ports};
 });
 specs.forEach((spec,i)=>{if(spec.parent<0)return;
  const child=all[i],outer=all[spec.parent],parent=outer.nodes[spec.node];parent.nestedGraph=child.graph;child.graph.parentNode=parent;
  if(spec.links.length)$getProperty(child.graph,GRAPH_PROPERTIES).add_2(EXTERNAL_PORTS);
  for(const [n,p]of spec.links){child.nodes[n].type_0=EXTERNAL_PORT;$setProperty_0(child.nodes[n],ORIGIN_0,outer.ports[p]);$setProperty_0(outer.ports[p],PORT_DUMMY,child.nodes[n]);$setProperty_0(outer.ports[p],INSIDE_CONNECTIONS,true);}
 });
 all.forEach(({graph})=>new PortListSorter().process(graph,new BasicProgressMonitor()));
 const processor=new LayerSweepCrossingMinimizer(($clinit_LayerSweepCrossingMinimizer$CrossMinType(),BARYCENTER));
 $process_71(processor,all[0].graph,new BasicProgressMonitor());
 const snapshots=all.map(({graph,nodes,ports})=>graph.layers.array.map(l=>l.nodes.array.map(n=>nodes.indexOf(n)).join(',')).join(';')+'/'+nodes.map(n=>n.ports.array.map(p=>ports.indexOf(p)).join(',')||'-').join(';')).join('|');
 const bottom=all.map(({graph})=>+processor.graphInfoHolders.array.find(h=>h.lGraph===graph).useBottomUp).join(',');
 const total=processor.graphInfoHolders.array.reduce((sum,h)=>sum+$countAllCrossings(h.crossingsCounter,$toNodeArray(h.lGraph)),0);
 return [snapshots,bottom,total,all.map(({graph})=>$nextDouble($getProperty(graph,RANDOM_0))).join(',')].join('\t');
}`,context);
const root={layers:[1,0,0,2,2],edges:[[1,0],[2,0],[0,3],[0,4]],parent:-1,node:0,links:[]};
const child={layers:[0,0,1,1,2,2],edges:[[0,3],[1,2],[2,4],[3,5]],parent:0,node:0,links:[[0,1],[1,3],[4,4],[5,6]]};
const mixed={layers:[0,0,1,1,2,2,1,1,2,2,1,1,2,2],edges:[[0,3],[1,2],[2,4],[3,5],[6,8],[7,9],[10,12],[11,13]],parent:0,node:0,links:child.links};
// Keep all normal interior nodes out of the separate last external layer.
mixed.layers=[0,0,1,1,3,3,1,1,2,2,1,1,2,2];
mixed.edges=[[0,3],[1,2],[2,8],[3,9],[8,4],[9,5],[6,12],[7,13],[10,12],[11,13]];
const nested={layers:[0,1,2],edges:[[0,1],[1,2]],parent:1,node:2,links:[[0,3],[2,4]]};
const bottomUp={...mixed,layers:[...mixed.layers,1,1,1,1,2,2,2,2],edges:[...mixed.edges,[14,18],[15,19],[16,20],[17,21]]};
const cases=[['parent-child',1,[root,child]],['parent-child-seed42',42,[root,child]],['nested',1,[root,child,nested]],['internal-paths',1,[root,mixed]],['bottom-up-child',1,[root,bottomUp]]];
const encode=g=>[g.layers.join(','),g.edges.map(e=>e.join(':')).join(','),g.parent,g.node,g.links.map(l=>l.join(':')).join(',')||'-'].join('@');
const rows=['# ELK recursive hierarchy sweep; name,seed,graphs(layers@edges@parent@node@links),order,bottom-up,crossings,next-random-double.'];
for(const [name,seed,graphs]of cases)rows.push([name,seed,graphs.map(encode).join('|'),original(graphs,seed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-hierarchy-sweep.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK hierarchical sweep: ${cases.length} original recursive graph ordering cases checked.`);
