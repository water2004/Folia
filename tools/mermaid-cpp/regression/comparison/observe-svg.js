// Capture-only browser DOM observation. Acceptance decisions live in Python.
function inspectVisibleSvg({viewport}) {
  const visiblePresentation = ({visibility, ancestors}) => !['hidden','collapse'].includes(visibility) && ancestors.every(style => style.display !== 'none' && Number(style.opacity || '1') !== 0);
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

