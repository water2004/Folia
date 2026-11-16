// Original ELK 0.9.3 TWO_SIDED_GREEDY_SWITCH stage observations.
// Inputs are complete layered graphs at the processor boundary. The worker
// performs all sweeps, crossing decisions, port moves and RNG consumption.
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

const observe = vm.runInContext(`json => {
  const spec = JSON.parse(json);
  $clinit_LayeredOptions(); $clinit_InternalProperties_1(); $clinit_PortSide();
  $clinit_PortConstraints(); $clinit_LNode$NodeType(); $clinit_LayerSweepCrossingMinimizer$CrossMinType();
  const graph = new LGraph(), nodes = spec.layers.map(() => new LNode(graph));
  for (let l = 0; l <= Math.max(-1, ...spec.layers); ++l) $add_3(graph.layers, new Layer(graph));
  nodes.forEach((node, n) => {
    $setLayer_0(node, graph.layers.array[spec.layers[n]]);
    $setProperty_0(node, MODEL_ORDER_1, valueOf_3(n));
    $setProperty_0(node, PORT_CONSTRAINTS_0,
      spec.fixed?.includes(n) ? FIXED_POS : spec.fixedOrder?.includes(n) ? FIXED_ORDER : FIXED_SIDE);
    node.size_0.x_0 = spec.boxes?.[n]?.[0] ?? 60;
    node.size_0.y_0 = spec.boxes?.[n]?.[1] ?? 35;
    if (spec.labels?.includes(n)) node.type_0 = LABEL;
  });
  spec.edges.forEach(([source, target], e) => {
    const a = new LPort(), b = new LPort(), edge = new LEdge();
    $setNode(a, nodes[source]); $setNode(b, nodes[target]);
    a.side = spec.sides?.[e]?.[0] === 'W' ? WEST_2 : EAST_2;
    b.side = spec.sides?.[e]?.[1] === 'E' ? EAST_2 : WEST_2;
    a.pos.y_0 = (e * 7) % 23; b.pos.y_0 = (e * 11) % 19;
    $setSource_0(edge, a); $setTarget_0(edge, b);
    $setProperty_0(edge, MODEL_ORDER_1, valueOf_3(e));
  });
  const setLayer = $setLayer_0;
  $setLayer_0 = (node, layer) => { if (!nodes.includes(node)) nodes.push(node); setLayer(node, layer); };
  try {
    $process_35(graph, new BasicProgressMonitor());
    if (spec.sides) $process_24(graph, new BasicProgressMonitor());
  } finally { $setLayer_0 = setLayer; }
  new PortListSorter().process(graph, new BasicProgressMonitor());
  const random = new Random_0(fromDouble_0(spec.seed ?? 1));
  $setProperty_0(graph, RANDOM_0, random);
  if (spec.barycenter) {
    const processor = new LayerSweepCrossingMinimizer(BARYCENTER);
    $process_71(processor, graph, new BasicProgressMonitor());
  }
  if (spec.randomState !== undefined) {
    random.seedhi = Math.floor(spec.randomState / 16777216);
    random.seedlo = spec.randomState % 16777216;
  }
  const state = () => random.seedhi * 16777216 + random.seedlo;
  const ports = nodes.flatMap(node => node.ports.array);
  const edges = [...new Set(ports.flatMap(port => port.outgoingEdges.array))];
  const optionalPort = port => port == null ? null : ports.indexOf(port);
  const kind = node => node.type_0 === NORMAL ? 'normal' : node.type_0 === LONG_EDGE ? 'long_edge' :
    node.type_0 === LABEL ? 'label' : node.type_0 === EXTERNAL_PORT ? 'external_port' : 'north_south';
  const side = port => port.side === EAST_2 ? 'east' : port.side === WEST_2 ? 'west' :
    port.side === NORTH_3 ? 'north' : port.side === SOUTH_2 ? 'south' : 'undefined';
  const order = () => ({layers: graph.layers.array.map(layer => layer.nodes.array.map(node => nodes.indexOf(node))),
    ports: nodes.map(node => node.ports.array.map(port => ports.indexOf(port)))});
  const countCrossings = () => {
    const nodeOrder = $toNodeArray(graph), counter = new AllCrossingsCounter(nodeOrder), initializers = new ArrayList();
    $add_3(initializers, counter); init_0(initializers, nodeOrder);
    return $countAllCrossings(counter, nodeOrder);
  };
  const input = {...order(), randomState: state(), nodes: nodes.map(node => ({
    kind: kind(node), layer: graph.layers.array.indexOf(node.layer), width: node.size_0.x_0, height: node.size_0.y_0,
    fixedPositions: $getProperty(node, PORT_CONSTRAINTS_0) === FIXED_POS,
    fixedOrder: $getProperty(node, PORT_CONSTRAINTS_0) === FIXED_ORDER,
    longSource: optionalPort($getProperty(node, LONG_EDGE_SOURCE)),
    longTarget: optionalPort($getProperty(node, LONG_EDGE_TARGET))
  })), portData: ports.map(port => ({node: nodes.indexOf(port.owner), side: side(port),
    x: port.pos.x_0, y: port.pos.y_0, incoming: port.incomingEdges.array.map(edge => edges.indexOf(edge)),
    outgoing: port.outgoingEdges.array.map(edge => edges.indexOf(edge))})),
    edges: edges.map(edge => ({source: ports.indexOf(edge.source), target: ports.indexOf(edge.target)}))};
  const beforeCrossings = countCrossings();
  const processor = new LayerSweepCrossingMinimizer(TWO_SIDED_GREEDY_SWITCH_0);
  $process_71(processor, graph, new BasicProgressMonitor());
  return JSON.stringify({input, expected: {...order(), randomState: state(), crossings: countCrossings(), beforeCrossings}});
}`, context);

