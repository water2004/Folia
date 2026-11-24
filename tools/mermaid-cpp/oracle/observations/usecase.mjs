// Read-only browser capture shared by official generation and native observation.
export default function inspectUsecase(){
 const svg=document.querySelector('svg'),v=svg.viewBox.baseVal,inv=svg.getScreenCTM().inverse();
 const box=(e,b=e.getBBox())=>{const m=inv.multiply(e.getScreenCTM()),p=[[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]].map(([x,y])=>({x:m.a*x+m.c*y+m.e,y:m.b*x+m.d*y+m.f})),xs=p.map(a=>a.x),ys=p.map(a=>a.y);return{x:Math.min(...xs),y:Math.min(...ys),width:Math.max(...xs)-Math.min(...xs),height:Math.max(...ys)-Math.min(...ys)};};
 const labels=[...svg.querySelectorAll('text,foreignObject')].filter(e=>e.textContent.trim()).map(e=>{
   const label=e.querySelector('span')??e,style=getComputedStyle(label);
   const runs=[];
   const walker=document.createTreeWalker(label,NodeFilter.SHOW_TEXT);
   let node;
   while(node=walker.nextNode()) {
     const {fontWeight:weight,fontStyle:slant}=getComputedStyle(node.parentElement);
     const last=runs.at(-1);
     if(last?.weight===weight&&last?.slant===slant) last.text+=node.textContent;
     else runs.push({text:node.textContent,weight,slant});
   }
   const measured=e.getAttribute('data-label-box');
   const [x,y,width,height]=measured?.split(' ').map(Number)??[];
   const text=measured?[...e.children].map(line=>line.textContent).join(' '):label.innerText??e.textContent;
   const glyphRects=[];
   if(e.tagName==='foreignObject') {
     const walker=document.createTreeWalker(label,NodeFilter.SHOW_TEXT);
     let node;
     while(node=walker.nextNode()) {
       if(!node.textContent.trim())continue;
       const range=document.createRange();range.selectNodeContents(node);
       for(const r of range.getClientRects()) {
         const a=new DOMPoint(r.x,r.y).matrixTransform(inv),b=new DOMPoint(r.right,r.bottom).matrixTransform(inv);
         glyphRects.push({x:a.x,y:a.y,width:b.x-a.x,height:b.y-a.y});
       }
     }
   } else if(measured) {
     for(const line of e.children) {
       const chars=line.getNumberOfChars();
       if(!chars)continue;
       const b=line.getBBox(),x=line.getStartPositionOfChar(0).x;
       glyphRects.push(box(line,{x,y:b.y,width:line.getEndPositionOfChar(chars-1).x-x,height:b.height}));
     }
   }
   let glyphBox=null;
   if(glyphRects.length) {
     const x=Math.min(...glyphRects.map(r=>r.x)),y=Math.min(...glyphRects.map(r=>r.y));
     glyphBox={x,y,width:Math.max(...glyphRects.map(r=>r.x+r.width))-x,
       height:Math.max(...glyphRects.map(r=>r.y+r.height))-y};
   }
   return {text:text.trim().replace(/\s+/g,' '),box:measured?box(e,{x,y,width,height}):box(e),size:style.fontSize,
     fill:e.tagName==='text'?style.fill:style.color,tag:e.tagName,portable:!!measured,glyphBox,
     runs:runs.map(run=>({...run,text:run.text.trim().replace(/\s+/g,' ')})).filter(run=>run.text)};
 });
 const paint=e=>{const style=getComputedStyle(e);return {fill:style.fill,stroke:style.stroke,
   strokeWidth:style.strokeWidth,shadow:style.filter!=='none'};};
 const roughPaths=e=>[...e.querySelectorAll('path')].map(path=>({d:path.getAttribute('d'),paint:paint(path)}));
 const nodes=[...svg.querySelectorAll('g.node,g.rough-node')].map(e=>{
   const kind=e.getAttribute('data-usecase-kind');
   const rough=e.classList.contains('rough-node');
   const shapes=rough?[]:kind==='actor'?[...e.querySelectorAll('.usecase-actor-glyph > path:not(.usecase-business-marker), .usecase-actor-glyph > circle, .usecase-actor-glyph > rect')]:
     kind==='note'?[]:[e.querySelector(':scope > ellipse, :scope > rect')].filter(Boolean);
   return {id:e.getAttribute('data-usecase-id'),box:box(e),shape:e.querySelector('ellipse,rect,path')?.tagName,
     paint:shapes.map(paint),paths:rough?roughPaths(e):[]};
 });
 const boundaries=[...svg.querySelectorAll('g.system-boundary')].map(e=>({id:e.getAttribute('data-usecase-id'),box:box(e),
   parts:[...e.querySelectorAll(':scope > .boundary-tab, :scope > .boundary-body')].map(shape=>({
     box:box(shape),paint:shape.tagName==='rect'?paint(shape):null,paths:roughPaths(shape),
   }))}));
 const marker=ref=>{const id=(ref||'').match(/#([^)]*)/)?.[1];if(!id)return null;const m=svg.querySelector(`#${CSS.escape(id)}`);if(!m)return null;const s=m.querySelector('path,circle,polygon');return{width:+m.getAttribute('markerWidth'),height:+m.getAttribute('markerHeight'),refX:+m.getAttribute('refX'),refY:+m.getAttribute('refY'),shape:s?.tagName,d:s?.getAttribute('d')||s?.getAttribute('points')||'',r:s?.getAttribute('r')||''};};
 const edges=[...svg.querySelectorAll('path[data-et="edge"]')].map(e=>({d:e.getAttribute('d'),stroke:getComputedStyle(e).stroke,dash:getComputedStyle(e).strokeDasharray,markerStart:marker(e.getAttribute('marker-start')),markerEnd:marker(e.getAttribute('marker-end'))}));
 return{view:{x:v.x,y:v.y,width:v.width,height:v.height},labels,nodes,boundaries,edges};
};
