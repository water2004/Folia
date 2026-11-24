'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '../..');
const bundle = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const sourceRoot = path.join(root, '.reference/MathJax-src/ts');
const lock = JSON.parse(fs.readFileSync(path.join(root, 'tests/math/fixtures/tex-source/color-source-lock.json'), 'utf8'));
assert.equal(lock.version,'4.1.3');
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const [file, hash] of Object.entries(lock.ts))
  assert.equal(digest(path.join(sourceRoot,file)), hash, `pinned TS changed: ${file}`);
for (const [file, hash] of Object.entries(lock.cjs))
  assert.equal(digest(path.join(bundle,'src/cjs',file)), hash, `pinned CJS changed: ${file}`);
assert.equal(JSON.parse(fs.readFileSync(path.join(bundle,'src/package.json'),'utf8')).version,'4.1.3');

const { ColorModel } = require(path.join(bundle,'src/cjs/input/tex/color/ColorUtil.js'));
const { COLORS } = require(path.join(bundle,'src/cjs/input/tex/color/ColorConstants.js'));
const observeCall = callback => {
  try { return { value: callback() }; }
  catch (error) { return { error: { id: error.id, message: error.message } }; }
};

const cases = [
  ['default-model-empty','', 'Orange'],
  ['named-model','named', 'rebeccapurple'],
  ['rgb-half','rgb','0.5,0,1'],
  ['rgb-decimal-forms','rgb','.2,1.,0'],
  ['rgb-unicode-comma-space','rgb','\uFEFF0.5\u3000,\u00A00.25,1\u202F'],
  ['rgb-leading-zeroes','rgb','000.5,00,01'],
  ['integer-rgb','RGB','128,0,255'],
  ['integer-rgb-leading-zeroes','RGB','0001,002,03'],
  ['gray-space','gray','\uFEFF0.5\u00A0'],
  ['gray-fraction','gray','.25'],
  ['gray-one','gray','1'],
  ['pass-css-name','named','not-a-constant'],
  ['invalid-name-semicolon','named','red;blue'],
  ['rgb-wrong-count','rgb','0,1'],
  ['rgb-invalid-decimal','rgb','1e-1,0,0'],
  ['rgb-out-of-range','rgb','1.001,0,0'],
  ['rgb-negative-syntax','rgb','-0.1,0,0'],
  ['rgb-large-decimal','rgb','9'.repeat(400)+',0,0'],
  ['RGB-invalid-integer','RGB','1.0,0,0'],
  ['RGB-out-of-range','RGB','256,0,0'],
  ['RGB-large-integer','RGB','9'.repeat(400)+',0,0'],
  ['gray-invalid','gray','.'],
  ['gray-out-of-range','gray','1.01'],
  ['gray-unicode-line-space','gray','\u2028.75\u2029'],
  ['gray-underflow','gray','0.'+'0'.repeat(400)+'1'],
  ['unknown-model','CMYK','0,0,0,1'],
];
const values = cases.map(([name,model,definition]) => ({
  name, ...observeCall(() => new ColorModel().getColor(model,definition)),
}));

const first = new ColorModel();
const second = new ColorModel();
first.defineColor('rgb','Custom','0,0.5,1');
const custom = first.getColor('named','Custom');
const isolated = second.getColor('named','Custom');
first.defineColor('named','Red','rebeccapurple');
const overriddenConstant = first.getColor('','Red');
const untouchedConstant = second.getColor('','Red');
first.defineColor('RGB','Custom','1,2,3');
const redefined = first.getColor('named','Custom');
const invalidDefine = observeCall(() => first.defineColor('rgb','Bad','0,2,0'));
const unchangedAfterFailedDefine = first.getColor('named','Bad');
first.defineColor('RGB','kept-on-failure','4,5,6');
const invalidRedefine = observeCall(() => first.defineColor('rgb','kept-on-failure','0,2,0'));
const unchangedAfterFailedRedefine = first.getColor('named','kept-on-failure');
first.defineColor('named','semi;name','#abc');
const semicolonNameLookup = first.getColor('named','semi;name');