const cases = [
  {name: 'empty-preserves-random-state', layers: [], edges: []},
  {name: 'single-node-preserves-random-state', layers: [0], edges: []},
  {name: 'isolated-layer-keeps-tied-order', layers: [0, 0, 0, 0], edges: []},
  {name: 'two-layer-reversed-matching', layers: [0, 0, 1, 1], edges: [[0, 3], [1, 2]]},
  {name: 'reversed-matching-opposite-initial-sweep', layers: [0, 0, 1, 1], edges: [[0, 3], [1, 2]], seed: 17},
  {name: 'three-layer-opposing-orders', layers: [0, 0, 1, 1, 2, 2], edges: [[0, 3], [1, 2], [2, 5], [3, 4]]},
  {name: 'west-fan-node-and-port-choices', layers: [0, 0, 0, 1], edges: [[0, 3], [1, 3], [2, 3]]},
  {name: 'east-fan-node-and-port-choices', layers: [0, 1, 1, 1], edges: [[0, 3], [0, 2], [0, 1]], seed: 17},
  {name: 'parallel-edges-require-port-improvement', layers: [0, 1],
    edges: [[0, 1], [0, 1], [0, 1], [0, 1], [0, 1]]},
  {name: 'parallel-edges-fixed-source-free-target', layers: [0, 1], fixed: [0],
    edges: [[0, 1], [0, 1], [0, 1], [0, 1], [0, 1]]},
  {name: 'parallel-edges-free-source-fixed-target', layers: [0, 1], fixedOrder: [1], seed: 17,
    edges: [[0, 1], [0, 1], [0, 1], [0, 1], [0, 1]]},
  {name: 'parallel-chain-both-middle-sides', layers: [0, 1, 2],
    edges: [[0, 1], [0, 1], [0, 1], [0, 1], [1, 2], [1, 2], [1, 2]]},
  {name: 'fixed-position-fan-preserves-ports', layers: [0, 0, 0, 1], edges: [[0, 3], [1, 3], [2, 3]], fixed: [3]},
  {name: 'fixed-order-fan-preserves-ports', layers: [0, 1, 1, 1], edges: [[0, 3], [0, 2], [0, 1]], fixedOrder: [0]},
  {name: 'middle-layer-fan-in-and-fan-out', layers: [0, 0, 0, 1, 1, 2, 2, 2],
    edges: [[0, 4], [1, 3], [2, 4], [0, 3], [3, 7], [4, 5], [3, 6], [4, 7]]},
  {name: 'dense-three-layer-bipartite', layers: [0, 0, 0, 1, 1, 1, 1, 2, 2, 2],
    edges: [[0, 6], [0, 4], [1, 5], [1, 3], [2, 4], [2, 6], [0, 3], [2, 5],
      [3, 9], [3, 7], [4, 8], [4, 9], [5, 7], [5, 9], [6, 8], [6, 7]]},
  {name: 'long-edge-dummies-cross-middle-fan', layers: [0, 0, 1, 1, 2, 2, 3],
    edges: [[0, 3], [1, 2], [0, 5], [1, 6], [2, 5], [3, 4], [4, 6], [5, 6]]},
  {name: 'label-dummies-with-crossing-branches', layers: [0, 0, 1, 1, 1, 2, 2], labels: [2, 4],
    edges: [[0, 4], [1, 2], [0, 3], [2, 6], [3, 5], [4, 5]],
    boxes: [[72, 40], [84, 38], [123, 18], [62, 54], [87, 24], [80, 32], [90, 44]]},
  {name: 'inverted-feedback-through-middle-layer', layers: [0, 1, 2],
    edges: [[0, 1], [1, 2], [0, 2]], sides: [['E', 'W'], ['E', 'W'], ['W', 'E']]},
  {name: 'parallel-feedback-and-crossing-branches', layers: [0, 0, 1, 1, 2],
    edges: [[0, 3], [1, 2], [2, 4], [3, 4], [0, 4], [1, 4]],
    sides: [['E', 'W'], ['E', 'W'], ['E', 'W'], ['E', 'W'], ['W', 'E'], ['W', 'E']]},
  {name: 'post-barycenter-diamond-rng-continuity', layers: [0, 1, 1, 2],
    edges: [[0, 1], [0, 2], [1, 3], [2, 3]], barycenter: true},
  {name: 'post-barycenter-dense-rng-continuity', layers: [0, 0, 1, 1, 1, 2, 2, 3], barycenter: true,
    edges: [[0, 4], [1, 2], [0, 3], [1, 4], [2, 6], [3, 5], [4, 6], [5, 7], [6, 7], [0, 7]]},
  {name: 'post-barycenter-four-layer-dense-improvement', layers: [0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3],
    barycenter: true, edges: [[0, 7], [0, 8], [1, 3], [1, 6], [1, 7], [2, 3], [2, 4], [2, 5], [2, 6], [2, 7],
      [3, 8], [3, 9], [3, 11], [4, 8], [4, 9], [4, 11], [5, 8], [5, 10], [5, 12], [6, 8], [6, 9], [6, 10],
      [7, 10], [7, 13], [8, 13], [9, 13], [9, 14], [10, 12], [10, 13], [10, 14], [11, 13]]},
  {name: 'post-barycenter-long-edge-interleaving-improvement', layers: [0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3],
    barycenter: true, edges: [[0, 4], [0, 6], [1, 3], [1, 6], [1, 7], [2, 3], [2, 5], [2, 9], [2, 10], [3, 9],
      [3, 11], [3, 12], [4, 8], [4, 9], [4, 10], [4, 11], [4, 13], [6, 10], [7, 8], [7, 14], [8, 12],
      [8, 13], [8, 14], [9, 12], [9, 13], [9, 14], [10, 12], [11, 12]]},
  {name: 'post-barycenter-asymmetric-branches-improvement', layers: [0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3],
    barycenter: true, edges: [[0, 6], [0, 7], [0, 10], [1, 3], [1, 5], [1, 6], [1, 7], [2, 3], [2, 4], [2, 10],
      [3, 8], [3, 9], [4, 9], [4, 14], [5, 8], [5, 9], [6, 9], [6, 11], [7, 8], [7, 14], [8, 13], [9, 13],
      [10, 13], [10, 14], [11, 12], [11, 14]]}
];
const rows = ['# ELK 0.9.3 TWO_SIDED_GREEDY_SWITCH; name, UTF-16 JSON stage input, UTF-16 JSON expected.',
  '# Complete original stage: node/port order, actual crossings and the shared 48-bit RNG state.'];
