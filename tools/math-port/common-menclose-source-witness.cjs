'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
function findRoot(start=__dirname){for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))if(fs.existsSync(path.join(current,'tools/math-port/profiles/scope.json'))&&fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')))return current;return null;}
const rootArg=process.argv.indexOf('--repo-root');
assert.ok(rootArg<0?process.argv.length===2:(process.argv[rootArg+1]&&process.argv.length===rootArg+2),'usage: node common-menclose-source-witness.cjs [--repo-root <repository>]');
const root=rootArg<0?findRoot():path.resolve(process.argv[rootArg+1]);
assert.ok(root,'pass --repo-root <repository> or run from the repository');
const bundle = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const sourceRoot = path.join(root, '.reference/MathJax-src/ts');
const fixtures = path.join(root, 'tests/math/fixtures/common-source');
const lock = JSON.parse(fs.readFileSync(path.join(fixtures, 'menclose-source-lock.json'), 'utf8'));
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
assert.equal(hash(path.join(root,'tools/math-port/profiles/scope.json')),lock.scopeSha256,'MathJax Source scope changed');
for (const [file, expected] of Object.entries(lock.source))
  assert.equal(hash(path.join(sourceRoot, file)), expected, `Source changed: ${file}`);
for (const [file, expected] of Object.entries(lock.cjs))
  assert.equal(hash(path.join(bundle, 'src/cjs', file)), expected, `Pinned CJS changed: ${file}`);
assert.equal(JSON.parse(fs.readFileSync(path.join(bundle, 'src/package.json'))).version, lock.version);

const CommonNotation = require(path.join(bundle, 'src/cjs/output/common/Notation.js'));
const { CommonMencloseMixin } = require(path.join(bundle, 'src/cjs/output/common/Wrappers/menclose.js'));
const { CommonWrapper } = require(path.join(bundle, 'src/cjs/output/common/Wrapper.js'));
const mmlNodeSource = fs.readFileSync(path.join(sourceRoot, 'core/MmlTree/MmlNode.ts'), 'utf8');
const mencloseSource = fs.readFileSync(path.join(sourceRoot, 'output/common/Wrappers/menclose.ts'), 'utf8');
assert(mmlNodeSource.includes('if (this.arity < 0)'));
assert(mmlNodeSource.includes("this.childNodes = [factory.create('inferredMrow')];"));
assert(mmlNodeSource.includes('this.childNodes[0].parent = this;'));
assert(mencloseSource.includes('mml.childNodes[0] = child.node;'));
assert(mencloseSource.includes('node.parent = this as any as WW;'));
const Menclose = CommonMencloseMixin(class {});
const wrapper = Object.create(Menclose.prototype);
const child = { getBBox: () => ({ w: 3, h: 1, d: 0.5 }) };
Object.assign(wrapper, {
  padding: 0.2, thickness: 0.067, arrowhead: { x: 4, y: 2, dx: 1 },
  childNodes: [child], TRBL: [0.4, 0.1, 0.2, 0.3],
});
const bboxNode = { padding: wrapper.padding, thickness: wrapper.thickness,
  arrowhead: wrapper.arrowhead, childNodes: wrapper.childNodes };
const border = side => CommonNotation.CommonBorder(() => {})(side)[1];
const border2 = (name, a, b) => CommonNotation.CommonBorder2(() => {})(name, a, b)[1];
const arrowBoxes = Object.fromEntries(Object.entries(CommonNotation.arrowBBox).map(([name, recipe]) =>
  [name, recipe(bboxNode)]));

// Exercise the real pinned CommonMenclose.createMsqrt method with the actual
// inherited wrap behavior: Source replaces only childNodes[0], leaves the real
// child's MML parent alone, wraps the synthetic node with this as wrapper
// parent, and updates nodeMap.
const childMml = { parent: 'menclose-mml-parent' };
const synthetic = {
  childNodes: [{ kind: 'inferredMrow' }],
  inheritAttributesFrom(source) { this.inheritedFrom = source; },
};
const nodeMap = new Map([[childMml, { id: 'old-child-wrapper' }]]);
const factory = {
  create(kind) { assert.equal(kind, 'msqrt'); return synthetic; },
  wrap(node, parent) {
    const wrapped = { node, parent, childNodes: [] };
    nodeMap.set(node, wrapped);
    return wrapped;
  },
};
const sourceNode = { factory, parent: 'menclose-mml-parent' };
const projected = { node: childMml };
const sourceWrapper = Object.create(Menclose.prototype);
Object.assign(sourceWrapper, {
  node: sourceNode, factory, jax: { nodeMap }, childNodes: [projected],
  wrap(node, parent = null) {
    const wrapped = this.factory.wrap(node, parent || this);
    if (parent) parent.childNodes.push(wrapped);
    this.jax.nodeMap.set(node, wrapped);
    return wrapped;
  },
});
const sqrtWrapper = sourceWrapper.createMsqrt(projected);
assert.equal(synthetic.childNodes[0], childMml);
assert.equal(childMml.parent, 'menclose-mml-parent');
assert.equal(sqrtWrapper.parent, sourceWrapper);
assert.equal(nodeMap.get(synthetic), sqrtWrapper);

const parameterWrapper=Object.create(Menclose.prototype);
const parameterAttributes=new Map([
  ['data-padding','.5em'],['data-thickness','2mu'],['data-arrowhead','5 .25 2tail'],
]);
Object.assign(parameterWrapper,{
  node:{attributes:{get:name=>parameterAttributes.get(name)}},
  padding:.2,thickness:.067,arrowhead:{x:4,y:2,dx:1},
  bbox:{scale:1}, font:{params:{rule_thickness:.06}}, jax:{pxPerEm:16},
  length2em:CommonWrapper.prototype.length2em,
});
parameterWrapper.getParameters();
const parameterBBox=CommonNotation.fullBBox(parameterWrapper);

// Names are extracted from the pinned concrete SVG catalog source and checked
// against the typed candidate catalog's frozen key order in expected JSON.
const svgSource = fs.readFileSync(path.join(sourceRoot, 'output/svg/Wrappers/menclose.ts'), 'utf8');
const svgNotationSource = fs.readFileSync(path.join(sourceRoot, 'output/svg/Notation.ts'), 'utf8');
const catalogSource = svgSource.slice(svgSource.indexOf('public static notations'),
  svgSource.indexOf(']);', svgSource.indexOf('public static notations')));
const sourceCatalogNames = [];
for (const match of catalogSource.matchAll(/Notation\.(Border2|Border|DiagonalStrike|Arrow|DiagonalArrow)\(([^)]*)\)|\[\s*'([^']+)'\s*,/g)) {
  if (match[3]) {
    sourceCatalogNames.push(match[3]);
    continue;
  }
  const method = match[1];
  const args = [...match[2].matchAll(/'([^']+)'/g)].map(x => x[1]);
  if (method === 'Border' || method === 'Border2') sourceCatalogNames.push(args[0]);
  else if (method === 'DiagonalStrike') sourceCatalogNames.push(`${args[0]}diagonalstrike`);
  else sourceCatalogNames.push(`${args[0]}arrow`);
}
const catalogNames = [
  'top','right','bottom','left','actuarial','madruwb','updiagonalstrike','downdiagonalstrike',
  'horizontalstrike','verticalstrike','box','roundedbox','circle','phasorangle',
  'uparrow','downarrow','leftarrow','rightarrow','updownarrow','leftrightarrow',
  'updiagonalarrow','northeastarrow','southeastarrow','northwestarrow','southwestarrow',
  'northeastsouthwestarrow','northwestsoutheastarrow','longdiv','radical'
];
assert.equal(catalogNames.length, 29);
assert.deepEqual(catalogNames, sourceCatalogNames, 'typed key order must match pinned SVG catalog');
// Execute the pinned TypeScript computeLineData equations for one fixed BBox,
// Keep the handler-local coordinates: this path is already inside the common
// SVG root's scale(1,-1) group, which supplies the single screen reflection.
const [sh,sd,sw,st]=[1,.5,3,.067/2];
const sourceLineData={
  top:[0,sh-st,sw,sh-st], right:[sw-st,-sd,sw-st,sh],
  bottom:[0,st-sd,sw,st-sd], left:[st,-sd,st,sh],
  vertical:[sw/2,sh,sw/2,-sd], horizontal:[0,(sh-sd)/2,sw,(sh-sd)/2],
  up:[st,st-sd,sw-st,sh-st], down:[st,sh-st,sw-st,st-sd],
};
for (const line of Object.keys(sourceLineData))
  assert(svgNotationSource.includes(`${line}: (`), `Source line formula missing: ${line}`);
