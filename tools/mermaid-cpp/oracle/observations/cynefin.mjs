// Browser capture only: shared by official fixture generation and Python validation.
export default function inspectCynefin() {
  const svg=document.querySelector('svg'),view=svg.viewBox.baseVal;
  const inverse=svg.getScreenCTM().inverse();
  const box=e=>{const b=e.getBBox(),m=inverse.multiply(e.getScreenCTM());const points=[[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]].map(([x,y])=>({x:m.a*x+m.c*y+m.e,y:m.b*x+m.d*y+m.f}));const left=Math.min(...points.map(p=>p.x)),top=Math.min(...points.map(p=>p.y)),right=Math.max(...points.map(p=>p.x)),bottom=Math.max(...points.map(p=>p.y));return{x:left,y:top,width:right-left,height:bottom-top}};
  const paths=[...svg.querySelectorAll('path')].filter(e=>!e.closest('defs')).map(e=>({
    cls:e.getAttribute('class')??'',d:e.getAttribute('d')??'',fill:getComputedStyle(e).fill,
    stroke:getComputedStyle(e).stroke,width:parseFloat(getComputedStyle(e).strokeWidth),
    box:box(e)
  }));
  const rects=[...svg.querySelectorAll('rect')].map(e=>({cls:e.getAttribute('class')??'',...box(e),fill:getComputedStyle(e).fill}));
  const labels=[...svg.querySelectorAll('text')].filter(e=>!e.closest('defs')).map(e=>{const b=box(e),s=getComputedStyle(e);return{text:e.textContent,...b,fontSize:s.fontSize,weight:s.fontWeight,fill:s.fill}});
  return{view:{x:view.x,y:view.y,width:view.width,height:view.height},paths,rects,labels};
};
