// Read-only browser capture shared by official reference generation and native observation.
export default function inspectSequence() {
 const root=document.querySelector('svg');
 const matrix=e=>root.getCTM().inverse().multiply(e.getCTM());
 const point=(e,x,y)=>new DOMPoint(x,y).matrixTransform(matrix(e));
 const box=e=>{const b=e.getBBox(),p=point(e,b.x,b.y),q=point(e,b.x+b.width,b.y+b.height);return {x:p.x,y:p.y,width:q.x-p.x,height:q.y-p.y};};
 const style=e=>{const s=getComputedStyle(e);return {fill:s.fill,stroke:s.stroke,strokeWidth:s.strokeWidth};};
 const numbers=[...root.querySelectorAll('line[marker-start*="sequencenumber"],circle.sequence-number-disc')].map(e=>{
  if(e.tagName==='circle') return {x:+e.getAttribute('cx'),y:+e.getAttribute('cy'),r:+e.getAttribute('r'),fill:getComputedStyle(e).fill};
  const id=e.getAttribute('marker-start').split('#').at(-1).slice(0,-1);const circle=root.querySelector('[id="'+id+'"] circle');
  return {x:+e.getAttribute('x1'),y:+e.getAttribute('y1'),r:+circle.getAttribute('r'),fill:getComputedStyle(circle).fill};
 });
 const actors=[...root.querySelectorAll('rect.actor')].map(e=>({id:e.getAttribute('name'),footer:e.classList.contains('actor-bottom'),...box(e),...style(e)})).sort((a,b)=>a.id.localeCompare(b.id)||Number(a.footer)-Number(b.footer)||a.y-b.y||a.x-b.x);
 const regions=[...root.querySelectorAll('.loopLine,.labelBox')].map(e=>({tag:e.tagName,...box(e),...style(e),dash:getComputedStyle(e).strokeDasharray,points:e.tagName==='polygon'?[...e.points].map(p=>[p.x,p.y]):null}));
 const backgrounds=[...root.querySelectorAll('rect.rect')].map(e=>({...box(e),...style(e)}));
 const notes=[...root.querySelectorAll('rect.note')].map(e=>({...box(e),...style(e)}));
 const activations=[...root.querySelectorAll('rect[class^=activation]')].map(e=>({...box(e),...style(e)})).sort((a,b)=>a.x-b.x||a.y-b.y);
 const figures=[...root.querySelectorAll('g.actor-man')].flatMap(g=>[...g.querySelectorAll('line,circle')].map(e=>({id:g.getAttribute('name'),footer:g.classList.contains('actor-bottom'),...box(e),...style(e)}))).sort((a,b)=>a.id.localeCompare(b.id)||Number(a.footer)-Number(b.footer)||a.y-b.y||a.x-b.x);
 const shapes=[...root.querySelectorAll('g.actor:not(.actor-man) path,g.actor:not(.actor-man) circle,g.actor:not(.actor-man) line')].map(e=>({tag:e.tagName,...box(e),...style(e),points:e.tagName==='path'?Array.from({length:21},(_,i)=>{const p=e.getPointAtLength(e.getTotalLength()*i/20),q=point(e,p.x,p.y);return [q.x,q.y];}):null})).sort((a,b)=>a.x-b.x||a.y-b.y||a.width-b.width);
 const lines=[...root.querySelectorAll('.actor-line')].map(e=>({...box(e),...style(e)})).sort((a,b)=>a.x-b.x);
 const texts=[...root.querySelectorAll('text')].map(e=>({text:e.textContent,...box(e),...style(e.querySelector('tspan')??e),size:getComputedStyle(e).fontSize,weight:getComputedStyle(e).fontWeight})).sort((a,b)=>a.text.localeCompare(b.text)||a.y-b.y);
 const signals=[...root.querySelectorAll('.messageLine0,.messageLine1')].map(e=>{
   const marker=side=>{const value=e.getAttribute('marker-'+side);if(!value)return null;const id=value.slice(value.lastIndexOf('#')+1,-1);const m=root.querySelector('[id="'+id+'"]');const path=m.querySelector('path');return {refX:m.getAttribute('refX'),refY:m.getAttribute('refY'),d:path.getAttribute('d'),...style(path)};};
   return {kind:e.classList.contains('messageLine1')?1:0,...style(e),points:Array.from({length:21},(_,i)=>{const p=e.getPointAtLength(e.getTotalLength()*i/20);return [p.x,p.y];}),start:marker('start'),end:marker('end')};
 });
 const v=root.viewBox.baseVal;return {numbers,backgrounds,regions,actors,notes,activations,figures,shapes,lines,texts,signals,view:{x:v.x,y:v.y,width:v.width,height:v.height}};
};
