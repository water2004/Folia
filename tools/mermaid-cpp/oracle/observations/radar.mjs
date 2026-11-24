// Browser capture only: shared by official fixture generation and Python validation.
export default function inspectRadar() {
 const svg=document.querySelector('svg'),v=svg.viewBox.baseVal;
 const matrix=e=>svg.getCTM().inverse().multiply(e.getCTM());
 const transform=e=>{const m=matrix(e);return [m.a,m.b,m.c,m.d,m.e,m.f];};
 const paint=e=>{const s=getComputedStyle(e);return {fill:s.fill,fillOpacity:s.fillOpacity,stroke:s.stroke,strokeWidth:s.strokeWidth};};
 const shapes=[...svg.querySelectorAll('circle,polygon,path,line,rect')].map(e=>{
  const data={kind:e.tagName,...paint(e),transform:transform(e)};
  if(e.tagName==='path'){data.samples=Array.from({length:25},(_,i)=>{const p=e.getPointAtLength(e.getTotalLength()*i/24);return [p.x,p.y];});}
  else if(e.tagName==='polygon')data.points=Array.from({length:e.points.numberOfItems},(_,i)=>{const p=e.points.getItem(i);return [p.x,p.y];});
  else for(const a of e.tagName==='circle'?['cx','cy','r']:e.tagName==='line'?['x1','y1','x2','y2']:['x','y','width','height'])data[a]=e[a].baseVal.value;
  return data;
 });
 const texts=[...svg.querySelectorAll('text')].map(e=>{const b=e.getBBox(),s=getComputedStyle(e);return {text:e.textContent,x:b.x,y:b.y,width:b.width,height:b.height,transform:transform(e),fill:s.fill,fontSize:s.fontSize,fontFamily:s.fontFamily,anchor:s.textAnchor,baseline:s.dominantBaseline};});
 return {view:{x:v.x,y:v.y,width:v.width,height:v.height},shapes,texts};
};
