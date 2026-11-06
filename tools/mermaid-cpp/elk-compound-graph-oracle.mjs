// Exact CompoundGraphPreprocessor topology and label segment ownership.
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
const original=vm.runInContext(`(parents,groups,directions,endpoints) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_PortConstraints();$clinit_Direction_0();$clinit_EdgeLabelPlacement();
 const graphs=[new LGraph()],nodes=[],ports=[],edgeId=new Property('test.edge');
 const groupGraphs=new Map();groups.forEach((n,i)=>{groupGraphs.set(n,graphs.length);graphs.push(new LGraph());});
 graphs.forEach((g,i)=>{$setProperty_0(g,DIRECTION,[RIGHT_6,LEFT_6,DOWN_1,UP_1][directions[i]]);$setProperty_0(g,MERGE_HIERARCHY_EDGES_0,true);});
 parents.forEach((p,i)=>{const g=graphs[p<0?0:groupGraphs.get(p)];const n=new LNode(g);n.type_0=($clinit_LNode$NodeType(),NORMAL);nodes.push(n);$add_3(g.layerlessNodes,n);$setProperty_0(n,PORT_CONSTRAINTS_0,FREE);});
 groups.forEach(n=>{nodes[n].nestedGraph=graphs[groupGraphs.get(n)];nodes[n].nestedGraph.parentNode=nodes[n];});
 const edges=endpoints.map(([s,t],i)=>{const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);
  $setSide(a,fromDirection($getProperty($getGraph(nodes[s]),DIRECTION)));$setSide(b,$opposed(fromDirection($getProperty($getGraph(nodes[t]),DIRECTION))));
  ports.push(a,b);$setSource_0(e,a);$setTarget_0(e,b);$setProperty_0(e,edgeId,i);
  for(const placement of [CENTER_5,TAIL,HEAD]){const l=new LLabel();$setProperty_0(l,EDGE_LABELS_PLACEMENT,placement);$add_3(e.labels,l);}return e;});
 const setNode=$setNode;$setNode=(p,n)=>{if(!nodes.includes(n))nodes.push(n);if(!ports.includes(p))ports.push(p);setNode(p,n);};
 try{$process_1(new CompoundGraphPreprocessor(),graphs[0],new BasicProgressMonitor());}finally{$setNode=setNode;}
 const key=(e,g)=>[g,ports.indexOf(e.source),ports.indexOf(e.target)].join(',');
 const cross=$getProperty(graphs[0],CROSS_HIERARCHY_MAP),chains=[],labelOwners=[];
 edges.forEach((edge,i)=>{let chain=[];if(edge.source)chain=[{newEdge:edge,graph_0:$getGraph(edge.source.owner)}];
  else {const it=$get(cross,edge).iterator_0();while(it.hasNext_0())chain.push(it.next_1());chain.sort((a,b)=>$compare_9(new CrossHierarchyEdgeComparator(graphs[0]),a,b));}
  chains.push(chain.map(c=>key(c.newEdge,graphs.indexOf(c.graph_0))).join(':'));
  labelOwners.push([CENTER_5,TAIL,HEAD].map(placement=>{const c=chain.find(c=>c.newEdge.labels.array.some(l=>$getProperty(l,EDGE_LABELS_PLACEMENT)===placement));return key(c.newEdge,graphs.indexOf(c.graph_0));}).join(':'));
 });
 return [graphs.map(g=>g.layerlessNodes.array.map(n=>nodes.indexOf(n)).join(',')||'-').join(';'),
  ports.map(p=>[nodes.indexOf(p.owner),p.side.ordinal].join(',')).join(';'),
  nodes.map(n=>{const origin=$getProperty(n,ORIGIN_0);return [graphs.indexOf($getGraph(n)),n.nestedGraph?graphs.indexOf(n.nestedGraph):-1,ports.includes(origin)?ports.indexOf(origin):-1,n.type_0===EXTERNAL_PORT?$getProperty(n,EXT_PORT_SIDE).ordinal:0].join(',');}).join(';'),
  chains.join(';'),labelOwners.join(';')].join('\t');
}`,context);
const cases=[
 ['group-local',[-1,0,0],[0],[0,0],[[1,2]]],
 ['outgoing',[-1,0,-1],[0],[0,0],[[1,2]]],
 ['incoming',[-1,0,-1],[0],[0,0],[[2,1]]],
 ['siblings',[-1,-1,0,1],[0,1],[0,0,0],[[2,3]]],
 ['nested-cross',[-1,0,1,-1],[0,1],[0,0,0],[[2,3],[3,2]]],
 ['mixed-direction',[-1,-1,0,1],[0,1],[2,0,3],[[2,3],[3,2]]],
 ['group-endpoints',[-1,0,0,-1],[0],[0,0],[[0,1],[2,0],[0,3]]],
];
const rows=['# ELK compound topology; name,parents,groups,directions(root then groups),endpoints,graph node order,ports(node/side),nodes(graph/nested/boundary-port/side),edge chains(graph/source-port/target-port),CENTER/TAIL/HEAD segment owners.'];
for(const [name,parents,groups,dirs,edges]of cases)rows.push([name,parents.join(','),groups.join(','),dirs.join(','),edges.map(e=>e.join(':')).join(','),original(parents,groups,dirs,edges)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-compound-graph.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK compound graph: ${cases.length} original hierarchy/segment/label observations checked.`);
