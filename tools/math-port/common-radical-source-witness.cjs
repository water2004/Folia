'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const { execFileSync } = require('node:child_process');
function findRepositoryRoot(start=__dirname){
  for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current)){
    if(fs.existsSync(path.join(current,'tools/math-port/profiles/scope.json'))&&
       fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')))return current;
  }
  return null;
}
function repositoryRoot(){
  const args=process.argv.slice(2),index=args.indexOf('--repo-root');
  if(index<0&&args.length)throw Error('usage: node common-radical-source-witness.cjs [--repo-root <repository>]');
  if(index>=0&&(!args[index+1]||args.length!==2))
    throw Error('usage: node common-radical-source-witness.cjs [--repo-root <repository>]');
  const root=index>=0?path.resolve(args[index+1]):findRepositoryRoot();
  assert.ok(root,'pass --repo-root <repository> or run from within the repository');
  return root;
}
const repo = repositoryRoot();
const bundle = path.join(repo, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
assert.equal(JSON.parse(fs.readFileSync(path.join(repo,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))).version,
  '4.1.3','MathJax source package version');
const fixture=path.join(repo,'tests/math/fixtures/radical_source_pairs.txt');
const fixtureHash=digest(fixture);
assert.equal(fixtureHash,'ec17997df11343784b8e80651d971060739eba68b14d13f500e69df33285e5ab',
  `Existing 192-row radical fixture changed: ${fixtureHash}`);
const fixtureRows=fs.readFileSync(fixture,'utf8').split(/\r?\n/).filter(line=>line&&/^\d+\s/.test(line));
assert.equal(fixtureRows.length,192,'Existing radical fixture row count');
const sourceFiles = [
  'output/common/Wrappers/msqrt.ts',
  'output/common/Wrappers/mroot.ts',
  'output/svg/Wrappers/msqrt.ts',
  'output/svg/Wrappers/mroot.ts',
];
const sourceHashes = Object.fromEntries(sourceFiles.map(name => [name,
  digest(path.join(repo, '.reference/MathJax-src/ts', name))]));
const scope = JSON.parse(fs.readFileSync(path.join(repo, 'tools/math-port/profiles/scope.json'), 'utf8'));
for (const name of sourceFiles) {
  const lock = scope.files.find(file => file.source === `ts/${name}`);
  if (!lock || lock.source_sha256 !== sourceHashes[name]) throw new Error(`Source hash mismatch ${name}`);
}
const commonDir = path.join(bundle, 'output/common/Wrappers');
const CommonSqrt = require(path.join(commonDir, 'msqrt.js')).CommonMsqrtMixin(class {});
const CommonRoot = require(path.join(commonDir, 'mroot.js')).CommonMrootMixin(CommonSqrt);
const SourceBBox = require(path.join(bundle, 'util/BBox.js')).BBox;
require(path.join(bundle, 'output/svg.js'));
const SvgSqrt = require(path.join(bundle, 'output/svg/Wrappers/msqrt.js')).SvgMsqrt;
const SvgRoot = require(path.join(bundle, 'output/svg/Wrappers/mroot.js')).SvgMroot;
const cjsFiles = [
  'output/common/Wrappers/msqrt.js',
  'output/common/Wrappers/mroot.js',
  'output/svg/Wrappers/msqrt.js',
  'output/svg/Wrappers/mroot.js',
];
const cjsHashes = Object.fromEntries(cjsFiles.map(name => [name, digest(path.join(bundle, name))]));
const bboxHash=digest(path.join(bundle,'util/BBox.js'));
assert.equal(bboxHash,'792428ce9802696fe669aa8a13b1a92f5f2eaadd07e05fc28bd359acd8d2062d',
  `Pinned CJS hash mismatch util/BBox.js: ${bboxHash}`);
const pinnedCjsHashes={
  'output/common/Wrappers/msqrt.js':'0b97de1e4817c61b679c8d3cad9ae8542a3876c0edac5eed055030bb08dbd865',
  'output/common/Wrappers/mroot.js':'1f0c1f5b864beefae82f40e670afc0413ffb309787981ef5e334465815962f17',
  'output/svg/Wrappers/msqrt.js':'c4423e667287af40a89c550a712353a224c732e9c57137b3e54fe9608791adda',
  'output/svg/Wrappers/mroot.js':'938c1b73be3193a49610588f38a23e7634c5d922e9947ddcde3e9a256a4e3de4',
};
for(const name of cjsFiles) {
  const relative=name.replaceAll('\\','/');
  if(cjsHashes[relative]!==pinnedCjsHashes[relative]) throw new Error(`Pinned CJS hash mismatch ${relative}`);
}
const box = raw => {
  const b = new SourceBBox({w: raw.w, h: raw.h, d: raw.d});
  for (const key of ['scale','rscale','L','R','ic','oc','sk','dx','pwidth']) b[key] = raw[key];
  return b;
};
const plain = b => ({w:b.w,h:b.h,d:b.d,scale:b.scale,rscale:b.rscale,L:b.L,R:b.R,
  ic:b.ic,oc:b.oc,sk:b.sk,dx:b.dx,pwidth:b.pwidth});
const profiles = [
  {base:{w:1.2,h:.64,d:.18,scale:1.1,rscale:.85,L:.08,R:.03,ic:.04,oc:.02,sk:.01,dx:0,pwidth:'73%'},
   degree:{w:.3,h:.39,d:.11,scale:.8,rscale:1.3,L:.05,R:.02,ic:.01,oc:.02,sk:.03,dx:.04,pwidth:'21%'},
   surd:{w:.46,h:.83,d:.17,scale:1,rscale:.9,L:.07,R:.02,ic:.02,oc:.01,sk:.03,dx:.01}},
  {base:{w:.48,h:.23,d:.07,scale:.72,rscale:1.4,L:-.04,R:.13,ic:-.02,oc:.06,sk:-.03,dx:.08,pwidth:'40%'},
   degree:{w:.11,h:.2,d:.28,scale:1.2,rscale:.64,L:-.07,R:.09,ic:.04,oc:-.01,sk:.02,dx:-.04,pwidth:'9%'},
   surd:{w:.24,h:.29,d:.09,scale:.88,rscale:1.17,L:-.06,R:.11,ic:.03,oc:.02,sk:-.01,dx:.05}},
  {base:{w:1.7,h:1.11,d:.46,scale:1.8,rscale:.55,L:.21,R:-.08,ic:.07,oc:-.03,sk:.08,dx:-.06,pwidth:'118%'},
   degree:{w:.9,h:.84,d:-.13,scale:.6,rscale:1.52,L:.12,R:-.09,ic:.02,oc:.03,sk:-.02,dx:.09,pwidth:'33%'},
   surd:{w:.71,h:1.35,d:.3,scale:1.2,rscale:.73,L:.14,R:-.05,ic:.06,oc:-.02,sk:.04,dx:-.02}},
  {base:{w:-.21,h:-.17,d:.35,scale:1.3,rscale:.69,L:.16,R:.05,ic:-.08,oc:.04,sk:.07,dx:.02,pwidth:'17%'},
   degree:{w:-.28,h:.31,d:.22,scale:.95,rscale:.81,L:.06,R:.1,ic:-.03,oc:.05,sk:.01,dx:-.07,pwidth:'4%'},
   surd:{w:.33,h:.41,d:-.16,scale:.66,rscale:1.32,L:.05,R:.09,ic:-.04,oc:.06,sk:.02,dx:.07}},
];
const fonts = [
  {rule_thickness:.06,surd_height:.12,x_height:.43},
  {rule_thickness:.15,surd_height:.08,x_height:.27},
];
function observe(item, index) {
  const base = box(item.base), degree = item.degree ? box(item.degree) : null;
  const surd = box(item.surd);
  const commonClass = degree ? CommonRoot : CommonSqrt;
  const common = Object.create(commonClass.prototype);
  common.font = {params:item.font};
  common.node = {attributes:{get:name=>name === 'displaystyle' ? item.display : null}};
  common.childNodes = [{getOuterBBox:()=>base}, ...(degree ? [{getOuterBBox:()=>degree}] : [])];
  common.surd = {size:item.surdSize, getBBox:()=>surd, getStretchedVariant:(dims,exact)=>{common.stretchCall={dims,exact};}};
  common.setChildPWidths = recompute => {common.recompute = recompute;};
  const commonCombineRoot = common.combineRootBBox;
  common.combineRootBBox = function(...args) { this.observedMeasuredHeight=args[2]; return commonCombineRoot.apply(this,args); };
  common.getStretchedSurd();
  const request = {
    requiredSurdHeight:common.surdH,
    stretchHeight:common.stretchCall.dims[0],
    stretchDepth:common.stretchCall.dims[1],
    stretchExact:common.stretchCall.exact,
    rootWidth:common.rootWidth(),
  };
  const p = common.getPQ(surd)[0], q = common.getPQ(surd)[1];
  const measured = new SourceBBox();
  common.computeBBox(measured, false);

  const placements=[];
  const svgClass=degree ? SvgRoot : SvgSqrt;
  const svg=Object.create(svgClass.prototype);
  svg.font={params:item.font};
  Object.defineProperty(svg,'node',{value:common.node});
  svg.bbox={scale:item.wrapperScale};
  svg.childNodes=common.childNodes;
  svg.surd={size:item.surdSize,getBBox:()=>surd,toSVG:()=>{},place:(x,y)=>placements.push({role:'surd',x,y})};
  const wrap=(role,b)=>({getOuterBBox:()=>b,toSVG:()=>{},place:(x,y)=>placements.push({role,x,y})});
  const baseWrap=wrap('base',base);
  const rootWrap=degree ? wrap('degree',degree) : null;
  svg.childNodes=[baseWrap,...(rootWrap?[rootWrap]:[])];
  const rects=[];
  svg.standardSvgNodes=()=>[{},{}];
  Object.defineProperty(svg,'adaptor',{value:{append:(_parent,node)=>node}});
  svg.svg=(name,attributes={})=>{if(name==='rect')rects.push(attributes);return {name,attributes};};
  svg.fixed=value=>value;
  const sourceGetRootDimens = svg.getRootDimens;
  svg.getRootDimens = function(...args) { this.observedSvgRootDimensions=sourceGetRootDimens.apply(this,args); return this.observedSvgRootDimensions; };
  const svgAddRoot = svg.addRoot;
  svg.addRoot = function(...args) { this.observedPlacedHeight=args[3]; this.observedRootOffset = svgAddRoot.apply(this, args); return this.observedRootOffset; };
  svg.toSVG([]);
  const placedRoot=svg.observedSvgRootDimensions || [0,0,0];
  return {
    id:index,
    input:{base:item.base,degree:item.degree,font:item.font,display:item.display,
      wrapperScale:item.wrapperScale,surd:item.surd,surdSize:item.surdSize},
    expected:{request, bbox:plain(measured), p,q,
      measuredHeight:common.observedMeasuredHeight,
      placedSurdHeight:rects[0].height,
      placedHeight:svg.observedPlacedHeight,
      rootOffset:placedRoot[0],
      rootHeight:placedRoot[1],
      rootDx:placedRoot[2],
      placements,
      overbar:{x:rects[0].x,y:rects[0].y,width:rects[0].width,height:rects[0].height},
      bboxReset:{scale:measured.scale,rscale:measured.rscale,L:measured.L,R:measured.R,
        ic:measured.ic,oc:measured.oc,sk:measured.sk,dx:measured.dx,pwidth:measured.pwidth}},
  };
}
const cases=[];
let id=0;
for(const profile of profiles) for(const font of fonts) for(const display of [false,true])
  for(const wrapperScale of [.75,1,1.35]) for(const surdSize of [-1,2])
    for(const degree of [false,true]) {
      const item={base:profile.base,degree:degree?profile.degree:null,surd:profile.surd,
        font,display,wrapperScale,surdSize};
      cases.push(observe(item,++id));
    }
const output={schema:1,sourceVersion:'MathJax 4.1.3',sourceHashes,cjsHashes,
  methods:{common:['getPQ','getRootDimens','rootWidth','getStretchedSurd','computeBBox','rootHeight'],
    svg:['addRoot','toSVG']},cases};
assert.equal(cases.length,192,'Expected all radical Source cases');
const outputPath=path.join(repo,'build/math-port/common-source/radical-source.json');
fs.mkdirSync(path.dirname(outputPath),{recursive:true});
fs.writeFileSync(outputPath,JSON.stringify(output,null,2)+'\n');
function compareFixtureTable(current,tracked,family){
  const body=text=>text.split(/\r?\n/).filter(line=>line&&!line.startsWith('#'));
  const actual=body(fs.readFileSync(current,'utf8')),expected=body(fs.readFileSync(tracked,'utf8'));
  const fields=line=>line.match(/"(?:\\.|[^"\\])*"|[^\s]+/g)||[];
  const columns=fields(expected[0]);
  if(JSON.stringify(fields(actual[0]))!==JSON.stringify(columns))
    throw new Error(`${family} fixture header differs: generated=${actual[0]} tracked=${expected[0]}`);
  if(actual.length!==expected.length)throw new Error(`${family} row count differs: generated=${actual.length-1} tracked=${expected.length-1}`);
  for(let row=1;row<actual.length;row++){
    const a=fields(actual[row]),e=fields(expected[row]);
    if(a.length!==columns.length||e.length!==columns.length)throw new Error(`${family} row ${row} has wrong field count`);
    for(let col=0;col<columns.length;col++)if(a[col]!==e[col])
      throw new Error(`${family} case ${e[0]} column ${columns[col]} differs: generated=${a[col]} tracked=${e[col]}`);
  }
  return {rows:actual.length-1,columns:columns.length-1};
}
const tablePath=path.join(repo,'build/math-port/common-source/radical-current.txt');
execFileSync(process.execPath,[path.join(repo,'tools/math-port/layout-source-table.cjs'),'--family','radical','--input',outputPath,'--output',tablePath],{cwd:repo,stdio:'inherit'});
const comparison=compareFixtureTable(tablePath,fixture,'radical');
console.log(`observed and matched ${comparison.rows} radical cases × ${comparison.columns} fields; fixture ${fixtureHash}; BBox.js ${bboxHash}; evidence ${path.relative(repo,outputPath)}`);
