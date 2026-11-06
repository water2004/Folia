// Original compound postprocessing on fixed routed segment geometry.
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
const original=vm.runInContext(`(parents,groups,endpoints,unnecessary) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_PortConstraints();$clinit_Direction_0();$clinit_EdgeLabelPlacement();$clinit_LNode$NodeType();
 const graphs=[new LGraph()],nodes=[],ports=[],groupGraphs=new Map();groups.forEach(n=>{groupGraphs.set(n,graphs.length);graphs.push(new LGraph());});
 graphs.forEach((g,i)=>{$setProperty_0(g,DIRECTION,RIGHT_6);$setProperty_0(g,MERGE_HIERARCHY_EDGES_0,true);$setProperty_0(g,UNNECESSARY_BENDPOINTS_0,unnecessary);
  g.padding.left=10+i;g.padding.top_0=20+i;g.offset.x_0=i*2;g.offset.y_0=-i;});
 parents.forEach((p,i)=>{const g=graphs[p<0?0:groupGraphs.get(p)],n=new LNode(g);n.type_0=NORMAL;nodes.push(n);$add_3(g.layerlessNodes,n);$setProperty_0(n,PORT_CONSTRAINTS_0,FREE);});
 groups.forEach(n=>{nodes[n].nestedGraph=graphs[groupGraphs.get(n)];nodes[n].nestedGraph.parentNode=nodes[n];});
 const edges=endpoints.map(([s,t],i)=>{const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);$setSide(a,EAST_2);$setSide(b,WEST_2);ports.push(a,b);$setSource_0(e,a);$setTarget_0(e,b);
  [CENTER_5,TAIL,HEAD].forEach((placement,j)=>{const l=new LLabel();l.testId=i*3+j;$setProperty_0(l,EDGE_LABELS_PLACEMENT,placement);$add_3(e.labels,l);});return e;});
 const setNode=$setNode;$setNode=(p,n)=>{if(!nodes.includes(n))nodes.push(n);if(!ports.includes(p))ports.push(p);setNode(p,n);};
 try{$process_1(new CompoundGraphPreprocessor(),graphs[0],new BasicProgressMonitor());}finally{$setNode=setNode;}
 nodes.forEach((n,i)=>{n.pos.x_0=i*30;n.pos.y_0=i*20;});ports.forEach((p,i)=>{p.pos.x_0=i*3;p.pos.y_0=i*2;});
 const cross=$getProperty(graphs[0],CROSS_HIERARCHY_MAP);
 edges.forEach((e,i)=>{let segments=[];if(e.source)segments=[e];else {const it=$get(cross,e).iterator_0();while(it.hasNext_0())segments.push(it.next_1().newEdge);}
  for(const s of segments){const p=ports.indexOf(s.source),q=ports.indexOf(s.target);if(i%2===0){$add_7(s.bendPoints,new KVector_1(100+p,200+q));$add_7(s.bendPoints,new KVector_1(120+p,220+q));}
   for(const l of s.labels.array){l.pos.x_0=7+l.testId;l.pos.y_0=11+l.testId;}}
 });
 $clinit_CompoundGraphPostprocessor();$process_0(graphs[0],new BasicProgressMonitor());
 const point=p=>p.x_0+','+p.y_0;
 return edges.map(e=>{const ref=isDescendant(e.target.owner,e.source.owner)?e.source.owner.nestedGraph:$getGraph(e.source.owner);
  const start=$getAbsoluteAnchor(e.source),end=$getAbsoluteAnchor(e.target);changeCoordSystem(start,$getGraph(e.source.owner),ref);changeCoordSystem(end,$getGraph(e.target.owner),ref);
  const bends=[];for(let p=e.bendPoints.header.next_0;p!==e.bendPoints.tail;p=p.next_0)bends.push(point(p.value_0));
  return [graphs.indexOf(ref),ports.indexOf(e.source),ports.indexOf(e.target),point(start),point(end),point($getProperty(e,TARGET_OFFSET)||new KVector()),bends.join(':')||'-',
   [...e.labels.array].sort((a,b)=>a.testId-b.testId).map(l=>l.testId+','+point(l.pos)).join(':')].join('|');}).join(';');
}`,context);
const cases=[
 ['siblings',[-1,-1,0,1],[0,1],[[2,3]],true],
 ['nested-return',[-1,0,1,-1],[0,1],[[2,3],[3,2]],true],
 ['group-endpoints',[-1,0,0,-1],[0],[[0,1],[2,0],[0,3]],true],
 ['minimal-bends',[-1,-1,0,1],[0,1],[[2,3],[3,2]],false],
];
const rows=['# Compound restoration; name,parents,groups,endpoints,unnecessary,edge(reference/sourcePort/targetPort/start/end/targetOffset/bends/labels).'];
for(const [name,parents,groups,edges,extra]of cases)rows.push([name,parents.join(','),groups.join(','),edges.map(e=>e.join(':')).join(','),+extra,original(parents,groups,edges,extra)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-compound-restore.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK compound restoration: ${cases.length} original coordinate/path/label cases checked.`);
