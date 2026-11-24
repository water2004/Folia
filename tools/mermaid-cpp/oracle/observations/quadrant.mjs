// Browser capture only: shared by official fixture generation and Python validation.
export default function inspectQuadrant() {
 const svg=document.querySelector('svg'),v=svg.viewBox.baseVal;
 const style=e=>{const s=getComputedStyle(e);return {fill:s.fill,stroke:s.stroke,strokeWidth:s.strokeWidth};};
 const shape=e=>{const b=e.getBBox();return {x:b.x,y:b.y,width:b.width,height:b.height,...style(e)};};
 const text=e=>{const s=getComputedStyle(e),m=e.transform.baseVal.consolidate().matrix;return {text:e.textContent,...shape(e),fontSize:s.fontSize,fontFamily:s.fontFamily,transform:[m.a,m.b,m.c,m.d,m.e,m.f],anchor:e.getAttribute('text-anchor'),baseline:e.getAttribute('dominant-baseline')};};
 return {view:{x:v.x,y:v.y,width:v.width,height:v.height},
  rects:[...svg.querySelectorAll('.quadrants rect')].map(shape),
  lines:[...svg.querySelectorAll('.border line')].map(e=>({x1:e.x1.baseVal.value,y1:e.y1.baseVal.value,x2:e.x2.baseVal.value,y2:e.y2.baseVal.value,stroke:getComputedStyle(e).stroke,strokeWidth:getComputedStyle(e).strokeWidth})),
  circles:[...svg.querySelectorAll('.data-points circle')].map(e=>({cx:e.cx.baseVal.value,cy:e.cy.baseVal.value,r:e.r.baseVal.value,...style(e)})),
  texts:[...svg.querySelectorAll('g.main text')].map(text)};
};
