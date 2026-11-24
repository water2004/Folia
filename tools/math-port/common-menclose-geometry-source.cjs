'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

function findRoot(start=__dirname){for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))if(fs.existsSync(path.join(current,'docs/math-port/scope.json'))&&fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')))return current;return null;}
const rootArg=process.argv.indexOf('--repo-root');
assert.ok(rootArg<0?process.argv.length===2:(process.argv[rootArg+1]&&process.argv.length===rootArg+2),'usage: node common-menclose-geometry-source.cjs [--repo-root <repository>]');
const root=rootArg<0?findRoot():path.resolve(process.argv[rootArg+1]);
assert.ok(root,'pass --repo-root <repository> or run from the repository');
const bundle = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const sourceRoot = path.join(root, '.reference/MathJax-src/ts');
const fixtures = path.join(root, 'tests/math/fixtures/common-source');
const lock = JSON.parse(fs.readFileSync(path.join(fixtures, 'menclose-source-lock.json'), 'utf8'));
const sha = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const [file, digest] of Object.entries(lock.source))
  assert.equal(sha(path.join(sourceRoot, file)), digest, `locked TS changed: ${file}`);
for (const [file, digest] of Object.entries(lock.cjs))
  assert.equal(sha(path.join(bundle, 'src/cjs', file)), digest, `locked CJS changed: ${file}`);
assert.equal(JSON.parse(fs.readFileSync(path.join(bundle, 'src/package.json'))).version, '4.1.3');
assert.equal(sha(path.join(root,'docs/math-port/scope.json')),lock.scopeSha256,'MathJax scope changed');
const svgRootSource = fs.readFileSync(path.join(sourceRoot, 'output/svg.ts'), 'utf8');
const svgRootCjs = fs.readFileSync(path.join(bundle, 'src/cjs/output/svg.js'), 'utf8');
assert.match(svgRootSource, /'stroke-width': 0,[\s\S]{0,100}transform: 'scale\(1,-1\)'/);
assert.match(svgRootCjs, /'stroke-width': 0,[\s\S]{0,100}transform: 'scale\(1,-1\)'/);

// Load the actual SvgWrapper class while stubbing only its CommonWrapper
// superclass and unused svg namespace import to avoid the component cycle.
// SvgWrapper.prototype.fixed remains the pinned implementation and executes.
const Module = require('node:module');
const originalLoad = Module._load;
const svgWrapperPath = path.join(bundle, 'src/cjs/output/svg/Wrapper.js');
Module._load = function (request, parent, isMain) {
  if (parent?.filename === svgWrapperPath && request === '../common/Wrapper.js')
    return { CommonWrapper: class CommonWrapperShell {} };
  if (parent?.filename === svgWrapperPath && request === '../svg.js')
    return { XLINKNS: 'http://www.w3.org/1999/xlink' };
  return originalLoad.call(this, request, parent, isMain);
};
const { SvgWrapper } = require(svgWrapperPath);
Module._load = originalLoad;
const { SvgMenclose } = require(path.join(bundle, 'src/cjs/output/svg/Wrappers/menclose.js'));
const { CommonOutputJax } = require(path.join(bundle, 'src/cjs/output/common.js'));
const { CommonWrapper } = require(path.join(bundle, 'src/cjs/output/common/Wrapper.js'));
const { BBox } = require(path.join(bundle, 'src/cjs/util/BBox.js'));
const { CommonMencloseMixin } = require(path.join(bundle, 'src/cjs/output/common/Wrappers/menclose.js'));
const CommonMenclose = CommonMencloseMixin(class CommonMencloseShell {});
const C = SvgMenclose.prototype;
const common = CommonMenclose.prototype;
const commonWrapperSource = fs.readFileSync(path.join(sourceRoot, 'output/common/Wrapper.ts'), 'utf8');
assert.match(commonWrapperSource, /this\.bbox = BBox\.zero\(\)/);
assert.match(commonWrapperSource, /const bbox = save \? this\.bbox : BBox\.zero\(\)/);

