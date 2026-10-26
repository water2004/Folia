// Original separate-layer constraint pre/postprocessing around NetworkSimplex.
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
const original=vm.runInContext(`(roles,endpoints) => {
 $clinit_LayeredOptions();$clinit_LNode$NodeType();$clinit_LayerConstraint();$clinit_LayerConstraintPreprocessor();
 const graph=new LGraph(),nodes=roles.map(role=>{const n=new LNode(graph);n.type_0=role?EXTERNAL_PORT:NORMAL;
  if(role)$setProperty_0(n,LAYERING_LAYER_CONSTRAINT_0,role===1?FIRST_SEPARATE_0:LAST_SEPARATE_0);$add_3(graph.layerlessNodes,n);return n;});
 const edges=endpoints.map(([s,t])=>{const a=new LPort(),b=new LPort(),e=new LEdge();$setNode(a,nodes[s]);$setNode(b,nodes[t]);$setSource_0(e,a);$setTarget_0(e,b);return e;});
 const reversed=[],reverse=$reverse_0;$reverse_0=(e,a)=>{reversed.push(edges.indexOf(e));reverse(e,a);};
 try{$process_57(new DepthFirstCycleBreaker(),graph,new BasicProgressMonitor());}finally{$reverse_0=reverse;}
 $process_32(graph,new BasicProgressMonitor());$process_68(new NetworkSimplexLayerer(),graph,new BasicProgressMonitor());$process_31(graph,new BasicProgressMonitor());
 return [reversed.join(',')||'-',nodes.map(n=>graph.layers.array.indexOf(n.layer)).join(','),graph.layers.array.map(l=>l.nodes.array.map(n=>nodes.indexOf(n)).join(',')).join(';')].join('\t');
}`,context);
const cases=[
 ['through-chain',[0,0,1,2],[[2,0],[0,1],[1,3]]],
 ['multiple-boundaries',[0,0,1,1,2,2],[[2,0],[3,1],[0,1],[0,4],[1,5]]],
 ['isolated-inferred-first',[0,0,0,1],[[0,1],[3,2]]],
 ['isolated-inferred-last',[0,0,0,2],[[0,1],[2,3]]],
 ['isolated-between',[0,0,0,1,2],[[0,1],[3,2],[2,4]]],
 ['boundary-only',[1,2],[[0,1]]],
 ['inner-feedback',[0,0,0,1,2],[[3,0],[0,1],[1,2],[2,0],[2,4]]],
];
const rows=['# ELK separate boundary layering; name,roles(0 normal/1 first separate/2 last separate),endpoints,reversed,layers,node order.'];
for(const [name,roles,edges]of cases)rows.push([name,roles.join(','),edges.map(e=>e.join(':')).join(','),original(roles,edges)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-boundary-layers.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK boundary layering: ${cases.length} original constraint/network-simplex observations checked.`);
