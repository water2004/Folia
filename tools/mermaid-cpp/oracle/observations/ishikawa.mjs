// Read-only browser capture shared by official generation and native observation.
export default function inspectIshikawa(){
 const svg=document.querySelector('svg'),view=svg.viewBox.baseVal,inv=svg.getScreenCTM().inverse();
 const box=e=>{const b=e.getBBox(),m=inv.multiply(e.getScreenCTM()),pts=[[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]].map(([x,y])=>({x:m.a*x+m.c*y+m.e,y:m.b*x+m.d*y+m.f})),xs=pts.map(p=>p.x),ys=pts.map(p=>p.y);return{x:Math.min(...xs),y:Math.min(...ys),width:Math.max(...xs)-Math.min(...xs),height:Math.max(...ys)-Math.min(...ys)};}; const lines=[...svg.querySelectorAll('.ishikawa-spine,.ishikawa-branch,.ishikawa-sub-branch')].map(e=>({cls:e.getAttribute('class'),x1:+e.getAttribute('x1'),y1:+e.getAttribute('y1'),x2:+e.getAttribute('x2'),y2:+e.getAttribute('y2'),marker:e.hasAttribute('marker-start')}));
 const labels=[...svg.querySelectorAll('text')].map(e=>({text:e.textContent.trim().replace(/\s+/g,' '),cls:e.getAttribute('class'),box:box(e),fontSize:getComputedStyle(e).fontSize,fill:getComputedStyle(e).fill}));
 const shapes=[...svg.querySelectorAll('.ishikawa-head,.ishikawa-label-box')].map(e=>({cls:e.getAttribute('class'),box:box(e),fill:getComputedStyle(e).fill,stroke:getComputedStyle(e).stroke}));
 return{view:{x:view.x,y:view.y,width:view.width,height:view.height},lines,labels,shapes};
};
