'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

function findRoot(start=__dirname) { for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current)) if(fs.existsSync(path.join(current,'tools/math-port/profiles/scope.json'))) return current; return null; }
const rootArg=process.argv.indexOf('--repo-root');
assert.ok(rootArg<0?process.argv.length===2:(process.argv[rootArg+1]&&process.argv.length===rootArg+2),'usage: node common-containers-method-source-witness.cjs [--repo-root <repository>]');
const root=rootArg<0?findRoot():path.resolve(process.argv[rootArg+1]);
assert.ok(root,'pass --repo-root <repository> or run from the repository');
const lock=JSON.parse(fs.readFileSync(path.join(root,'tests/math/fixtures/common-source/containers-source-lock.json'),'utf8'));
const tsRoot = path.join(root, '.reference/MathJax-src/ts/output/common/Wrappers');
const cjsRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/output/common/Wrappers');
const sha256 = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for(const relative of ['output/common/Wrappers/mpadded.ts','output/common/Wrappers/semantics.ts','output/common/Wrappers/TeXAtom.ts'])assert.equal(sha256(path.join(root,'.reference/MathJax-src/ts',relative)),lock.sourceTsSha256[relative],`pinned ${relative} changed`);
for(const relative of ['output/common/Wrappers/mpadded.js','output/common/Wrappers/semantics.js','output/common/Wrappers/TeXAtom.js'])assert.equal(sha256(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',relative)),lock.cjsSha256[`src/cjs/${relative}`],`installed ${relative} differs from pinned build`);
assert.equal(JSON.parse(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))).version,lock.version,'MathJax package version');

const { CommonMpaddedMixin } = require(path.join(cjsRoot, 'mpadded.js'));
const { CommonSemanticsMixin } = require(path.join(cjsRoot, 'semantics.js'));
const { CommonTeXAtomMixin } = require(path.join(cjsRoot, 'TeXAtom.js'));

function Base() {}
Base.prototype.computeBBox = function(box) { box.w = 17; box.h = 9; box.d = 4; };
const number = (text, size) => {
  const s = String(text);
  const m = s.match(/[-+]?(?:\d+(?:\.\d*)?|\.\d+)(%|em)?/);
  if (!m) return size;
  const value = Number(m[0].replace(/%|em/g, ''));
  return m[1] === '%' ? value * size / 100 : value;
};
function makeMpadded(attributes, child) {
  const Mpadded = CommonMpaddedMixin(Base);
  const wrapper = new Mpadded();
  wrapper.node = { attributes };
  wrapper.childNodes = [child];
  wrapper.length2em = (value, size = 1) => number(value, size);
  wrapper.getAlignX = (width, box, align) => {
    if (align === 'right') return width - (box.w + (box.R || 0)) * (box.rscale || 1);
    if (align === 'left') return (box.L || 0) * (box.rscale || 1);
    return (width - box.w * (box.rscale || 1)) / 2;
  };
  return wrapper;
}
let dims = { width: '+50%', height: '-50%', depth: '+25%', lspace: '0.25em',
  voffset: '-0.125em', 'data-align': 'right' };
const attributes = {
  getList(...keys) { return Object.fromEntries(keys.map(key => [key, dims[key] || ''])); },
  get(key) { return dims[key] || ''; },
};
let childBox = { w: 4, h: 3, d: 1, R: .2, rscale: 1 };
let breaks = 0, widthRecomputations = [];
const child = {
  getOuterBBox() { return childBox; },
  breakToWidth(width) { ++breaks; childBox = { ...childBox, w: width }; },
};
const padded = makeMpadded(attributes, child);
const dimensions = padded.getDimens();
assert.deepEqual(dimensions.slice(0, 8), [3, 1, 4, -1.5, .25, 2, .25, -.125]);
assert.ok(Math.abs(dimensions[8] - 1.8) < 1e-12);
const box = {};
padded.setBBoxDimens(box);
assert.deepEqual(box, { w: 6, h: 1.5, d: 1.25 });
assert.equal(padded.getChildAlign(0), 'right');
padded.getBBox = () => ({ w: 6 });
assert.equal(padded.getWrapWidth(0), 6);

// The overflowing branch calls the actual child break method, then derives its
// dimensions again from the child result before assigning percentage widths.
dims = { width: '1em', 'data-overflow': 'linebreak' };
childBox = { w: 4, h: 3, d: 1, R: 0, rscale: 1 };
padded.jax = { math: { root: { attributes: { get: () => '' } } } };
padded.setChildPWidths = (recompute, width) => { widthRecomputations.push([recompute, width]); };
const overflowBox = {};
padded.computeBBox(overflowBox, true);
assert.equal(breaks, 1);
assert.deepEqual(overflowBox, { w: 1, h: 3, d: 1 });
assert.deepEqual(widthRecomputations, [[true, 1]]);

const Semantics = CommonSemanticsMixin(Base);
const semantics = new Semantics();
semantics.node = { isEmbellished: false };
semantics.childNodes = [{
  getBBox: () => ({ w: 5, h: 2, d: .5 }),
  breakCount: 3,
}, { getBBox: () => ({ w: 99, h: 99, d: 99 }), breakCount: 99 }];
const semanticBox = {};
semantics.computeBBox(semanticBox);
assert.deepEqual(semanticBox, { w: 5, h: 2, d: .5 });
assert.equal(semantics.breakCount, 3);
semantics.node.isEmbellished = true;
semantics.coreMO = () => ({ embellishedBreakCount: 7 });
assert.equal(semantics.breakCount, 7);

const TeXAtom = CommonTeXAtomMixin(Base);
const atom = new TeXAtom();
atom.childNodes = [{ bbox: { ic: .125 } }];
const atomBox = {};
atom.computeBBox(atomBox);
assert.deepEqual(atomBox, { w: 17, h: 9, d: 4, ic: .125 });

const evidence={version:lock.version,checks:['relative mpadded dimensions and alignment','overflow child break and percentage-width recomputation','semantics presentation-child bbox and embellished break count','TeXAtom bbox and italic correction'],observed:{relativeDimensions:dimensions,relativeBox:box,overflowBox,widthRecomputations,semanticBox,semanticBreakCount:semantics.breakCount,atomBox}};
const output=path.join(root,'build/math-port/common-source/containers-method-source.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify(evidence,null,2)+'\n');
console.log(`verified Common Mpadded/Semantics/TeXAtom Source methods; evidence ${path.relative(root,output)}`);
