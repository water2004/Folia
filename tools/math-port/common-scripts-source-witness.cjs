'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
function findRoot(start=__dirname) {
  for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))
    if(fs.existsSync(path.join(current,'tools/math-port/profiles/scope.json')) && fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  return null;
}
const rootArg=process.argv.indexOf('--repo-root');
assert.ok(rootArg<0 ? process.argv.length===2 : (process.argv[rootArg+1] && process.argv.length===rootArg+2),'usage: node common-scripts-source-witness.cjs [--repo-root <repository>]');
const root=rootArg<0?findRoot():path.resolve(process.argv[rootArg+1]);
assert.ok(root,'pass --repo-root <repository> or run from within the repository');
const pkg = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const lock = JSON.parse(fs.readFileSync(path.join(root,'tests/math/fixtures/common-source/scripts-source-lock.json'),'utf8'));
assert.equal(JSON.parse(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))).version,lock.version,'MathJax package version');
for (const [relative, expected] of Object.entries(lock.sha256)) {
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(pkg, relative))).digest('hex');
  assert.equal(actual, expected, `pinned MathJax source changed: ${relative}`);
}
const { MmlFactory } = require(path.join(pkg, 'core/MmlTree/MmlFactory.js'));
const { CommonScriptbaseMixin } = require(path.join(pkg, 'output/common/Wrappers/scriptbase.js'));
const { CommonMsubMixin, CommonMsupMixin, CommonMsubsupMixin } = require(path.join(pkg, 'output/common/Wrappers/msubsup.js'));
const { CommonMmultiscriptsMixin } = require(path.join(pkg, 'output/common/Wrappers/mmultiscripts.js'));
const box = (data = {}) => ({w:.5,h:.7,d:.2,ic:.08,sk:.02,dx:0,rscale:1,...data});
class ChildWrapper {
  constructor(node, parent, metrics = box()) {
    this.node = node; this.parent = parent; this.childNodes = (node.childNodes || []).map(n => new ChildWrapper(n, this));
    this.bbox = metrics; this.mathaccent = undefined;
  }
  getOuterBBox() { return {...this.bbox}; }
  getLineBBox() { return {...this.bbox}; }
  breakCount() { return 0; }
  getText() { return this.node.getText ? this.node.getText() : (this.node.childNodes || []).map(n => n.getText?.() || '').join(''); }
  coreMO() { return {node:this.node}; }
  get isMathAccent() { return false; }
}
const params = {sub1:.2,sub2:.16,sup1:.4,sup2:.3,sup3:.5,sub_drop:.1,sup_drop:.1,x_height:.4,
  rule_thickness:.04,separation_factor:5,big_op_spacing1:.1,big_op_spacing2:.1,
  big_op_spacing3:.1,big_op_spacing4:.1,scriptspace:.05,extra_ic:0};
function Base(factory, node, parent = null) {
    this.factory=factory; this.node=node; this.parent=parent; this.childNodes=(node.childNodes||[]).map((n,i)=>new ChildWrapper(n,this,i===0?box():box({w:.2,h:.3,d:.1,ic:0,sk:0})));
    this.bbox=box(); this.font={params,skewIcFactor:.75}; this.jax={face:{params}};
}
Base.prototype.getOuterBBox = function() { return {...this.bbox}; };
Base.prototype.getLineBBox = function() { return {...this.bbox}; };
Base.prototype.length2em = function(value, fallback) { return value === '' || value == null ? fallback : Number(value); };
const ScriptBase = CommonScriptbaseMixin(Base);
const Msub = CommonMsubMixin(ScriptBase);
const Msup = CommonMsupMixin(ScriptBase);
const Msubsup = CommonMsubsupMixin(ScriptBase);
const Mmultiscripts = CommonMmultiscriptsMixin(CommonMsubsupMixin(ScriptBase));
const factory = new MmlFactory();
const text = value => factory.create('text').setText(value);
const mi = value => factory.create('mi', {}, [text(value)]);
const msubNode = factory.create('msub', {}, [mi('x'), mi('i')]);
const msub = new Msub({}, msubNode);
assert.equal(msub.baseIsChar, true);
assert.equal(msub.scriptChild.node.getText(), 'i');
assert.deepEqual(msub.getOffset(), [0, -.2]);
assert.equal(msub.getDelta(msub.childNodes[1]), .06);
const msupNode = factory.create('msup', {}, [mi('x'), mi('2')]);
const msup = new Msup({}, msupNode);
assert.deepEqual(msup.getOffset(), [.054000000000000006, .3]);
const pairNode = factory.create('msubsup', {}, [mi('x'), mi('i'), mi('2')]);
const pair = new Msubsup({}, pairNode);
pair.baseCore = pair.childNodes[0]; pair.baseScale = 1; pair.baseIsChar = true;
const uvq = pair.getUVQ();
assert.equal(uvq.length, 3);
assert.deepEqual(uvq, [.42000000000000004, -.09999999999999992, .12]);
assert.deepEqual(pair.getUVQ(), uvq);
const multiNode = factory.create('mmultiscripts', {}, [mi('x'),mi('i'),mi('2'),
  factory.create('mprescripts', {}, []),mi('j'),mi('3')]);
const multi = new Mmultiscripts({}, multiNode);
multi.getScriptData();
assert.equal(multi.scriptData.numScripts, 1);
assert.equal(multi.scriptData.numPrescripts, 1);
assert.equal(multi.firstPrescript, 4);
const report = {mathjaxVersion: lock.version, source: 'actual CJS MmlFactory + CommonMsub/CommonMsup/CommonMsubsup/CommonMmultiscripts mixin instances; FontData skewIcFactor 0.75',
  cases:{msub:{baseKind:msub.baseCore.node.kind,scriptKind:msub.scriptChild.node.kind,baseIsChar:msub.baseIsChar,offset:msub.getOffset()},
    msup:{baseKind:msup.baseCore.node.kind,scriptKind:msup.scriptChild.node.kind,baseIsChar:msup.baseIsChar,offset:msup.getOffset()},
    msubsup:{uvq,cacheStable:pair.getUVQ().every((v,i)=>v===uvq[i])},
    mmultiscripts:{numScripts:multi.scriptData.numScripts,numPrescripts:multi.scriptData.numPrescripts,firstPrescript:multi.firstPrescript}}};
const output=path.join(root,'build/math-port/common-source/scripts-source.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
process.stdout.write(`verified ${Object.keys(report.cases).length} scripts cases; evidence ${path.relative(root,output)}\n`);
