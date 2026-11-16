// Complete original ELK component layout with preserved node model identity.
// Explicit boxes isolate algorithms; these are not font/SVG measurements.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';

const require=createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url)));
const base=path.dirname(require.resolve('elkjs/package.json'));
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
for(const file of lock.elkCycle.files)
  assert.equal(createHash('sha256').update(readFileSync(path.join(base,file.path))).digest('hex'),file.sha256,file.path);
const ELK=require('elkjs/lib/elk.bundled.js'),elk=new ELK();
const options={
  'elk.algorithm':'layered','elk.hierarchyHandling':'SEPARATE_CHILDREN','elk.direction':'RIGHT',
  'elk.layered.cycleBreaking.strategy':'DEPTH_FIRST',
  'elk.layered.nodePlacement.strategy':'BRANDES_KOEPF',
  'elk.layered.nodePlacement.bk.fixedAlignment':'BALANCED',
  'elk.layered.considerModelOrder.strategy':'NODES_AND_EDGES',
  'elk.layered.crossingMinimization.greedySwitch.type':'TWO_SIDED',
  'elk.layered.crossingMinimization.greedySwitch.activationThreshold':40,
  'elk.layered.unnecessaryBendpoints':true,
  'elk.spacing.portsSurrounding':'[top=12,left=12,bottom=12,right=12]',
  'elk.spacing.componentComponent':24,'elk.aspectRatio':Math.fround(1.6),
  'elk.padding':'[top=12,left=12,bottom=12,right=12]'
};
// Record the actual original-engine configuration at the native boundary.
// The native processor decides activation before component splitting.
const layoutMode = {
  hierarchicalLayout: options['elk.hierarchyHandling'] === 'INCLUDE_CHILDREN',
  twoSidedGreedySwitch: options['elk.layered.crossingMinimization.greedySwitch.type'] === 'TWO_SIDED',
  greedySwitchActivationThreshold: options['elk.layered.crossingMinimization.greedySwitch.activationThreshold']
};
const endpoints=[[0,3],[2,3],[1,3]];
const official=await elk.layout({id:'root',layoutOptions:options,
  children:Array.from({length:5},(_,n)=>({id:'n'+n,width:60,height:35})),
  edges:endpoints.map(([source,target],e)=>({id:'e'+e,sources:['n'+source],targets:['n'+target]}))});
const point=p=>[p.x,p.y];
const rows=['# ELK complete component layout; name, UTF-16 JSON input, UTF-16 JSON expected.',
  '# Preserve the original MODEL_ORDER while DFS reorders storage. Full original engine; fixed measured boxes.'];
const hex=value=>Array.from(JSON.stringify(value),c=>c.charCodeAt(0).toString(16).padStart(4,'0')).join('');
for(const [name,order,explicit]of [
  ['declared-model-order',[0,1,2,3,4],false],
  ['dfs-storage-preserves-model-order',[0,3,2,1,4],true]
]){
  const local=new Map(order.map((original,index)=>[original,index]));
  const input={nodes:order.map(()=>[60,35]),edges:endpoints.map(([a,b])=>[local.get(a),local.get(b)]),
    nodeModelOrder:explicit?order:[],options:{padding:12,componentSpacing:24,aspectRatio:Math.fround(1.6),
      considerModelOrder:true,direction:'RIGHT',...layoutMode}};
  const expected={bounds:[official.width,official.height],nodes:order.map(index=>{
    const node=official.children.find(node=>node.id==='n'+index);
    return[node.x,node.y,node.width,node.height];
  }),routes:official.edges.map((edge,e)=>{
    assert.equal(edge.sections.length,1);const section=edge.sections[0];
    return{edge:e,source:input.edges[e][0],target:input.edges[e][1],
      points:[point(section.startPoint),...(section.bendPoints??[]).map(point),point(section.endPoint)]};
  }),centers:[],ends:[]};
  rows.push([name,hex(input),hex(expected)].join('\t'));
}

async function observeComponents(name, nodes, edges) {
  const input = {nodes, edges, nodeModelOrder: [], options: {padding: 12, componentSpacing: 24,
    aspectRatio: Math.fround(1.6), considerModelOrder: false, direction: 'RIGHT', ...layoutMode}};
  const graph = await elk.layout({id: 'root', layoutOptions: {...options, 'elk.layered.considerModelOrder.strategy': 'NONE'},
    children: nodes.map(([width, height], n) => ({id: 'n' + n, width, height})),
    edges: edges.map(([source, target], e) => ({id: 'e' + e, sources: ['n' + source], targets: ['n' + target]}))});
  const expected = {bounds: [graph.width, graph.height], nodes: nodes.map((_, n) => {
    const node = graph.children.find(node => node.id === 'n' + n);
    return [node.x, node.y, node.width, node.height];
  }), routes: graph.edges.map((edge, e) => {
    assert.equal(edge.sections.length, 1);
    const section = edge.sections[0];
    return {edge: e, source: edges[e][0], target: edges[e][1],
      points: [point(section.startPoint), ...(section.bendPoints ?? []).map(point), point(section.endPoint)]};
  }), centers: [], ends: []};
  rows.push([name, hex(input), hex(expected)].join('\t'));
}
const diamond = offset => [[0, 1], [0, 2], [1, 3], [2, 3]].map(pair => pair.map(n => n + offset));
const p = [[74, 38], [108, 46], [86, 58], [96, 42]];
const q = [[162, 28], [68, 74], [130, 40], [84, 92]];
await observeComponents('isolated-before-nontrivial-component', [[43, 24], ...p], diamond(1));
await observeComponents('isolated-between-two-different-diamonds', [...p, [43, 24], ...q], [...diamond(0), ...diamond(5)]);
// The default activation threshold is evaluated on the original graph before
// ComponentsProcessor splits it. Every diamond remains below forty nodes.
for (const count of [39, 40, 41]) {
  const nodes = [[43, 24], ...p, [51, 32], ...q];
  while (nodes.length < count) nodes.push([27 + nodes.length % 7 * 9, 19 + nodes.length % 5 * 8]);
  await observeComponents(`greedy-threshold-before-splitting-${count}`, nodes, [...diamond(1), ...diamond(6)]);
}
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-components-layout.tsv',import.meta.url);
const expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log('ELK components: 7 complete original layouts, including model identity, isolated-node RNG and threshold 39/40/41.');