const hex = value => Array.from(JSON.stringify(value), character => character.charCodeAt(0).toString(16).padStart(4, '0')).join('');
const observations = cases.map(spec => ({name: spec.name, postBarycenter: Boolean(spec.barycenter),
  ...JSON.parse(observe(JSON.stringify(spec)))}));
let improvements = 0, portChanges = 0, postBarycenterImprovements = 0;
for (const {name, input, expected, postBarycenter} of observations) {
  assert(expected.crossings <= expected.beforeCrossings, name);
  if (expected.crossings < expected.beforeCrossings) {
    ++improvements;
    if (postBarycenter) ++postBarycenterImprovements;
  }
  if (JSON.stringify(input.ports) !== JSON.stringify(expected.ports)) ++portChanges;
  rows.push([name, hex(input), hex(expected)].join('\t'));
}
assert(improvements >= 8, 'The fixture must exercise actual crossing improvements.');
assert(portChanges >= 4, `The fixture must exercise actual port changes; observed ${portChanges}.`);
assert(postBarycenterImprovements >= 3, 'Include actual improvements after the original BARYCENTER stage.');
const fixture = new URL('../../tests/mermaid_cpp/fixtures/elk-greedy-switch.tsv', import.meta.url);
const expected = rows.join('\n') + '\n';
if (process.argv.includes('--write')) writeFileSync(fixture, expected);
else assert.equal(readFileSync(fixture, 'utf8').replaceAll('\r\n', '\n'), expected);
console.log(`ELK greedy switch: ${cases.length} original stages; ${improvements} crossing improvements ` +
  `(${postBarycenterImprovements} after BARYCENTER), ${portChanges} port changes.`);
if (process.argv.includes('--describe'))
  for (const value of observations) console.log(value.name, value.expected.beforeCrossings, '→', value.expected.crossings,
    value.input.randomState, '→', value.expected.randomState);
