// Execute the pinned ComponentsProcessor and SimpleRowGraphPlacer directly.
// These are graph topology/packing observations, not font measurements or SVGs.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync, writeFileSync, realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

const require = createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json', import.meta.url)));
const base = path.dirname(require.resolve('elkjs/package.json'));
const lock = JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json', import.meta.url)));
for (const file of lock.elkCycle.files)
  assert.equal(createHash('sha256').update(readFileSync(path.join(base, file.path))).digest('hex'), file.sha256, file.path);
const context = vm.createContext({module: {exports: {}}, exports: {}, setTimeout, clearTimeout});
context.global = context;
vm.runInContext(readFileSync(path.join(base, 'lib/elk-worker.js'), 'utf8'), context);
const split = vm.runInContext(`(nodeCount, endpoints) => {
  $clinit_LayeredOptions(); $clinit_InternalProperties_1(); $clinit_LNode$NodeType();
  const graph = new LGraph(), nodes = Array.from({length: nodeCount}, () => {
    const node = new LNode(graph); node.type_0 = NORMAL;
    $add_3(graph.layerlessNodes, node); return node;
  });
  endpoints.forEach(([source, target]) => {
    const a = new LPort(), b = new LPort(), edge = new LEdge();
    $setNode(a, nodes[source]); $setNode(b, nodes[target]);
    $setSource_0(edge, a); $setTarget_0(edge, b);
  });
  return $split_2(new ComponentsProcessor_0(), graph).array.map(component => {
    const members = component.layerlessNodes.array.map(node => nodes.indexOf(node));
    return {nodes: members, edges: endpoints.flatMap(([source], edge) => members.includes(source) ? [edge] : [])};
  });
}`, context);
const pack = vm.runInContext(`(sizes, spacing, aspect) => {
  $clinit_LayeredOptions();
  const graphs = sizes.map(([width, height]) => {
    const graph = new LGraph(); graph.size_0.x_0 = width; graph.size_0.y_0 = height;
    $setProperty_0(graph, SPACING_COMPONENT_COMPONENT_0, spacing);
    $setProperty_0(graph, ASPECT_RATIO_1, aspect);
    return graph;
  });
  const components = new ArrayList(); graphs.forEach(graph => $add_3(components, graph));
  const target = new LGraph(), offsets = graphs.map(() => [0, 0]);
  const offsetGraph = $offsetGraph;
  $offsetGraph = (graph, x, y) => { offsets[graphs.indexOf(graph)] = [x, y]; offsetGraph(graph, x, y); };
  try { new SimpleRowGraphPlacer().combine(components, target); }
  finally { $offsetGraph = offsetGraph; }
  return {order: components.array.map(graph => graphs.indexOf(graph)), offsets,
    width: target.size_0.x_0, height: target.size_0.y_0};
}`, context);

const splitCases = [
  ['empty', 0, []],
  ['isolated', 4, []],
  ['out-of-order-chain', 5, [[0, 2], [2, 4], [4, 1]]],
  ['two-diamonds-and-isolate', 9, [[0, 1], [0, 2], [1, 3], [2, 3], [4, 5], [4, 6], [5, 7], [6, 7]]],
  ['incoming-weak-connectivity', 6, [[2, 0], [1, 2], [3, 1], [4, 5]]],
  ['parallel-and-self-edges', 4, [[0, 0], [1, 0], [0, 1], [2, 2]]],
  ['port-insertion-order', 4, [[0, 2], [0, 1], [1, 3], [2, 3]]],
];
const packingCases = [
  ['empty', []],
  ['single', [[31, 17]]],
  ['directional-scope-diamonds', [[508, 140.5], [588, 154.5], [43, 24]]],
  ['equal-area-stability', [[20, 10], [10, 20], [40, 5], [5, 40]]],
  ['mixed-aspect', [[220, 12], [18, 180], [80, 60], [30, 20], [170, 110]]],
  ['zero-size-components', [[0, 0], [0, 0], [0, 0]]],
  ['custom-spacing-and-aspect', [[120, 35], [25, 80], [45, 45], [200, 20]], 12, 1.25],
];
const hex = value => Array.from(JSON.stringify(value), character => character.charCodeAt(0).toString(16).padStart(4, '0')).join('');
const rows = ['# ELK 0.9.3 original components/row packing; name, kind, input JSON UTF-16 hex, observation JSON UTF-16 hex.'];
for (const [name, nodes, edges] of splitCases)
  rows.push([name, 'split', hex({nodes, edges}), hex(split(nodes, edges))].join('\t'));
for (const [name, sizes, spacing = 24, aspect = Math.fround(1.6)] of packingCases)
  rows.push([name, 'pack', hex({sizes, spacing, aspect}), hex(pack(sizes, spacing, aspect))].join('\t'));
const fixture = new URL('../../tests/mermaid_cpp/fixtures/elk-components.tsv', import.meta.url);
const expected = rows.join('\n') + '\n';
if (process.argv.includes('--write')) writeFileSync(fixture, expected);
else assert.equal(readFileSync(fixture, 'utf8').replaceAll('\r\n', '\n'), expected);
console.log(`ELK components: ${splitCases.length} original DFS partitions and ${packingCases.length} original row packings checked.`);