const cases = {
  positive: { w: 3.0, h: 1.25, d: 0.5 },
  negative_height: { w: 2.4, h: -0.4, d: 1.1 },
};
const parsePath = d => {
  const tokens = d.match(/[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?/g) || [];
  const result = [];
  let i = 0, command = '', x = 0, y = 0;
  while (i < tokens.length) {
    if (/^[A-Za-z]$/.test(tokens[i])) command = tokens[i++];
    assert(command, `missing SVG path command in ${d}`);
    if (command === 'Z' || command === 'z') { result.push(['Z']); command = ''; continue; }
    if (command === 'M' || command === 'm' || command === 'L' || command === 'l') {
      const relative = command === command.toLowerCase();
      const nx = Number(tokens[i++]), ny = Number(tokens[i++]);
      x = relative ? x + nx : nx; y = relative ? y + ny : ny;
      result.push([command.toUpperCase(), x / 1000, y / 1000]);
      if (command === 'M') command = 'L'; else if (command === 'm') command = 'l';
      continue;
    }
    if (command === 'a' || command === 'A') {
      const rx = Number(tokens[i++]), ry = Number(tokens[i++]), rotation = Number(tokens[i++]);
      const large = Number(tokens[i++]), sweep = Number(tokens[i++]);
      const dx = Number(tokens[i++]), dy = Number(tokens[i++]);
      const nx = command === 'a' ? x + dx : dx, ny = command === 'a' ? y + dy : dy;
      x = nx; y = ny;
      result.push(['A', rx / 1000, ry / 1000, rotation,
        !!large, !!sweep, x / 1000, y / 1000]);
      continue;
    }
    throw new Error(`unsupported SVG path command ${command} in ${d}`);
  }
  return result;
};
const transformOps = value => {
  if (!value) return [];
  const ops = [];
  const pattern = /(translate|rotate)\(([^)]*)\)/g;
  for (const match of value.matchAll(pattern)) {
    const nums = match[2].trim().split(/[ ,]+/).map(Number);
    if (match[1] === 'translate') ops.push(['T', nums[0] / 1000, nums[1] / 1000]);
    else ops.push(['R', nums[0], nums[1] / 1000, nums[2] / 1000]);
  }
  assert.equal(value.replace(pattern, '').trim(), '', `unsupported Source transform spelling: ${value}`);
  return ops;
};
const sceneShape = element => {
  const { tag, attrs } = element;
  const allowed = {
    line: ['x1','y1','x2','y2','stroke-width'],
    rect: ['x','y','width','height','rx','fill','stroke-width'],
    ellipse: ['rx','ry','cx','cy','fill','stroke-width'],
    path: ['d','style','stroke-linecap','stroke-linejoin','fill','transform'],
  }[tag];
  assert(allowed, `unsupported captured SVG tag ${tag}`);
  for (const key of Object.keys(attrs)) assert(allowed.includes(key), `${tag} paint/geometry attribute is not modeled: ${key}`);
  if (attrs.style) for (const key of Object.keys(attrs.style))
    assert.equal(key, 'stroke-width', `unmodeled Source SVG style property: ${key}`);
  let segments;
  let fill = 'inherited', strokeWidth = 0;
  if (tag === 'line') {
    segments = [['M', Number(attrs.x1) / 1000, Number(attrs.y1) / 1000],
      ['L', Number(attrs.x2) / 1000, Number(attrs.y2) / 1000]];
    strokeWidth = Number(attrs['stroke-width']) / 1000;
  } else if (tag === 'rect') {
    const x = Number(attrs.x), y = Number(attrs.y), w = Number(attrs.width), h = Number(attrs.height);
    const x0 = x / 1000, x1 = (x + w) / 1000, y0 = y / 1000, y1 = (y + h) / 1000;
    const rr = attrs.rx === undefined ? 0 : Math.min(Number(attrs.rx), w / 2, h / 2);
    const r = rr / 1000;
    // These operations are on the actual formatter-produced milli-em
    // attributes. Sum before dividing so this canonical path interpretation
    // represents the same emitted decimal endpoints as the native writer.
    if (!rr) segments = [['M', x0, y0], ['L', x1, y0],
      ['L', x1, y1], ['L', x0, y1], ['Z']];
    else segments = [['M', (x + rr) / 1000, y0], ['L', (x + w - rr) / 1000, y0],
      ['A', r, r, 0, false, true, x1, (y + rr) / 1000],
      ['L', x1, (y + h - rr) / 1000], ['A', r, r, 0, false, true, (x + w - rr) / 1000, y1],
      ['L', (x + rr) / 1000, y1], ['A', r, r, 0, false, true, x0, (y + h - rr) / 1000],
      ['L', x0, (y + rr) / 1000], ['A', r, r, 0, false, true, (x + rr) / 1000, y0], ['Z']];
    fill = 'none'; strokeWidth = Number(attrs['stroke-width']) / 1000;
  } else if (tag === 'ellipse') {
    const rx = Number(attrs.rx) / 1000, ry = Number(attrs.ry) / 1000;
    const rawRx = Number(attrs.rx), rawRy = Number(attrs.ry);
    const rawCx = Number(attrs.cx), rawCy = Number(attrs.cy);
    segments = [['M', (rawCx + rawRx) / 1000, rawCy / 1000],
      ['A', rx, ry, 0, false, true, (rawCx - rawRx) / 1000, rawCy / 1000],
      ['A', rx, ry, 0, false, true, (rawCx + rawRx) / 1000, rawCy / 1000], ['Z']];
    fill = 'none'; strokeWidth = Number(attrs['stroke-width']) / 1000;
  } else if (tag === 'path') {
    segments = parsePath(attrs.d);
    if (attrs.style) {
      fill = 'none';
      strokeWidth = Number(attrs.style['stroke-width']) / 1000;
    } else {
      fill = 'inherited';
    }
  } else throw new Error(`unsupported captured SVG tag ${tag}`);
  const paint = { fill, stroke: 'inherited', stroke_width: strokeWidth };
  // Absence is semantic (inherit); explicitly set Source values stay typed.
  paint.opacity = attrs.opacity === undefined ? 1 : Number(attrs.opacity);
  paint.fill_rule = attrs['fill-rule'] ?? 'nonzero';
  paint.fill_opacity = attrs['fill-opacity'] ?? null;
  paint.stroke_opacity = attrs['stroke-opacity'] ?? null;
  paint.line_cap = attrs['stroke-linecap'] ?? null;
  const sourceJoin = attrs['stroke-linejoin'] ?? null;
  paint.line_join = sourceJoin === 'mitre' ? 'miter' : sourceJoin;
  return { segments, transform: transformOps(attrs.transform), paint };
};

