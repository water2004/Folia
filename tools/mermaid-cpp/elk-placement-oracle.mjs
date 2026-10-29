// Original ELK BK conflict marking, four-way alignment and inside-block shifts.
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
const original=vm.runInContext(`(assignment, endpoints, dummies) => {
  $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_LNode$NodeType();
  const graph=new LGraph(), nodes=assignment.map(()=>new LNode(graph));
  for(let l=0;l<=Math.max(...assignment);++l)$add_3(graph.layers,new Layer(graph));
  nodes.forEach((n,i)=>{
    $setLayer_0(n,graph.layers.array[assignment[i]]);
    n.type_0=dummies.includes(i)?LONG_EDGE:NORMAL;
    n.size_0.y_0=dummies.includes(i)?1:24+i*3;
    n.margin.top_0=i%3;n.margin.bottom=(i+1)%4;
  });
  const edges=endpoints.map(([s,t],i)=>{
    const a=new LPort(),b=new LPort(),e=new LEdge();
    $setNode(a,nodes[s]);$setNode(b,nodes[t]);a.side=EAST_2;b.side=WEST_2;
    a.pos.y_0=(i*7)%23;b.pos.y_0=(i*11)%19;
    $setSource_0(e,a);$setTarget_0(e,b);return e;
  });
  const ni=buildFor(graph),placer=new BKNodePlacer();placer.ni=ni;
  $markConflicts(placer,graph);
  const marked=edges.map(e=>$contains_6(placer.markedEdges,e)?1:0).join(',')||'-';
  $clinit_BKAlignedLayout$VDirection();$clinit_BKAlignedLayout$HDirection();
  const layouts=[[RIGHT_3,DOWN_0],[RIGHT_3,UP_0],[LEFT_3,DOWN_0],[LEFT_3,UP_0]].map(([h,v])=>{
    const b=new BKAlignedLayout(graph,nodes.length,v,h);
    $verticalAlignment(new BKAligner(graph,ni),b,placer.markedEdges);$insideBlockShift(b);
    return nodes.map(n=>[nodes.indexOf(b.root[n.id_0]),nodes.indexOf(b.align_0[n.id_0]),
      b.innerShift[n.id_0],b.blockSize[n.id_0]??0,b.od[n.id_0]?1:0].join(',')).join(';');
  });
  $setProperty_0(graph,SPACINGS,new Spacings(graph));
  const positions=[true,false].map(favor=>{
    $setProperty_0(graph,NODE_PLACEMENT_FAVOR_STRAIGHT_EDGES_0,favor);
    $process_77(new BKNodePlacer(),graph,new BasicProgressMonitor());
    return nodes.map(n=>n.pos.y_0).join(',');
  });
  return [marked,...layouts,...positions].join('\t');
}`,context);
const cases=[
 ['single',[0],[],[]],
 ['unequal-chain',[0,1,2],[[0,1],[1,2]],[]],
 ['diamond',[0,1,1,2],[[0,1],[0,2],[1,3],[2,3]],[]],
 ['crossed',[0,0,1,1],[[0,3],[1,2]],[]],
 ['parallel-median',[0,0,1,1],[[0,2],[0,3],[0,3],[1,3]],[]],
 ['inner-conflict',[0,1,1,2,2,3],[[0,1],[1,4],[2,3],[3,5],[4,5]],[1,4]],
 ['disconnected',[0,0,1,1,2],[[0,2],[2,4]],[]],
];
const rows=['# ELK 0.9.3 BK placement; name, layers, endpoints, dummy nodes, marked edges, four layouts (root,align,shift,size,only-dummies), straight/balanced final y.'];
for(const [name,layers,edges,dummies] of cases)
 rows.push([name,layers.join(','),edges.map(e=>e.join(':')).join(',')||'-',dummies.join(',')||'-',
   original(layers,edges,dummies)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-placement.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK BK placement: ${cases.length} original four-way blocks and final coordinates checked.`);
