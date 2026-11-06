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
 const order=()=>graph.layers.array.map(l=>l.nodes.array.map(n=>nodes.indexOf(n)).join(',')).join(';');
 const endpoint=()=>edges.map(e=>ports.indexOf(e.source)+':'+ports.indexOf(e.target)).join(',');
 const list=(n,key)=>($getProperty(n,key)?.array??[]).map(v=>nodes.indexOf(v)).join(',')||'-';
 const pre=[order(),nodes.map(n=>n.ports.array.map(p=>ports.indexOf(p)).join(',')||'-').join(';'),endpoint(),
  nodes.map(n=>list(n,BARYCENTER_ASSOCIATES)).join(';'),nodes.map(n=>list(n,IN_LAYER_SUCCESSOR_CONSTRAINTS)).join(';'),
  nodes.map(n=>nodes.indexOf($getProperty(n,IN_LAYER_LAYOUT_UNIT))).join(',')];
 const dummy=nodes.slice(assignment.length).map(n=>{const p=n.ports.array[0],origin=$getProperty(p,ORIGIN_0);return [nodes.indexOf(n),nodes.indexOf(origin.owner),ports.indexOf(origin),ports.indexOf(p),origin.side.ordinal].join(':');}).join(',');
 nodes.forEach((n,i)=>{n.pos.x_0=i*17+3;n.pos.y_0=i*13+7;});ports.forEach((p,i)=>{p.pos.x_0=i*3+2;p.pos.y_0=i+1;});
 edges.forEach((e,i)=>$add_7(e.bendPoints,new KVector_1(100+i,200+i)));
 $process_37(graph,new BasicProgressMonitor());
 const paths=edges.map(e=>{const values=[];for(let it=$listIterator_2(e.bendPoints,0);it.currentNode!==it.this$01.tail;){const p=$next_9(it);values.push(p.x_0+','+p.y_0);}return values.join(':');}).join(';');
 return [...pre,dummy,order(),endpoint(),paths].join('\t');
}`,context);
const cases=[
 ['north-in-out',[0,1,2],[[0,1,2,1],[1,2,1,4]],[]],
 ['south-in-out',[0,1,2],[[0,1,2,3],[1,2,3,4]],[]],
 ['mixed-fan',[0,0,1,2,2],[[0,2,2,1],[1,2,2,3],[2,3,3,4],[2,4,1,4]],[]],
 ['fixed-north',[0,0,1,2,2],[[2,3,1,4],[0,2,2,1],[2,4,1,4],[1,2,2,1]],[2]],
];
const rows=['# ELK N/S implicit ports; name,layers,edges(s/t/s-side/t-side),fixed,pre layer/ports/endpoints/associates/successors/units,dummies,post layers/endpoints/bends.'];
for(const [name,layers,edges,fixed]of cases)rows.push([name,layers.join(','),edges.map(e=>e.join(':')).join(','),fixed.join(',')||'-',original(layers,edges,fixed)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-north-south.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK north/south ports: ${cases.length} original pre/post cases checked.`);