const names = [...SvgMenclose.notations.keys()];
assert.equal(names.length, 29);
const output = [];
const svgFixed = SvgWrapper.prototype.fixed;
const jaxFixed = CommonOutputJax.prototype.fixed;
for (const [caseName, childBox] of Object.entries(cases)) {
  for (const name of names) {
    const def = SvgMenclose.notations.get(name);
    if (def.renderChild) continue; // radical is separately checked by projection tests.
    const sourceMetrics = Object.create(common);
    Object.assign(sourceMetrics, {
      notations: { [name]: def }, padding: 0.2, thickness: 0.067,
      arrowhead: { x: 4, y: 2, dx: 1 }, childNodes: [{ getBBox: () => childBox }],
    });
    const trbl = common.getBBoxExtenders.call(sourceMetrics);
    sourceMetrics.TRBL = trbl;
    const sourceChildBBox = new BBox(childBox);
    Object.assign(sourceMetrics, {
      childNodes: [{ getBBox: () => sourceChildBBox }],
      setChildPWidths() {},
      bbox: BBox.zero(),
      bboxComputed: false,
    });
    // Invoke the actual pinned CommonWrapper.getBBox against the actual
    // CommonMenclose.computeBBox prototype method. The real constructor seeds
    // bbox with BBox.zero(), and getBBox computes into that object on first save.
    const sourceOuterBBox = CommonWrapper.prototype.getBBox.call(sourceMetrics);
    const box = { w: sourceOuterBBox.w, h: sourceOuterBBox.h, d: sourceOuterBBox.d };
    const elements = [];
    const fixedInputs = [];
    const jaxFixedInputs = [];
    const node = Object.create(C);
    Object.assign(node, {
      padding: 0.2, thickness: 0.067, arrowhead: { x: 4, y: 2, dx: 1 },
      TRBL: trbl, childNodes: [{ getBBox: () => sourceChildBBox }], dom: [{}],
      getBBox: () => box,
      // Instrument only the inputs. Delegate to the actual pinned SvgWrapper
      // and CommonOutputJax methods for every formatted SVG attribute.
      fixed(value, digits) {
        fixedInputs.push({ em: value, digits: digits === undefined ? 1 : digits });
        return svgFixed.call(this, value, digits);
      },
      getOffset: common.getOffset.bind(sourceMetrics),
      getArgMod: common.getArgMod.bind(sourceMetrics),
      arrowData: common.arrowData.bind(sourceMetrics),
      arrowAW: common.arrowAW.bind(sourceMetrics),
      jax: { fixed(value, digits) {
        jaxFixedInputs.push({ milliEm: value, digits: digits === undefined ? 3 : digits });
        return jaxFixed.call(this, value, digits);
      } },
      svg: (tag, attrs = {}) => ({ tag, attrs: { ...attrs } }),
      adaptor: {
        append(_parent, child) { elements.push(child); return child; },
        setAttribute(element, key, value) { element.attrs[key] = value; },
      },
    });
    def.renderer(node, null);
    const normalized = elements.map(sceneShape);
    const sourcePaintFeatures = elements.map(({ tag, attrs }) => ({
      tag,
      linecap: attrs['stroke-linecap'] || null,
      linejoin: attrs['stroke-linejoin'] || null,
      fill: attrs.fill === undefined ? 'svg-default' : attrs.fill,
      stroke_width: attrs['stroke-width'] ?? attrs.style?.['stroke-width'] ?? null,
    }));
    output.push({ case: caseName, notation: name, child: childBox,
      trbl, source_bbox: box, fixed_inputs_em: fixedInputs,
      jax_fixed_inputs_milli_em: jaxFixedInputs,
      source_svg: elements, svg_local_geometry: normalized,
      source_paint_features: sourcePaintFeatures });
  }
}
assert.equal(output.length, 56);
const paintExtensions = [...new Set(output.flatMap(row => row.source_paint_features
  .filter(feature => feature.linecap || feature.linejoin)
  .map(feature => `${row.notation}:${feature.linecap}:${feature.linejoin}`)))].sort();
