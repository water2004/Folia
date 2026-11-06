// Original SMART_DOWN, EndLabelPreprocessor and EndLabelPostprocessor.
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
const original=vm.runInContext(`(count,endpoints,measurements,reversed,vertical) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_LNode$NodeType();
 $clinit_Direction_0();$clinit_EdgeLabelPlacement();
 const graph=new LGraph(),layer=new Layer(graph);$add_3(graph.layers,layer);
 $setProperty_0(graph,DIRECTION,vertical?DOWN_1:RIGHT_6);
 $setProperty_0(graph,SPACING_EDGE_LABEL_0,2);$setProperty_0(graph,SPACING_LABEL_LABEL,3);
 const nodes=Array.from({length:count},(_,i)=>{const n=new LNode(graph);n.type_0=NORMAL;$setLayer_0(n,layer);
  n.size_0.x_0=80+i*5;n.size_0.y_0=60;n.margin.left=1;n.margin.right=2;n.margin.top_0=3;n.margin.bottom=4;
  n.pos.x_0=i*150;n.pos.y_0=i*20;return n;});
 const edges=endpoints.map(([s,t])=>{const a=new LPort(),b=new LPort(),e=new LEdge();
  $setNode(a,nodes[s]);$setNode(b,nodes[t]);$setSource_0(e,a);$setTarget_0(e,b);return e;});
 const labels=measurements.map(([e,head,w,h])=>{const l=new LLabel();l.size_0.x_0=w;l.size_0.y_0=h;
  $setProperty_0(l,EDGE_LABELS_PLACEMENT,head?HEAD:TAIL);$add_3(edges[e].labels,l);return l;});
 reversed.forEach(e=>$reverse_0(edges[e],false));$process_43(graph,new BasicProgressMonitor());
 nodes.forEach(n=>{n.ports.array.sort((a,b)=>a.side.ordinal-b.side.ordinal);
  n.ports.array.forEach((p,i)=>{p.pos.x_0=p.side===EAST_2?n.size_0.x_0:0;p.pos.y_0=12+i*8;});});
 $process_30(graph,new BasicProgressMonitor());$process_9(graph,new BasicProgressMonitor());
 const margins=nodes.map(n=>[n.margin.left,n.margin.right,n.margin.top_0,n.margin.bottom].join(',')).join(';');
 $process_8(graph,new BasicProgressMonitor());
 return [margins,labels.map(l=>[l.pos.x_0,l.pos.y_0].join(',')).join(';')].join('\t');
}`,context);
const cases=[
 ['both-ends',2,[[0,1]],[[0,0,28,14],[0,1,18,12]],[],false],
 ['two-on-end',2,[[0,1]],[[0,0,28,14],[0,0,18,12]],[],false],
 ['smart-two',3,[[0,1],[0,2]],[[0,0,28,14],[1,0,18,12],[0,1,24,14]],[],false],
 ['smart-empty-port',3,[[0,1],[0,2]],[[0,0,28,14]],[],false],
 ['west-smart',3,[[0,2],[1,2]],[[0,1,28,14],[1,1,18,12]],[],false],
 ['vertical-multiple',2,[[0,1]],[[0,0,28,14],[0,0,18,12],[0,1,22,12]],[],true],
 ['vertical-overlap',4,[[0,1],[0,2],[0,3]],[[0,0,28,20],[1,0,18,18],[2,0,24,22]],[],true],
 ['feedback',2,[[1,0]],[[0,0,28,14],[0,1,18,12]],[0],false],
];
const rows=['# ELK end labels; name,count,endpoints,labels(edge,head,width,height),reversed,vertical,margins(left,right,top,bottom),labelXY.'];
for(const [name,count,edges,labels,reversed,vertical]of cases)
 rows.push([name,count,edges.map(e=>e.join(':')).join(','),labels.map(l=>l.join(':')).join(','),reversed.join(',')||'-',+vertical,original(count,edges,labels,reversed,vertical)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-end-labels.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK end labels: ${cases.length} original side/margin/geometry observations checked.`);