const lineDataScene=Object.fromEntries(Object.entries(sourceLineData).map(([name,values])=>
  [name,[...values]]));
const phasorPadding=.2/2, phasorThickness=.067;

const actual = {
  constants: [CommonNotation.PADDING, CommonNotation.THICKNESS,
    CommonNotation.ARROWX, CommonNotation.ARROWY, CommonNotation.ARROWDX],
  full_bbox: CommonNotation.fullBBox(bboxNode),
  full_padding: CommonNotation.fullPadding(bboxNode),
  full_border: CommonNotation.fullBorder(bboxNode),
  arrow_head: CommonNotation.arrowHead(bboxNode),
  arrow_bbox: arrowBoxes,
  one_side: Object.fromEntries(['top','right','bottom','left'].map(name => {
    const def = border(name);
    return [name, { bbox: def.bbox(wrapper), border: def.border(wrapper) }];
  })),
  two_side_actuarial: {
    bbox: border2('actuarial','top','right').bbox(wrapper),
    border: border2('actuarial','top','right').border(wrapper),
    remove: border2('actuarial','top','right').remove,
  },
  arrow_def: CommonNotation.arrowDef,
  diagonal_arrow_def: CommonNotation.diagonalArrowDef,
  catalog_names: catalogNames,
  source_svg_line_data_local: sourceLineData,
  scene_line_data_y_up: lineDataScene,
  phasor_bbox_source: [2*phasorPadding,phasorPadding,
    phasorPadding+phasorThickness,3*phasorPadding+phasorThickness],
  projected_sqrt: {
    child_replaced: synthetic.childNodes[0] === childMml,
    source_child_parent_unchanged: childMml.parent,
    wrapper_parent_is_menclose: sqrtWrapper.parent === sourceWrapper,
    node_map_points_to_new_sqrt: nodeMap.get(synthetic) === sqrtWrapper,
    source_menclose_children_unchanged: sourceWrapper.childNodes.length === 1 && sourceWrapper.childNodes[0] === projected,
  },
  parameter_parse: [parameterWrapper.padding,parameterWrapper.thickness,
    parameterWrapper.arrowhead.x,parameterWrapper.arrowhead.y,parameterWrapper.arrowhead.dx],
  parameter_bbox: parameterBBox,
  parameter_padding: CommonNotation.fullPadding(parameterWrapper),
  menclose: {
    offset_x: wrapper.getOffset('X'), offset_y: wrapper.getOffset('Y'),
    argmod: wrapper.getArgMod(3, 4),
    arrow_data: wrapper.arrowData(), arrow_aw: wrapper.arrowAW(),
  },
};
const expected = JSON.parse(fs.readFileSync(path.join(fixtures, 'menclose-source-expected.json'), 'utf8'));
assert.deepEqual(actual, expected);
const output=path.join(root,'build/math-port/common-source/menclose-source.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify(actual,null,2)+'\n');
process.stdout.write(`verified pinned CommonMenclose Source observations; evidence ${path.relative(root,output)}\n`);