assert.deepEqual(paintExtensions, ['longdiv:round:round', 'phasorangle:round:mitre']);
const explicitCaps = output.flatMap(row => row.svg_local_geometry.map(shape =>
  [row.notation, shape.paint.line_cap, shape.paint.line_join]))
  .filter(([, cap, join]) => cap !== null || join !== null);
assert.deepEqual(explicitCaps, [
  ['phasorangle', 'round', 'miter'], ['longdiv', 'round', 'round'],
  ['phasorangle', 'round', 'miter'], ['longdiv', 'round', 'round'],
]);
assert(output.every(row => row.svg_local_geometry.every(shape =>
  ['phasorangle', 'longdiv'].includes(row.notation) ||
  (shape.paint.line_cap === null && shape.paint.line_join === null))));
const expectedPath = path.join(fixtures, 'menclose-geometry-source-expected.json');
const generated={version:lock.version,cases,rows:output};
// The fixture is the JSON-persisted witness: its defined boundary normalizes
// JavaScript negative zero to 0, so compare through the same serialization.
assert.deepEqual(JSON.parse(JSON.stringify(generated)),JSON.parse(fs.readFileSync(expectedPath,'utf8')),
  'pinned Source geometry/BBox/formatted SVG observations changed');
const evidenceDir=path.join(root,'build/math-port/common-source');
fs.mkdirSync(evidenceDir,{recursive:true});
const evidencePath=path.join(evidenceDir,'menclose-geometry-source.json');
fs.writeFileSync(evidencePath,JSON.stringify(generated,null,2)+'\n');
const {byteField, binary64, writeTables} = require('./fixture-data.cjs');
// The former generated C++ header used JSON.stringify numbers, which converted
// negative zero to zero. Keep that established persisted-reference boundary.
const persistedNumber = value => binary64(Object.is(value, -0) ? 0 : value);
writeTables(path.join(root, 'tests/math/fixtures/output/menclose'), {
  geometry: {
    columns: ['case_name_bytes', 'notation_bytes', 'w_binary64', 'h_binary64', 'd_binary64',
      'top_binary64', 'right_binary64', 'bottom_binary64', 'left_binary64'],
    rows: output.map(row => [byteField(row.case), byteField(row.notation),
      ...[row.source_bbox.w, row.source_bbox.h, row.source_bbox.d, ...row.trbl].map(persistedNumber)])
  }
});
console.log(`verified ${output.length} pinned Source SVG handler outputs and raw Common layout rows (${names.length} catalog names; radical handled by projection); evidence ${path.relative(root,evidencePath)}`);
