// Full-corpus SVG acceptance. Font measurements and screenshots come from a
// real browser; there is no DOM geometry shim or global image-error allowance.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,existsSync,statSync,mkdirSync} from 'node:fs';
import {resolve,dirname,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import {JSDOM} from '../oracle/node_modules/jsdom/lib/api.js';
import {loadCorpus,hash,upstream} from './corpus.mjs';

export const geometryTolerance = 1.5;
const rasterScale = 2;
const channelTolerance = 2; // Rounding of 8-bit antialias coverage, not paint colors.

function near(actual, expected, location, tolerance=geometryTolerance) {
  assert(Number.isFinite(actual) && Number.isFinite(expected) &&
    Math.abs(actual-expected)<=tolerance,
  location+': '+actual+' != '+expected+' (tolerance '+tolerance+')');
}
function closeObject(actual, expected, location) {
  if(typeof expected==='number')near(actual,expected,location);
  else if(Array.isArray(expected)){
    assert.equal(actual.length,expected.length,location+' count');
    expected.forEach((value,index)=>closeObject(actual[index],value,location+'.'+index));
  }else if(expected&&typeof expected==='object'){
    for(const [key,value]of Object.entries(expected)){
      if(['fontSize','strokeWidth','opacity','fillOpacity','strokeOpacity'].includes(key))
        near(actual[key],value,location+'.'+key,.001);
      else closeObject(actual[key],value,location+'.'+key);
    }
  }else assert.equal(actual,expected,location);
}

// Packet and Info do not ask the DOM for text bounds. Every visible primitive
// is a rectangle or text node, so their SVGs can be checked without a browser.
// This adapter does not claim that jsdom provides font or raster measurements.
function measurementFreeSnapshot(source,family) {
  assert(['packet','info'].includes(family),'Unsupported measurement-free family');
  const dom=new JSDOM('<!doctype html><html><body>'+source+'</body></html>');
  try {
    const {document}=dom.window,svg=document.querySelector('svg');
    assert(svg,'Missing '+family+' SVG root');
    // jsdom does not apply SVG presentation attributes for visibility and
    // display. Register them as zero-specificity author declarations, before
    // the actual stylesheets. CSS and inline styles retain their normal
    // precedence, and jsdom then resolves inherited visibility itself.
    const presentation=document.createElement('style'),rules=[];
    for(const [index,element]of [svg,...svg.querySelectorAll('*')].entries()){
      const properties=document.createElement('span').style;
      for(const name of ['display','visibility'])if(element.hasAttribute(name))
        properties.setProperty(name,element.getAttribute(name));
      if(!properties.cssText)continue;
      element.setAttribute('data-folia-snapshot-presentation',String(index));
      rules.push(':where([data-folia-snapshot-presentation="'+index+'"]) {'+properties.cssText+'}');
    }
    presentation.textContent=rules.join('\n');document.head.append(presentation);
    // jsdom does not register SVG <style> elements in its CSSOM. Keep the
    // exact stylesheet text and selectors, but register them through HTML
    // style elements so computed presentation is actually checked.
    for(const source of svg.querySelectorAll('style')){
      const style=document.createElement('style');
      style.textContent=source.textContent;document.head.append(style);
    }
    const viewBox=svg.getAttribute('viewBox');
    const view=viewBox===null?null:viewBox.trim().split(/[ ,]+/).map(Number);
    if(family==='packet'||view!==null){
      assert.equal(view?.length,4,family+' viewBox');
      assert(view.every(Number.isFinite),'Non-finite '+family+' viewport');
    }
    // Info intentionally has no viewBox or height. Compare its actual sizing
    // contract instead of inventing the intrinsic dimensions of a browser.
    const sizing=family==='info'?{
      width:svg.getAttribute('width'),height:svg.getAttribute('height'),
      maxWidth:dom.window.getComputedStyle(svg).maxWidth,
      maxHeight:dom.window.getComputedStyle(svg).maxHeight,
      preserveAspectRatio:svg.getAttribute('preserveAspectRatio')??'xMidYMid meet'
    }:undefined;
    const color=value=>{
      if(value==='none')return value;
      const element=document.createElement('span');
      element.style.color=value;
      assert(element.style.color,'Unsupported '+family+' paint '+value);
      document.body.append(element);
      const result=dom.window.getComputedStyle(element).color;
      element.remove();return result;
    };
    const property=(element,name,fallback)=>{
      for(let current=element;current&&current.nodeType===1;current=current.parentElement){
        const computed=dom.window.getComputedStyle(current).getPropertyValue(name);
        const value=computed||current.getAttribute(name);
        if(value&&value!=='inherit')return value.trim();
        if(current===svg)break;
      }
      return fallback;
    };
    const numeric=(element,name,fallback=0)=>{
      const value=element.getAttribute(name);
      const number=value===null?fallback:Number(value);
      assert(Number.isFinite(number),'Invalid '+family+' '+name);return number;
    };
    const elements=[];
    for(const element of svg.querySelectorAll('*')){
      if(element.closest('defs,style,title,desc'))continue;
      const tag=element.localName;
      if(tag==='g'){
        assert(!element.getAttribute('transform'),'Unexpected transformed '+family+' group');
        continue;
      }
      assert(tag==='text'||(family==='packet'&&tag==='rect'),'Unexpected '+family+' primitive '+tag);
      assert(!element.getAttribute('transform'),'Unexpected transformed '+family+' primitive');
      const result={tag,x:numeric(element,'x'),y:numeric(element,'y'),
        fill:color(property(element,'fill','black')),
        stroke:color(property(element,'stroke','none')),
        strokeWidth:Number.parseFloat(property(element,'stroke-width','1')),
        fillOpacity:Number.parseFloat(property(element,'fill-opacity','1')),
        strokeOpacity:Number.parseFloat(property(element,'stroke-opacity','1')),
        strokeDasharray:property(element,'stroke-dasharray','none'),
        strokeLinecap:property(element,'stroke-linecap','butt'),
        strokeLinejoin:property(element,'stroke-linejoin','miter'),opacity:1,displayed:true,
        visibility:dom.window.getComputedStyle(element).visibility||'visible'};
      for(let node=element;node;node=node.parentElement){
        // Descendants can override inherited visibility, but cannot restore
        // painting below an ancestor with display:none.
        if(dom.window.getComputedStyle(node).display==='none')result.displayed=false;
        const opacity=dom.window.getComputedStyle(node).opacity||node.getAttribute('opacity')||'1';
        result.opacity*=Number(opacity);
        for(const name of ['transform','filter','clip-path','mask']){
          const value=dom.window.getComputedStyle(node).getPropertyValue(name)||node.getAttribute(name);
          assert(!value||value==='none','Unsupported '+family+' presentation '+name+': '+value);
        }
        if(node===svg)break;
      }
      if(tag==='rect'){
        result.width=numeric(element,'width');result.height=numeric(element,'height');
        result.rx=numeric(element,'rx');result.ry=numeric(element,'ry');
      }else{
        result.text=element.textContent;
        result.fontSize=Number.parseFloat(property(element,'font-size','16px'));
        result.fontFamily=property(element,'font-family','').replace(/["']/g,'').replace(/\s*,\s*/g,',').toLowerCase();
        result.fontWeight=property(element,'font-weight','400').replace('normal','400').replace('bold','700');
        result.fontStyle=property(element,'font-style','normal');
        result.anchor=property(element,'text-anchor','start');
        result.baseline=property(element,'dominant-baseline','auto');
      }
      elements.push(result);
    }
    return {view,...(sizing?{sizing}:{}),elements,title:svg.querySelector(':scope > title')?.textContent??'',
      description:svg.querySelector(':scope > desc')?.textContent??''};
  }finally{dom.window.close();}
}
export const packetSnapshot=source=>measurementFreeSnapshot(source,'packet');
export const infoSnapshot=source=>measurementFreeSnapshot(source,'info');
export function createFontconfigResolver() {
  const cache=new Map();
  return element=>{
    const key=JSON.stringify([element.fontFamily,element.fontWeight,element.fontStyle]);
    if(cache.has(key))return cache.get(key);
    const weight=Number(element.fontWeight);
    assert(Number.isFinite(weight)&&weight>=100&&weight<=900,'Unsupported font weight');
    const cssWeights=[100,200,300,400,500,600,700,800,900];
    const fcWeights=[0,40,50,80,100,180,200,205,210];
    const index=cssWeights.findIndex(value=>value>=weight);
    let resolvedWeight=fcWeights[index];
    if(index>0&&cssWeights[index]!==weight)
      resolvedWeight=fcWeights[index-1]+(weight-cssWeights[index-1])/100*(fcWeights[index]-fcWeights[index-1]);
    const slant={normal:0,italic:100,oblique:110}[element.fontStyle];
    assert(slant!==undefined,'Unsupported font style');
    const pattern=element.fontFamily.replaceAll('\\','\\\\').replaceAll(':','\\:')+
      ':weight='+resolvedWeight+':slant='+slant;
    const [file,family,style,fontIndex]=execFileSync('fc-match',
      ['--format=%{file}\n%{family[0]}\n%{style[0]}\n%{index}\n',pattern],{encoding:'utf8'}).trimEnd().split('\n');
    assert(file&&family&&existsSync(file),'fontconfig did not resolve an installed font');
    const result={requestedFamily:element.fontFamily,weight:element.fontWeight,
      requestedStyle:element.fontStyle,family,style,file,index:fontIndex,sha256:hash(readFileSync(file))};
    cache.set(key,result);return result;
  };
}

function compareMeasurementFree(official,native,family,{resolveFont}={}) {
  const expected=measurementFreeSnapshot(official,family),actual=measurementFreeSnapshot(native,family);
  const fonts=new Map();
  if(resolveFont)for(const snapshot of [expected,actual])for(const element of snapshot.elements){
    if(element.tag!=='text')continue;
    const font=resolveFont(element);
    assert(font.file&&font.sha256,'Missing installed font evidence');
    fonts.set(JSON.stringify(font),font);
    element.fontFamily=font.sha256+':'+font.index;
  }
  closeObject(actual,expected,family);
  return {method:'complete-'+family+'-svg',elements:expected.elements.length,
    textNodes:expected.elements.filter(item=>item.tag==='text').length,rasterCompared:false,
    fontResolution:resolveFont?'fontconfig':'literal-css-family',fonts:[...fonts.values()]};
}
export const comparePacket=(official,native,options)=>compareMeasurementFree(official,native,'packet',options);
export const compareInfo=(official,native,options)=>compareMeasurementFree(official,native,'info',options);

export function compareText(actual,expected,tolerance=geometryTolerance) {
  assert.equal(actual.length,expected.length,'Visible grapheme count');
  const candidates=new Map();
  actual.forEach((item,index)=>{
    const group=candidates.get(item.text)??[];
    group.push({item,index});candidates.set(item.text,group);
  });
  const options=expected.map(target=>{
    const group=candidates.get(target.text)??[];
    const matching=[];
    let best=Infinity;
    for(const candidate of group){
      const distance=Math.max(...['x','y','width','height'].map(key=>Math.abs(candidate.item[key]-target[key])));
      best=Math.min(best,distance);
      if(distance<=tolerance&&Math.abs(candidate.item.size-target.size)<=.05&&
          ['weight','style','paint'].every(key=>candidate.item[key]===target[key]))
        matching.push({index:candidate.index,distance});
    }
    assert(matching.length,'Missing, displaced or restyled grapheme '+JSON.stringify(target.text)+
      ' at '+target.x+','+target.y+'; nearest geometry difference '+best);
    return matching.sort((a,b)=>a.distance-b.distance);
  });
  // Nearby duplicate glyphs require a complete one-to-one assignment. Greedy
  // matching can consume the only candidate for the next glyph and falsely
  // fail an otherwise valid subpixel shift.
  const owners=new Map(),assignments=new Map();
  const assign=(target,seen)=>{
    for(const option of options[target]){
      if(seen.has(option.index))continue;
      seen.add(option.index);
      if(!owners.has(option.index)||assign(owners.get(option.index),seen)){
        owners.set(option.index,target);assignments.set(target,option);return true;
      }
    }
    return false;
  };
  expected.forEach((target,index)=>assert(assign(index,new Set()),
    'No one-to-one visible glyph correspondence near '+target.x+','+target.y));
  const maximumDifference=Math.max(0,...[...assignments.values()].map(item=>item.distance));
  // A CSS family list and its resolved installed family can describe the same
  // glyphs. Actual glyph bounds and the raster gate verify that resolution.
  return {graphemes:expected.length,maximumDifference};
}

// This function is self-contained so the same implementation runs in Node
// self-tests and in a browser after decoding real Playwright screenshots.
export function comparePixels(input) {
  const {actual,expected,scale,tolerance,colorTolerance}=input;
  const width=Math.max(actual.width,expected.width),height=Math.max(actual.height,expected.height);
  const radius=tolerance*scale,integerRadius=Math.ceil(radius);
  const offsets=[];
  for(let y=-integerRadius;y<=integerRadius;++y)for(let x=-integerRadius;x<=integerRadius;++x)
    if(x*x+y*y<=radius*radius)offsets.push([x,y]);
  offsets.sort((a,b)=>a[0]*a[0]+a[1]*a[1]-b[0]*b[0]-b[1]*b[1]);
  const compose=image=>{
    const output=new Uint8Array(width*height*4);
    for(let index=0;index<output.length;index+=4)
      output[index]=output[index+1]=output[index+2]=255;
    for(let y=0;y<image.height;++y)for(let x=0;x<image.width;++x){
      const from=(y*image.width+x)*4,to=(y*width+x)*4,alpha=image.data[from+3]/255;
      for(let c=0;c<3;++c)output[to+c]=Math.round(image.data[from+c]*alpha+255*(1-alpha));
      output[to+3]=image.data[from+3];
    }
    return output;
  };
  const a=compose(actual),e=compose(expected),examples=[];
  const different=(source,index,target,other)=>Math.max(
    Math.abs(source[index]-target[other]),Math.abs(source[index+1]-target[other+1]),
    Math.abs(source[index+2]-target[other+2]),
    Math.abs(source[index+3]-target[other+3]))>colorTolerance;
  let missingPixels=0,extraPixels=0;
  const check=(source,target,direction)=>{
    let failures=0;
    for(let y=0;y<height;++y)for(let x=0;x<width;++x){
      const index=(y*width+x)*4;
      if(source[index+3]===0)continue;
      if(!different(source,index,target,index))continue;
      let matched=false;
      for(const [dx,dy]of offsets){
        const xx=x+dx,yy=y+dy;
        if(xx>=0&&yy>=0&&xx<width&&yy<height&&!different(source,index,target,(yy*width+xx)*4)){
          matched=true;break;
        }
      }
      if(!matched){
        ++failures;
        if(examples.length<32)examples.push({direction,x:x/scale,y:y/scale});
      }
    }
    return failures;
  };
  missingPixels=check(e,a,'missing');extraPixels=check(a,e,'extra');
  const integral=(data,background)=>{
    const sum=new Float64Array((width+1)*(height+1));
    for(let y=0;y<height;++y){
      let row=0;
      for(let x=0;x<width;++x){
        const index=(y*width+x)*4;
        // Check local ink against both white and black. A white stroke on a
        // transparent/black background must be as significant as a black one.
        row+=(background==='white'?
          255-Math.min(data[index],data[index+1],data[index+2]):
          Math.max(data[index],data[index+1],data[index+2])-255+data[index+3])/255;
        sum[(y+1)*(width+1)+x+1]=sum[y*(width+1)+x+1]+row;
      }
    }
    return sum;
  };
  const area=(sum,x0,y0,x1,y1)=>{
    x0=Math.max(0,x0);y0=Math.max(0,y0);x1=Math.min(width,x1);y1=Math.min(height,y1);
    if(x1<=x0||y1<=y0)return 0;
    const stride=width+1;
    return sum[y1*stride+x1]-sum[y0*stride+x1]-sum[y1*stride+x0]+sum[y0*stride+x0];
  };
  let failedTiles=0;
  const tile=16*scale,halo=Math.ceil(radius);
  for(const background of ['white','black']){
    const aInk=integral(a,background),eInk=integral(e,background);
    for(let y=0;y<height;y+=tile)for(let x=0;x<width;x+=tile){
      const inner=[x+halo,y+halo,x+tile-halo,y+tile-halo];
      const outer=[x-halo,y-halo,x+tile+halo,y+tile+halo];
      const roundingAllowance=(tile*tile)*colorTolerance/255;
      if(area(eInk,...inner)>area(aInk,...outer)+roundingAllowance||
          area(aInk,...inner)>area(eInk,...outer)+roundingAllowance){
        ++failedTiles;
        if(examples.length<32)examples.push({direction:'local-ink-'+background,x:x/scale,y:y/scale});
      }
    }
  }
  return {ok:missingPixels===0&&extraPixels===0&&failedTiles===0,
    missingPixels,extraPixels,failedTiles,examples,width,height};
}

// Info is the sole renderer whose root intentionally has no viewBox/height.
// Its natural viewport must come from a real browser's layout, never from a
// guessed intrinsic size. An explicitly present invalid viewBox still fails.
export function captureViewport({family,viewBoxPresent,view,bounds}) {
  const intrinsic=family==='info'&&!viewBoxPresent;
  const size=intrinsic?bounds:view;
  if((!intrinsic&&!viewBoxPresent)||!size||
      !(size.width>0&&size.height>0&&Number.isFinite(size.width)&&Number.isFinite(size.height)))
    throw new Error('Invalid SVG viewport');
  return {width:size.width,height:size.height,intrinsic};
}

export function visiblePresentation({visibility,ancestors}) {
  // Visibility is inherited and can be overridden by the descendant. Display
  // and group opacity instead suppress painting for the entire subtree.
  return !['hidden','collapse'].includes(visibility)&&
    ancestors.every(style=>style.display!=='none'&&Number(style.opacity||'1')!==0);
}

function inspectVisibleSvg({viewport,visibilityDecision}) {
  const visiblePresentation=(0,eval)('('+visibilityDecision+')');
  const svg=document.querySelector('svg');
  if(!svg)throw new Error('Missing SVG root');
  const bounds=svg.getBoundingClientRect();
  const scaleX=viewport.intrinsic?1:bounds.width/viewport.width;
  const scaleY=viewport.intrinsic?1:bounds.height/viewport.height;
  const visible=element=>{
    const ancestors=[];
    for(let node=element;node&&node.nodeType===1;node=node.parentElement){
      const style=getComputedStyle(node);
      ancestors.push({display:style.display,opacity:style.opacity});
      if(node===svg)break;
    }
    return visiblePresentation({visibility:getComputedStyle(element).visibility,ancestors})&&
      !element.closest('defs,style,script,metadata,title,desc,clipPath,mask,marker');
  };
  const texts=[];
  const walker=document.createTreeWalker(svg,NodeFilter.SHOW_TEXT);
  const segmenter=new Intl.Segmenter('en',{granularity:'grapheme'});
  for(let node=walker.nextNode();node;node=walker.nextNode()){
    const parent=node.parentElement;
    if(!parent||!visible(parent)||!parent.closest('text,foreignObject'))continue;
    const style=getComputedStyle(parent),isHtml=parent.namespaceURI!=='http://www.w3.org/2000/svg';
    const fill=isHtml?style.color:style.fill;
    const stroke=isHtml?'none':style.stroke;
    if(fill==='none'&&stroke==='none')continue;
    for(const part of segmenter.segment(node.nodeValue)){
      if(/^\s+$/.test(part.segment))continue;
      const range=document.createRange();
      range.setStart(node,part.index);range.setEnd(node,part.index+part.segment.length);
      const box=range.getBoundingClientRect();
      if(!(box.width>0&&box.height>0))continue;
      texts.push({text:part.segment.normalize('NFC'),
        x:(box.x-bounds.x)/scaleX,y:(box.y-bounds.y)/scaleY,
        width:box.width/scaleX,height:box.height/scaleY,
        size:parseFloat(style.fontSize),weight:style.fontWeight,style:style.fontStyle,
        paint:fill+'|'+stroke+'|'+(stroke==='none'?'0':style.strokeWidth),
        family:style.fontFamily});
    }
  }
  const paints=new Set();
  const addPaint=(value,opacity)=>{
    if(!value||value==='none'||Number(opacity)===0||value==='rgba(0, 0, 0, 0)')return;
    if(value.startsWith('url(')){
      // Resource ids are intentionally not stable between implementations.
      // Check the actual stop paints here; the raster gate checks the gradient
      // or pattern geometry and every resulting pixel.
      const match=/#([^"')]+)["']?\)$/.exec(value);
      const resource=match&&document.getElementById(match[1]);
      if(!resource)throw new Error('Unresolved SVG paint resource '+value);
      for(const stop of resource.querySelectorAll('stop')){
        const style=getComputedStyle(stop);
        addPaint(style.stopColor,String(Number(style.stopOpacity)*Number(opacity)));
      }
    }else paints.add(value+'|'+opacity);
  };
  for(const element of svg.querySelectorAll('path,rect,circle,ellipse,line,polygon,polyline,text')){
    if(!visible(element))continue;
    const box=element.getBoundingClientRect();
    if(box.width===0&&box.height===0)continue;
    const style=getComputedStyle(element);
    for(const [property,opacity]of [['fill','fillOpacity'],['stroke','strokeOpacity']]){
      const value=style[property];
      addPaint(value,style[opacity]);
    }
  }
  for(const item of texts)addPaint(item.paint.split('|')[0],'1');
  return {width:viewport.width,height:viewport.height,texts,paints:[...paints].sort(),
    accessibility:{title:svg.querySelector(':scope > title')?.textContent??'',
      description:svg.querySelector(':scope > desc')?.textContent??''}};
}

async function renderCapture(page,source,family) {
  await page.setContent('<!doctype html><html><head><meta charset="utf-8">'+
    '<style>html,body{margin:0;padding:0;background:transparent}body>svg{vertical-align:top}</style>'+
    '</head><body>'+source+'</body></html>',{waitUntil:'load'});
  const viewport=await page.evaluate(async({family,decide})=>{
    const captureViewport=(0,eval)('('+decide+')');
    const svg=document.querySelector('svg');
    if(!svg)throw new Error('Missing SVG root');
    const currentViewport=()=>captureViewport({family,viewBoxPresent:svg.hasAttribute('viewBox'),
      view:svg.viewBox.baseVal,bounds:svg.getBoundingClientRect()});
    const initial=currentViewport();
    if(!initial.intrinsic){
      svg.style.width=initial.width+'px';svg.style.height=initial.height+'px';
      svg.style.maxWidth='none';svg.style.maxHeight='none';
      document.body.style.width=Math.ceil(initial.width)+'px';
      document.body.style.height=Math.ceil(initial.height)+'px';
    }
    await document.fonts.ready;
    await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));
    return currentViewport();
  },{family,decide:captureViewport.toString()});
  const snapshot=await page.evaluate(inspectVisibleSvg,
    {viewport,visibilityDecision:visiblePresentation.toString()});
  const png=await page.screenshot({omitBackground:true,captureBeyondViewport:true,
    clip:{x:0,y:0,width:Math.ceil(viewport.width),height:Math.ceil(viewport.height)}});
  return {snapshot,png};
}

async function compareBrowser(page,official,native,family) {
  if(family==='info')
    closeObject(infoSnapshot(native).sizing,infoSnapshot(official).sizing,'Info sizing contract');
  const expected=await renderCapture(page,official,family),actual=await renderCapture(page,native,family);
  near(actual.snapshot.width,expected.snapshot.width,'Viewport width');
  near(actual.snapshot.height,expected.snapshot.height,'Viewport height');
  const text=compareText(actual.snapshot.texts,expected.snapshot.texts);
  assert.deepEqual(actual.snapshot.paints,expected.snapshot.paints,'Visible paint palette');
  assert.deepEqual(actual.snapshot.accessibility,expected.snapshot.accessibility,'SVG accessibility');
  const pixels=await page.evaluate(async input=>{
    const decode=async data=>{
      const image=new Image();
      image.src='data:image/png;base64,'+data;
      await image.decode();
      const canvas=document.createElement('canvas');
      canvas.width=image.width;canvas.height=image.height;
      const context=canvas.getContext('2d',{willReadFrequently:true});
      context.drawImage(image,0,0);
      return {width:image.width,height:image.height,
        data:context.getImageData(0,0,image.width,image.height).data};
    };
    const compare=(0,eval)('('+input.compare+')');
    return compare({actual:await decode(input.actual),expected:await decode(input.expected),
      scale:input.scale,tolerance:input.tolerance,colorTolerance:input.colorTolerance});
  },{actual:actual.png.toString('base64'),expected:expected.png.toString('base64'),
    compare:comparePixels.toString(),scale:rasterScale,tolerance:geometryTolerance,
    colorTolerance:channelTolerance});
  assert(pixels.ok,'Raster differs: '+JSON.stringify(pixels));
  return {method:'browser-graphemes-and-local-raster',rasterCompared:true,text,pixels};
}

function readReport(argument) {
  const input=resolve(argument),path=statSync(input).isDirectory()?join(input,'report.json'):input;
  const report=JSON.parse(readFileSync(path,'utf8'));
  assert(Array.isArray(report.results),'Report lacks per-case results: '+path);
  const rows=new Map();
  for(const item of report.results){
    assert(!rows.has(item.id),'Duplicate report case '+item.id);
    rows.set(item.id,item);
  }
  return {report,path,directory:dirname(path),rows};
}
function caseSource(input,item,corpusHash) {
  const snapshot=input.report.corpusHashes?.[item.family+'.json']??
    input.report.coverage?.find(row=>row.family===item.family)?.sha256;
  assert.equal(snapshot,corpusHash,'Corpus snapshot mismatch for '+item.family+' in '+input.path);
  const row=input.rows.get(item.id);
  assert(row,'Missing case '+item.id+' in '+input.path);
  assert.equal(row.family,item.family,'Wrong report family for '+item.id);
  assert.equal(row.ok,true,'SVG generation failed for '+item.id+': '+(row.error??'unknown error'));
  assert(typeof row.svgSha256==='string','Report has no SVG evidence for '+item.id);
  const filename=row.svgFile??(item.id+'.svg');
  const path=resolve(input.directory,filename);
  assert(existsSync(path),'Missing SVG file '+path);
  const bytes=readFileSync(path);
  assert.equal(hash(bytes),row.svgSha256,'SVG evidence changed for '+item.id);
  return bytes.toString('utf8');
}

export function selfTest() {
  const glyph={text:'A',x:2,y:3,width:6,height:9,size:12,weight:'400',style:'normal',
    paint:'rgb(0, 0, 0)|none|0',family:'Arial'};
  compareText([{...glyph,x:3}],[glyph]);
  assert.throws(()=>compareText([],[glyph]));
  assert.throws(()=>compareText([{...glyph,text:'B'}],[glyph]));
  assert.throws(()=>compareText([{...glyph,weight:'700'}],[glyph]));
  assert.throws(()=>compareText([{...glyph,x:4}],[glyph]));
  compareText([{...glyph,x:1},{...glyph,x:-1}],[{...glyph,x:0},{...glyph,x:2}]);
  const blank=()=>{
    const data=new Uint8ClampedArray(64*64*4);data.fill(255);
    return {width:64,height:64,data};
  };
  const rectangle=(image,x,y,width,height,color=[0,0,0])=>{
    for(let yy=y;yy<y+height;++yy)for(let xx=x;xx<x+width;++xx){
      const index=(yy*image.width+xx)*4;
      for(let channel=0;channel<3;++channel)image.data[index+channel]=color[channel];
      image.data[index+3]=255;
    }
    return image;
  };
  const compare=(a,e)=>comparePixels({actual:a,expected:e,scale:2,
    tolerance:geometryTolerance,colorTolerance:channelTolerance});
  const expected=rectangle(blank(),8,8,18,18);
  assert(compare(expected,expected).ok,'Identical raster');
  assert(compare(rectangle(blank(),10,8,18,18),expected).ok,'One CSS pixel translation');
  assert(!compare(blank(),expected).ok,'Missing region must fail');
  assert(!compare(rectangle(blank(),8,8,18,18,[200,0,0]),expected).ok,'Changed paint must fail');
  const dotted=rectangle(rectangle(blank(),8,8,18,18),45,45,2,2);
  assert(!compare(expected,dotted).ok,'A small missing isolated mark must fail');
  const parallel=rectangle(rectangle(blank(),10,6,1,22),12,6,1,22);
  const missing=rectangle(blank(),10,6,1,22);
  const local=compare(missing,parallel);
  assert(!local.ok&&local.failedTiles>0,'Neighboring stroke cannot hide local missing ink');
  const transparent=blank();
  for(let index=3;index<transparent.data.length;index+=4)transparent.data[index]=0;
  assert(!compare(transparent,blank()).ok,'Opaque white is not transparent');
  const black=()=>rectangle(blank(),0,0,64,64);
  const whiteParallel=rectangle(rectangle(black(),10,6,1,22,[255,255,255]),12,6,1,22,[255,255,255]);
  const whiteMissing=rectangle(black(),10,6,1,22,[255,255,255]);
  const whiteLocal=compare(whiteMissing,whiteParallel);
  assert(!whiteLocal.ok&&whiteLocal.failedTiles>0,'Neighboring white stroke cannot hide missing ink');
  const packet='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 40" font-family="Arial">'+
    '<style>.packetBlock{fill:#efefef;stroke:black;stroke-width:1}.packetLabel{fill:black;font-size:12px}</style>'+
    '<g><rect x="1" y="2" width="98" height="30" class="packetBlock"/>'+
    '<text x="50" y="17" class="packetLabel" text-anchor="middle" dominant-baseline="middle">A</text></g></svg>';
  comparePacket(packet,packet);
  assert.throws(()=>comparePacket(packet,packet.replace('>A<','>B<')));
  assert.throws(()=>comparePacket(packet,packet.replace('width="98"','width="80"')));
  assert.throws(()=>comparePacket(packet,packet.replace('fill:#efefef','fill:#eeeeee')));
  assert.throws(()=>comparePacket(packet,packet.replace('font-size:12px','font-size:13px')));
  assert.throws(()=>comparePacket(packet,packet.replace('font-size:12px','font-size:12px;font-style:italic')));
  assert.throws(()=>comparePacket(packet,packet.replace('viewBox=','opacity="0.5" viewBox=')));
  assert.throws(()=>comparePacket(packet,packet.replace('<g>','<title>Unexpected title</title><g>')));
  assert.throws(()=>comparePacket(packet,packet.replace('<g>','<g display="none">')));
  assert.throws(()=>comparePacket(packet,packet.replace('<rect ','<rect visibility="hidden" ')));
  assert.throws(()=>comparePacket(packet,packet.replace('<g>',
    '<style>g{visibility:hidden}</style><g>')));
  comparePacket(packet,packet.replace('<g>',
    '<g visibility="hidden">').replace('<rect ','<rect visibility="visible" ')
    .replace('<text ','<text visibility="visible" '));
  const info='<svg xmlns="http://www.w3.org/2000/svg" width="100%" style="max-width:400px">'+
    '<text x="100" y="40" font-family="Arial" font-size="32" fill="#333" text-anchor="middle">v12.2.0</text></svg>';
  compareInfo(info,info);
  assert.throws(()=>compareInfo(info,info.replace('v12.2.0','v12.2.1')));
  assert.throws(()=>compareInfo(info,info.replace('x="100"','x="102"')));
  assert.throws(()=>compareInfo(info,info.replace('max-width:400px','max-width:450px')));
  assert.throws(()=>compareInfo(info,info.replace('font-size="32"','font-size="31"')));
  assert.throws(()=>compareInfo(info,info.replace('fill="#333"','fill="#334"')));
  assert.throws(()=>compareInfo(info,info.replace('font-family="Arial"','font-family="serif"')));
  assert.throws(()=>compareInfo(info,info.replace('width="100%"','viewBox="0 0 400 100" width="100%"')));
  assert.throws(()=>compareInfo(info,info.replace('<text ','<text display="none" ')));
  assert.throws(()=>compareInfo(info,info.replace('<text ','<text visibility="hidden" ')));
  assert.throws(()=>compareInfo(info,info.replace('<text ','<text style="visibility:hidden" ')));
  assert.throws(()=>compareInfo(info,info.replace('<svg ','<svg visibility="hidden" ')));
  assert.throws(()=>compareInfo(info,info.replace('<svg ','<svg display="none" ')
    .replace('<text ','<text style="display:block" ')));
  assert.throws(()=>compareInfo(info,info.replace('<text ',
    '<style>svg{visibility:hidden}</style><text ')));
  compareInfo(info,info.replace('<svg ','<svg visibility="hidden" ')
    .replace('<text ','<text visibility="visible" '));
  compareInfo(info,info.replace('<text ',
    '<style>svg{visibility:hidden}text{visibility:visible}</style><text '));
  compareInfo(info,info.replace('<text ',
    '<style>text{visibility:visible}</style><text visibility="hidden" '));
  compareInfo(info,info.replace('<text ',
    '<style>text{display:block}</style><text display="none" '));
  compareInfo(info,info.replace('<text ','<text visibility="hidden" style="visibility:visible" '));
  assert.throws(()=>compareInfo(info,info.replace('<text ',
    '<style>svg{display:none}text{display:block}</style><text ')));
  compareInfo(info,info.replace('max-width:400px','max-width:400px;visibility:hidden')
    .replace('<text ','<text style="visibility:visible" '));
  const explicit={family:'flowchart',viewBoxPresent:true,
    view:{width:250,height:180},bounds:{width:500,height:360}};
  assert.deepEqual(captureViewport(explicit),{width:250,height:180,intrinsic:false});
  const intrinsic={...explicit,family:'info',viewBoxPresent:false,
    view:{width:0,height:0},bounds:{width:400,height:150}};
  assert.deepEqual(captureViewport(intrinsic),{width:400,height:150,intrinsic:true});
  assert.throws(()=>captureViewport({...intrinsic,family:'packet'}));
  assert.throws(()=>captureViewport({...intrinsic,viewBoxPresent:true}));
  assert.throws(()=>captureViewport({...intrinsic,bounds:{width:NaN,height:150}}));
  assert.throws(()=>captureViewport({...intrinsic,bounds:{width:400,height:0}}));
  assert.deepEqual(captureViewport({...explicit,family:'info'}),
    {width:250,height:180,intrinsic:false});
  const visible={visibility:'visible',ancestors:[{display:'inline',opacity:'1'}]};
  assert(visiblePresentation(visible));
  assert(!visiblePresentation({...visible,visibility:'hidden'}));
  assert(!visiblePresentation({...visible,visibility:'collapse'}));
  assert(!visiblePresentation({...visible,ancestors:[...visible.ancestors,{display:'none',opacity:'1'}]}));
  assert(!visiblePresentation({...visible,ancestors:[...visible.ancestors,{display:'block',opacity:'0'}]}));
  assert(visiblePresentation({...visible,
    ancestors:[...visible.ancestors,{display:'block',opacity:'.5',visibility:'hidden'}]}));
  return {textChecks:6,rasterChecks:8,packetChecks:12,infoChecks:21,viewportChecks:7,visibilityChecks:6};
}

async function main() {
  const args=process.argv.slice(2);
  if(args.length===1&&args[0]==='--self-test'){
    console.log(JSON.stringify({stage:'comparison-logic-self-test',...selfTest(),
      officialSvgCompared:false}));return;
  }
  const options={};
  for(let i=0;i<args.length;i+=2){
    assert(['--official','--native','--output','--browser'].includes(args[i])&&args[i+1],
      'Usage: compare-svg.mjs --official directory/report.json --native directory/report.json [--output file] [--browser executable]');
    assert(!options[args[i]],'Duplicate option '+args[i]);options[args[i]]=args[i+1];
  }
  assert(options['--official']&&options['--native'],'Both official and native reports are required');
  const corpus=loadCorpus(),official=readReport(options['--official']),native=readReport(options['--native']);
  assert.deepEqual(official.report.upstream,upstream,'Official Mermaid version/commit mismatch');
  assert.equal(official.report.stage,'official-svg','An official SVG capture report is required');
  const cases=new Set(corpus.records.map(item=>item.id));
  for(const input of [official,native])for(const id of input.rows.keys())
    assert(cases.has(id),'Report contains an unknown or removed corpus case '+id);
  const results=[];
  const resolveFont=process.platform==='linux'?createFontconfigResolver():undefined;
  let browser,page,browserError;
  try {
    for(const item of corpus.records){
      try {
        const snapshot=corpus.summary.find(row=>row.family===item.family).sha256;
        const expected=caseSource(official,item,snapshot),actual=caseSource(native,item,snapshot);
        const capture=official.rows.get(item.id);
        const measurementFree=item.family==='packet'||item.family==='info';
        if(measurementFree)assert(capture.browser===true||
          (capture.renderer==='official-mermaid-jsdom'&&capture.geometryMeasurements==='none-required'),
          item.family+' capture must use the real official renderer without synthetic measurements');
        else assert(capture.browser===true&&capture.renderer==='official-browser',
          'Official layout was not captured in a real browser; matched-measurement evidence is a separate layer');
        let comparison;
        if(measurementFree&&resolveFont)
          comparison=compareMeasurementFree(expected,actual,item.family,{resolveFont});
        else{
          if(browserError)throw new Error(browserError);
          if(!browser){
            try{
              const {chromium}=await import('../oracle/node_modules/playwright-core/index.mjs');
              browser=await chromium.launch({headless:true,
                executablePath:options['--browser']??process.env.FOLIA_BROWSER});
              page=await browser.newPage({viewport:{width:1600,height:900},
                deviceScaleFactor:rasterScale});
            }catch(error){browserError='Real browser unavailable: '+String(error.message);throw new Error(browserError);}
          }
          comparison=await compareBrowser(page,expected,actual,item.family);
        }
        results.push({id:item.id,family:item.family,ok:true,...comparison});
      }catch(error){
        results.push({id:item.id,family:item.family,ok:false,error:String(error.message)});
      }
      const result=results.at(-1);
      console.log((result.ok?'PASS ':'FAIL ')+result.id+(result.ok?'':': '+result.error));
    }
  }finally{if(browser)await browser.close();}
  const report={schema:1,stage:'complete-svg-comparison',upstream,
    geometryTolerance,rasterScale,channelRoundingTolerance:channelTolerance,
    coverage:corpus.summary,corpusHashes:Object.fromEntries(corpus.summary.map(row=>[row.family+'.json',row.sha256])),
    results,total:results.length,passed:results.filter(row=>row.ok).length,
    browserCompared:results.filter(row=>row.ok&&row.rasterCompared).length,
    officialSvgCompared:results.length===corpus.records.length&&results.every(row=>row.ok)};
  const output=resolve(options['--output']??join(native.directory,'comparison.json'));
  mkdirSync(dirname(output),{recursive:true});writeFileSync(output,JSON.stringify(report,null,2)+'\n');
  console.log(report.passed+'/'+report.total+' complete SVG comparisons passed');
  if(!report.officialSvgCompared)process.exitCode=1;
}

if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url))
  await main().catch(error=>{console.error(error.stack);process.exitCode=1;});
