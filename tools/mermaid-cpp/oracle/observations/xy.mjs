// Browser capture only: shared by official fixture generation and Python validation.
export default function inspectXY() {
 const svg=document.querySelector('svg'),v=svg.viewBox.baseVal;
 return {view:{x:v.x,y:v.y,width:v.width,height:v.height},elements:[...svg.querySelectorAll('g.main rect,g.main path,g.main text')].map(e=>{
  const b=e.getBBox(),s=getComputedStyle(e),m=e.getCTM();
  return {type:e.tagName,groups:[e.parentElement.parentElement.getAttribute('class'),e.parentElement.getAttribute('class')],
   attrs:Object.fromEntries([...e.attributes].map(a=>[a.name,a.value])),text:e.textContent,
   box:{x:b.x,y:b.y,width:b.width,height:b.height},transform:[m.a,m.b,m.c,m.d,m.e,m.f],
   paint:{fill:s.fill,stroke:s.stroke,strokeWidth:s.strokeWidth,fontSize:s.fontSize,fontFamily:s.fontFamily}};
 })};
};
