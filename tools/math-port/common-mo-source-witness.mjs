import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
function findRoot(start=path.dirname(fileURLToPath(import.meta.url))) {
  for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))
    if(fs.existsSync(path.join(current,'tools/math-port/README.md'))&&
       fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')))return current;
  return null;
}
const args=process.argv.slice(2),ri=args.indexOf('--repo-root');
if(ri>=0&&(!args[ri+1]||args.length!==ri+2))throw Error('usage: node --experimental-strip-types --experimental-loader <loader> <witness> [--repo-root <repository>]');
const root=ri>=0?path.resolve(args[ri+1]):findRoot();
assert.ok(root,'pass --repo-root <repository> or run this witness from within the repository');
const sourceFile=path.join(root,'.reference/MathJax-src/ts/output/common/Wrappers/mo.ts');
const packageJson=JSON.parse(fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'),'utf8'));
assert.equal(packageJson.version,'4.1.3');
const sourceSha256=crypto.createHash('sha256').update(fs.readFileSync(sourceFile)).digest('hex');
assert.equal(sourceSha256,'67c7ed182bc8a38139da98e4d42d0350d2386de29a7e5f95095b4161dcab1bcf'.toLowerCase(),'pinned CommonMo TypeScript changed');
const sourceUrl=pathToFileURL(sourceFile);
const fixtureRoot=process.env.FOLIA_COMMON_SOURCE_FIXTURE_ROOT||path.join(root,'tests/math/fixtures/common-source');
const stubsUrl=pathToFileURL(path.join(fixtureRoot,'common-mo-source-stubs.mjs'));
const { CommonMoMixin }=await import(sourceUrl.href);

const DIRECTION={None:'',Vertical:'v',Horizontal:'h'};
const params={axis_height:.25,delimiterfactor:900,delimitershortfall:.3,
  extender_factor:.5,nulldelimiterspace:.12};
const recipe={dir:'v',sizes:[.5,1,1.5],variants:[0,1,2],schar:[0,0x239B,0x239B],
  stretch:[0x239B,0x239C,0x239D],stretchv:[0,0,0],HD:[.75,.25],HDW:[.75,.25,.8],
  fullExt:[1,2],c:0x28,ext:1,min:0};
const font={params,getDelimiter:()=>recipe,getSizeVariant:(c,i)=>['size0','size1','size2'][i],
  getRemappedChar:(kind,c)=>kind==='accent'&&c===0x5e?'x':undefined};
const attrs={
  values:new Map([['stretchy',true],['symmetric',false],['minsize',0]]),
  get(k){return this.values.get(k);}, isSet(k){return this.values.has(k);},
  hasExplicit(k){return this.values.has(k);}
};
const node={attributes:attrs,texClass:0,parent:null,w:.4,h:.7,d:.2,
  properties:new Map(),getProperty(k){return this.properties.get(k);},
  getText(){return '(';}, coreParent(){return {parent:null};},
  childIndex(){return 0;},childNodes:[],setTeXclass(){} };
const factory={font,jax:{options:{mathmlSpacing:false},currentLinebreakOptions:{lineleading:.2}},
  createMo(){return null;}};
const Mo=CommonMoMixin((await import(stubsUrl.href)).CommonWrapper);
const mo=new Mo(factory,node,null);
mo.node=node; mo.font=font; mo.stretch=recipe; mo.size=null;
// A detached synthetic radical's operator has no physical source parent.
// Source only inspects parent.isKind when the operator is an accent.
mo.isAccent=false;
assert.deepEqual(mo.remapChars([0x2B]),[0x2B]);
assert.equal(mo.getWH([]),0);
assert.equal(mo.getWH([2,1]),3);
attrs.values.set('symmetric',true);
assert.equal(mo.getWH([2,1]),3.5);
attrs.values.set('symmetric',false);
assert.deepEqual(mo.getBaseline([3,1],4,recipe),[3,1]);
assert.deepEqual(mo.getBaseline([],4,recipe),[3,1]);
assert.equal(mo.checkExtendedHeight(4.1,recipe),5);
assert.equal(mo.checkExtendedHeight(1.5,recipe),2);

// Fixed-size threshold and schar selection use the pinned Source method.
let childInvalidated=false;
mo.childNodes=[{invalidateBBox(){childInvalidated=true;}}];
mo.getStretchedVariant([1.2],true);
assert.equal(mo.size,2);
assert.equal(mo.variant,'size2');
assert.equal(mo.stretch.c,0x239B);
assert.equal(childInvalidated,true);

// Math-accent tolerance lowers a chosen nonzero size index by one.
node.properties.set('mathaccent',true);
mo.size=null;
mo.getStretchedVariant([1.2],false);
assert.equal(mo.size,1);
node.properties.delete('mathaccent');

// Source numeric size zero is falsy, so its selected schar overlay is retained
// on the next stretch request instead of refreshing the base delimiter.
mo.stretch={...recipe,schar:[0x239A,0x239B,0x239B]};
mo.size=null;
mo.getStretchedVariant([.2],true);
assert.equal(mo.size,0);
assert.equal(mo.stretch.c,0x239A);
mo.getStretchedVariant([.1],true);
assert.equal(mo.stretch.c,0x239A);

// Assembly uses fullExtent rounding and Source HDW metrics.
mo.stretch={...recipe,sizes:[.5],stretch:[0x239B,0x239C,0x239D]};
mo.size=null; mo.bbox=new (await import(stubsUrl.href)).BBox();
mo.getStretchedVariant([3,2]);
assert.equal(mo.size,-1);
assert.deepEqual([mo.bbox.h,mo.bbox.d,mo.bbox.w],[3,2,.8]);

// Source line-break selection reads the live linebreak attributes.
attrs.values.set('linebreak','newline'); attrs.values.set('linebreakstyle','before');
assert.equal(mo.getBreakStyle(),'before');
attrs.values.set('linebreakstyle','infixlinebreakstyle');
attrs.values.set('infixlinebreakstyle','duplicate');
assert.equal(mo.getBreakStyle(),'duplicate');
const evidence={package:packageJson.name,version:packageJson.version,sourceFile:path.relative(root,sourceFile).replaceAll('\\','/'),sourceSha256,cases:['nonaccent remap without a source parent','dimension and symmetry','baseline','full-extent rounding','fixed size and schar selection','size-zero truthiness','mathaccent size decrement','assembly metrics','linebreak style resolution'],status:'passed'};
const output=path.join(root,'build/math-port/common-source/mo.json');fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify(evidence,null,2)+'\n');
console.log(`Source CommonMo witness passed: ${evidence.cases.length} cases; ${path.relative(root,output)}`);
