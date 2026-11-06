// Original fixed-node implicit point-port preparation used by Mermaid's ELK adapter.
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
const original=vm.runInContext(`(width,height,directions,center) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_LNode$NodeType();$clinit_PortConstraints();
 const graph=new LGraph(),layer=new Layer(graph),node=new LNode(graph);$add_3(graph.layers,layer);$setLayer_0(node,layer);
 node.type_0=NORMAL;node.size_0.x_0=width;node.size_0.y_0=height;
 $setProperty_0(node,PORT_CONSTRAINTS_0,FREE);
 const margin=new ElkMargin();margin.top_0=margin.bottom=margin.left=margin.right=12;
 $setProperty_0(graph,SPACING_PORTS_SURROUNDING,margin);
 $clinit_PortAlignment();if(center)$setProperty_0(node,PORT_ALIGNMENT_DEFAULT,CENTER_6);
 const ports=directions.map(direction=>{
  const port=new LPort(),other=new LPort(),edge=new LEdge();$setNode(port,node);$setNode(other,new LNode(graph));
  if(direction==='o'){$setSource_0(edge,port);$setTarget_0(edge,other);}
  else {$setSource_0(edge,other);$setTarget_0(edge,port);}return port;
 });
 $process_43(graph,new BasicProgressMonitor());
 new PortListSorter().process(graph,new BasicProgressMonitor());
 $process_25(graph,new BasicProgressMonitor());
 return [node.ports.array.map(p=>ports.indexOf(p)).join(',')||'-',ports.map(p=>[p.side.ordinal,p.pos.x_0,p.pos.y_0].join(',')).join(';')||'-'].join('\t');
}`,context);
const cases=[
 ['one-port',80,38,'o',false],['both-sides',80,60,'ioioio',false],
 ['three-default',80,38,'ooo',false],['three-center',80,38,'ooo',true],
 ['tall-default',80,120,'iii',false],['tall-center',80,120,'iii',true],
 ['short-default',40,12,'ooo',false],['no-ports',80,38,'',false],
];
const rows=['# ELK fixed-node point ports; name, width, height, port directions, center alignment, sorted port order, side/x/y per original port.'];
for(const [name,width,height,directions,center] of cases)
 rows.push([name,width,height,directions||'-',Number(center),original(width,height,[...directions],center)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-ports.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK ports: ${cases.length} original side/order/point-geometry observations checked.`);