// Exercise the exact parser/session lifecycle through real MathJax objects,
// not a hand-built approximation of packageData ownership.
const { mathjax } = require(path.join(bundle,'src/cjs/mathjax.js'));
const { TeX } = require(path.join(bundle,'src/cjs/input/tex.js'));
const { liteAdaptor } = require(path.join(bundle,'src/cjs/adaptors/liteAdaptor.js'));
const { RegisterHTMLHandler } = require(path.join(bundle,'src/cjs/handlers/html.js'));
const { STATE } = require(path.join(bundle,'src/cjs/core/MathItem.js'));
require(path.join(bundle,'src/cjs/input/tex/base/BaseConfiguration.js'));
require(path.join(bundle,'src/cjs/input/tex/color/ColorConfiguration.js'));
RegisterHTMLHandler(liteAdaptor());
const tex = new TeX({packages:['base','color']});
const document = mathjax.document('',{InputJax:tex});
document.convert('\\definecolor{Stored}{rgb}{0,0.5,1}x',{end:STATE.COMPILED});
const lifecycleModel = tex.parseOptions.packageData.get('color').model;
const lifecycleAfterDefine=lifecycleModel.getColor('named','Stored');
document.convert('\\color{Stored}x',{end:STATE.COMPILED});
const sameAfterColor=tex.parseOptions.packageData.get('color').model===lifecycleModel;
tex.reset();
document.convert('\\color{Stored}y',{end:STATE.COMPILED});
const sameAfterReset=tex.parseOptions.packageData.get('color').model===lifecycleModel;
const lifecycleAfterReset=tex.parseOptions.packageData.get('color').model.getColor('named','Stored');
const otherTex=new TeX({packages:['base','color']});
const otherDocument=mathjax.document('',{InputJax:otherTex});
otherDocument.convert('x',{end:STATE.COMPILED});
const otherModel=otherTex.parseOptions.packageData.get('color').model;

const actual = {
  constants: Array.from(COLORS.entries()),
  color_cases: values,
  state_cases: {
    custom, isolated, overridden_constant: overriddenConstant,
    untouched_constant: untouchedConstant, redefined,
    invalid_define: invalidDefine, unchanged_after_failed_define: unchangedAfterFailedDefine,
    invalid_redefine: invalidRedefine,
    unchanged_after_failed_redefine: unchangedAfterFailedRedefine,
    semicolon_name_lookup: semicolonNameLookup,
  },
  actual_tex_lifecycle: {
    after_define: lifecycleAfterDefine,
    same_model_after_color: sameAfterColor,
    same_model_after_reset: sameAfterReset,
    after_reset: lifecycleAfterReset,
    separate_jax_has_distinct_model: otherModel!==lifecycleModel,
    separate_jax_lookup: otherModel.getColor('named','Stored'),
  },
};
assert.equal(actual.constants.length,68);
assert.equal(new Set(actual.constants.map(([name])=>name)).size,68);
// Native immutable-table comparison runs in Python's portable tooling suite.
// This witness only observes the independently pinned official ColorModel.

const expectedPath = path.join(root,'tests/math/fixtures/tex-source/color-source-expected.json');
const expected=JSON.parse(fs.readFileSync(expectedPath,'utf8'));
assert.deepEqual(actual,expected);
process.stdout.write(`pinned Source color witness matched ${actual.constants.length} constants, ${values.length} behavior cases and the real TeX lifecycle\n`);

