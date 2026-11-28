'use strict';
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');
const outputPath=(()=>{
  const index=process.argv.indexOf('--output');
  if(index<0||!process.argv[index+1])throw new Error('Usage: node layout-source-fraction.cjs --output <path>');
  return path.resolve(process.argv[index+1]);
})();
const packages=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
require(path.join(packages,'output/svg.js'));
const SvgMfrac=require(path.join(packages,'output/svg/Wrappers/mfrac.js')).SvgMfrac;
const CommonWrapper=require(path.join(packages,'output/common/Wrapper.js')).CommonWrapper;
const CommonMfracMixin=require(path.join(packages,'output/common/Wrappers/mfrac.js')).CommonMfracMixin;
const SourceBBox=require(path.join(packages,'util/BBox.js')).BBox;
const Common=CommonMfracMixin(class {});
const proto=Common.prototype;
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const sourceRoot=path.join(root,'.reference/MathJax-src/ts');
const sourceFiles=['output/common/Wrappers/mfrac.ts','output/svg/Wrappers/mfrac.ts'];
const sourceHashes=Object.fromEntries(sourceFiles.map(name=>[name,digest(path.join(sourceRoot,name))]));
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
for(const name of sourceFiles){
  const lock=scope.files.find(file=>file.source===`ts/${name}`);
  if(!lock||lock.source_sha256!==sourceHashes[name])throw new Error(`Source hash mismatch: ${name}`);
}
const cjsLocks={
  'output/common/Wrappers/mfrac.js':'4deba461704fca1492f630a9e0f712c6e02f4ab9abc9507290be05b2d4df06ea',
  'output/svg/Wrappers/mfrac.js':'55068c351d419d68b459eca706653c1a95ad6947cbce406eeaabddd627a0a9a6',
  'output/common/Wrapper.js':'9e21f886b91624d170ff89df0eecef31e789ffecda001abc3e5649a38419412b',
  'util/BBox.js':'792428ce9802696fe669aa8a13b1a92f5f2eaadd07e05fc28bd359acd8d2062d'
};
for(const [name,hash] of Object.entries(cjsLocks))if(digest(path.join(packages,name))!==hash)throw new Error(`Pinned CJS hash mismatch: ${name}`);
const runtimeHash='1495e23a9da0bf6b0e8b44d71f54271fa12e902c11444e4f18cdd749ad3b800e';
if(digest(path.join(packages,'output/svg.js'))!==runtimeHash)throw new Error('Pinned CJS hash mismatch: output/svg.js');
const bbox=(w,h,d,scale,rscale,L,R)=>Object.assign(new SourceBBox(),{w,h,d,scale,rscale,L,R});
const plainBox=box=>Object.fromEntries(['w','h','d','scale','rscale','L','R','pwidth','ic','oc','sk','dx'].map(k=>[k,box[k]]));
const font={axis_height:.25,num1:.68,num2:.39,num3:.52,denom1:.69,denom2:.35,rule_thickness:.06,nulldelimiterspace:.12};
const cases=[];
for(const display of [false,true])for(const scalePair of [[1,1],[.72,1.23]])
for(const alignPair of [['left','right'],['center','left'],['right','center']])
for(const withDelims of [false,true])for(const profile of [0,1,2,3]){
  const [ns,ds]=scalePair;
  const n=bbox([.46,1.35,.22,-.31][profile],[.31,1.05,.16,.27][profile],[.08,.24,-.39,.12][profile],ns,ns*.83,[.04,-.11,.17,-.08][profile],[.02,.19,-.03,.16][profile]);
  const d=bbox([1.08,.51,.18,-.48][profile],[.1,.23,-.4,-.22][profile],[.3,1.13,.13,.51][profile],ds,ds*1.14,[.08,.03,-.06,-.15][profile],[.05,-.02,.21,-.04][profile]);
  for(const kind of ['ruled','atop','bevelled']){
    const thickness=kind==='ruled'?[.06,.17,.025,-.03][profile]:kind==='atop'?0:.06;
    const input={kind,display,withDelims,lineThickness:thickness,align:alignPair,
      numerator:plainBox(n),denominator:plainBox(d),font:{...font}};
    let percentWidth=null;
    const attrs={
      getList(...keys){return Object.fromEntries(keys.map(key=>[key,
        key==='linethickness'?thickness:key==='bevelled'?kind==='bevelled':key==='displaystyle'?display:key==='scriptlevel'?0:undefined]));},
      get(key){return key==='bevelled'?kind==='bevelled':key==='linethickness'?thickness:undefined;}
    };
    const receiver=Object.assign(Object.create(proto),{
      childNodes:[{getOuterBBox:()=>n},{getOuterBBox:()=>d}],font:{params:font},pad:withDelims?0:font.nulldelimiterspace,
      node:{attributes:attrs,getProperty:key=>key==='withDelims'?withDelims:undefined},
      isDisplay:()=>display,length2em:value=>Number(value),getBBox:()=>out,
      setChildPWidths(_recompute,width){percentWidth=width;}
    });
    const out=new SourceBBox();
    const slash=kind==='bevelled'?bbox(.42,.76,.08,1,.91,.03,.01):null;
    if(slash)receiver.bevel={getOuterBBox:()=>slash};
    const combineTrace=[];
    const originalCombine=SourceBBox.prototype.combine;
    SourceBBox.prototype.combine=function(box,x=0,y=0){combineTrace.push({box,x,y});return originalCombine.call(this,box,x,y);};
    try{proto.computeBBox.call(receiver,out,false);}finally{SourceBBox.prototype.combine=originalCombine;}
    const sourceWrap=[proto.getWrapWidth.call(receiver,0),proto.getWrapWidth.call(receiver,1)];
    const expected={bbox:plainBox(out),contentWidth:percentWidth,wrapWidths:sourceWrap};
    if(kind==='bevelled'){
      const data=proto.getBevelData.call(receiver,display);
      const denominatorCombine=combineTrace.at(-1);
      expected.H=data.H;expected.delta=data.delta;expected.u=data.u;expected.v=data.v;
      expected.bboxDenominatorOrigin={x:denominatorCombine.x,y:denominatorCombine.y};
      expected.slash=plainBox(slash);
    }
    expected.svg=sourceSvgObservation(kind,display,thickness,alignPair,withDelims,font,n,d,slash);
    cases.push({input,expected});
  }
}
function sourceSvgObservation(kind,display,thickness,align,withDelims,font,n,d,slash){
  const calls=[];
  const child=(box,index)=>({getOuterBBox:()=>box,toSVG(){},place(x,y){calls.push({child:index,x,y});}});
  const children=[child(n,0),child(d,1)];
  const bevel=slash&&{getOuterBBox:()=>slash,toSVG(){},place(x,y){calls.push({child:'slash',x,y});}};
  const attrs={getList(...keys){return Object.fromEntries(keys.map(key=>[key,key==='numalign'?align[0]:key==='denomalign'?align[1]:undefined]));}};
  const receiver={node:{attributes:attrs,getProperty:key=>key==='withDelims'?withDelims:undefined},childNodes:children,
    font:{params:font},dom:[{}],adaptor:{append(){}},getAlignX:CommonWrapper.prototype.getAlignX,
    getTUV(displayValue,t){return proto.getTUV.call({font:this.font},displayValue,t);},
    getUVQ(displayValue){return proto.getUVQ.call({childNodes:this.childNodes,font:this.font},displayValue);},
    getBevelData(displayValue){return proto.getBevelData.call({childNodes:this.childNodes,font:this.font},displayValue);},
    bevel,svg(_kind,attributes){calls.push({rule:attributes});return attributes;},fixed(value){return value;}};
  if(kind==='bevelled')SvgMfrac.prototype.makeBevelled.call(receiver,display);
  else if(kind==='atop')SvgMfrac.prototype.makeAtop.call(receiver,display);
  else SvgMfrac.prototype.makeFraction.call(receiver,display,thickness);
  return calls;
}
const output={schema:1,sourceVersion:'MathJax 4.1.3',sourceHashes,cjsLocks,
  sourceMethods:{common:['computeBBox','getTUV','getUVQ','getBevelData','getFractionBBox','getAtopBBox','getBevelledBBox','getWrapWidth'],svg:['makeFraction','makeAtop','makeBevelled']},cases};
fs.writeFileSync(outputPath,JSON.stringify(output,null,2)+'\n');
process.stdout.write(`observed ${cases.length} paired Source inputs; wrote ${outputPath}\n`);
