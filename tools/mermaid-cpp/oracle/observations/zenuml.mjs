// Read-only browser capture. Acceptance decisions are implemented in Python.
export default function inspect(){
 const svg=document.querySelector('svg'),root=svg.getScreenCTM().inverse(),v=svg.viewBox.baseVal;
 const box=e=>{const b=e.getBBox(),m=root.multiply(e.getScreenCTM()),ps=[[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]].map(([x,y])=>new DOMPoint(x,y).matrixTransform(m));return [Math.min(...ps.map(p=>p.x)),Math.min(...ps.map(p=>p.y)),Math.max(...ps.map(p=>p.x))-Math.min(...ps.map(p=>p.x)),Math.max(...ps.map(p=>p.y))-Math.min(...ps.map(p=>p.y))];};
 const paints=[...svg.querySelectorAll('rect,path,circle,ellipse,line,polyline,polygon')].filter(e=>!e.closest('defs')).map(e=>{const s=getComputedStyle(e);return {type:e.tagName,box:box(e),paint:[s.fill,s.stroke,s.strokeWidth,s.strokeDasharray,s.strokeDashoffset,s.opacity,s.fillOpacity],path:e.getAttribute('d')??'',radius:Number(e.getAttribute('rx')??0)};});
 const labels=[...svg.querySelectorAll('text')].filter(e=>!e.closest('defs')).map(e=>{const s=getComputedStyle(e);return{text:e.textContent.trim(),box:box(e),font:[s.fontFamily,s.fontSize,s.fontWeight,s.fill],opacity:s.opacity};});
 paints.sort((a,b)=>a.type.localeCompare(b.type)||a.box[1]-b.box[1]||a.box[0]-b.box[0]);labels.sort((a,b)=>a.text.localeCompare(b.text)||a.box[1]-b.box[1]||a.box[0]-b.box[0]);return{view:[v.x,v.y,v.width,v.height],paints,labels};
};