// The original package methods through the actual TeX lifecycle, in addition
// to the existing ColorModel witness above.
const colorReference = require('./reference-profile.cjs').createReference();
assert.equal(colorReference.provenance.compiled_source_sha256, 'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const {AbstractMathItem} = require(path.join(bundle,'src/cjs/core/MathItem.js'));
const {MmlFactory} = require(path.join(bundle,'src/cjs/core/MmlTree/MmlFactory.js'));
const colorModule = require(path.join(bundle,'src/cjs/input/tex/color/ColorMethods.js'));
assert.deepEqual(Object.keys(colorModule.ColorMethods).sort(), ['Color','TextColor','DefineColor','ColorBox','FColorBox'].sort());
function colorFixture(options={}) {
  let error='';
  const jax=new TeX({packages:['base','color'],...options,formatError:(j,e)=>{error=e.id;return j.formatError(e);}});
  jax.setMmlFactory(new MmlFactory());
  return {jax,compile(source){error='';return jax.compile(new AbstractMathItem(source,jax,false),colorReference.document);},get error(){return error;}};
}
function colorTokens(root){const nodes=[];root.walkTree(n=>{if(n.isToken)nodes.push(n);});return nodes;}
function colorBoxes(root){const nodes=[];root.walkTree(n=>{if(n.kind==='mpadded')nodes.push(n);});return nodes;}
{
  const f=colorFixture();assert.equal(f.jax.parseOptions.options.color.padding,'5px');assert.equal(f.jax.parseOptions.options.color.borderWidth,'2px');
  const nodes=colorTokens(f.compile('\\color{red}a\\textcolor{blue}{b+\\color{green}c}d'));
  assert.deepEqual(nodes.map(n=>n.attributes.get('mathcolor')),['red','blue','blue','green','red']);assert.equal(f.error,'');
  f.compile('\\definecolor{Stored}{rgb}{0,0.5,1}');const model=f.jax.parseOptions.packageData.get('color').model;
  assert.equal(colorTokens(f.compile('\\color{Stored}x'))[0].attributes.get('mathcolor'),'#007fff');f.jax.reset(9);
  assert.equal(colorTokens(f.compile('\\textcolor{Stored}{y}'))[0].attributes.get('mathcolor'),'#007fff');assert.equal(f.jax.parseOptions.packageData.get('color').model===model,true);
  assert.equal(colorFixture().jax.parseOptions.packageData.get('color').model.getColor('named','Stored'),'Stored');assert.equal(model.getColor('named','Stored'),'#007fff');
}
{
  const f=colorFixture(),boxes=colorBoxes(f.compile('\\colorbox{red}{hi}\\colorbox{blue}{$x^2$}'));
  assert.equal(boxes.length,2);assert.equal(boxes[0].attributes.get('mathbackground'),'red');assert.equal(boxes[1].attributes.get('mathbackground'),'blue');
  for(const box of boxes)for(const [key,value]of Object.entries({width:'+10px',height:'+5px',depth:'+5px',lspace:'5px'}))assert.equal(box.attributes.get(key),value);
  assert.deepEqual(colorTokens(boxes[0]).map(n=>[n.kind,n.getText()]),[['mtext','hi']]);let script=false;boxes[1].walkTree(n=>{if(n.kind==='msup')script=true;});assert.equal(script,true);
}
{
  const f=colorFixture({color:{padding:'0.25em',borderWidth:'0.1em'}}),boxes=colorBoxes(f.compile('\\fcolorbox[rgb]{1,0,0}{0,0,1}{hi}'));
  assert.equal(boxes.length,1);for(const [key,value]of Object.entries({mathbackground:'#0000ff',style:'border: 0.1em solid #ff0000',width:'+0.5em',lspace:'0.25em'}))assert.equal(boxes[0].attributes.get(key),value);
}
for(const [input,expected]of [['5px','+10px'],['.25em','+0.5em'],['2e2pt','+400pt'],['0x10em','+0em'],['-2px','+NaNpx'],['2EM','+4'],['1e-400px','+0px'],['1e400px','+Infinitypx'],['5px\n','+105px\n'],['','+NaN']]) {
  const p=colorModule.padding(input);assert.equal(p.width,expected);assert.equal(p.lspace,input);assert.equal(p.height,'+'+input);
}
for(const [source,id]of [['\\color[cmyk]{0,0,0,0}x','UndefinedColorModel'],['\\textcolor[rgb]{2,0,0}{x}','ModelArg2'],['\\definecolor{bad}{RGB}{1,2}','ModelArg1'],['\\color{red;blue}x','BadColorValue']]) {
  const f=colorFixture();f.compile(source);assert.equal(f.error,id);assert.equal(colorTokens(f.compile('\\color{green}x'))[0].attributes.get('mathcolor'),'green');assert.equal(f.error,'');
}
console.log(JSON.stringify({packageMethods:5,paddingCases:10,compiled_source_sha256:colorReference.provenance.compiled_source_sha256}));
