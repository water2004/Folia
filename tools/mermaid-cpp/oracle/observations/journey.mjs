// Browser capture only: shared by official fixture generation and Python validation.
export default function inspectJourney(){
 const svg=document.querySelector('svg'),v=svg.viewBox.baseVal;
 const world=e=>{const b=e.getBBox(),m=svg.getScreenCTM().inverse().multiply(e.getScreenCTM());return{x:b.x+m.e,y:b.y+m.f,width:b.width,height:b.height}};
 const font=e=>{const s=getComputedStyle(e);return{fill:s.fill||s.color,size:s.fontSize,family:s.fontFamily,weight:s.fontWeight}};
 const labels=[...svg.querySelectorAll('text')].filter(e=>!e.closest('switch')).map(e=>{
  if(!e.hasAttribute('data-html-label'))return{text:e.textContent,box:world(e),font:font(e)};
  // HTML Range measures the advance interval, not glyph overhang. The native
  // projection retains that layout box while painting actual SVG glyphs.
  const spans=[...e.querySelectorAll('tspan')],boxes=spans.map(span=>{const b=world(span),length=span.getComputedTextLength(),m=svg.getScreenCTM().inverse().multiply(span.getScreenCTM()),x=span.x.baseVal.getItem(0).value+m.e;return{x:x-length/2,y:b.y,width:length,height:b.height}});
  const left=Math.min(...boxes.map(b=>b.x)),top=Math.min(...boxes.map(b=>b.y)),right=Math.max(...boxes.map(b=>b.x+b.width)),bottom=Math.max(...boxes.map(b=>b.y+b.height));
  return{text:spans.map(span=>span.textContent.trim()).join(' '),box:{x:left,y:top,width:right-left,height:bottom-top},font:font(e)};
 });
 for(const e of svg.querySelectorAll('foreignObject .label')){const range=document.createRange();range.selectNodeContents(e);const b=range.getBoundingClientRect(),m=svg.getScreenCTM();const style=getComputedStyle(e);labels.push({text:e.textContent,box:{x:(b.x-m.e)/m.a,y:(b.y-m.f)/m.d,width:b.width/m.a,height:b.height/m.d},font:{fill:style.color,size:style.fontSize,family:style.fontFamily,weight:style.fontWeight}});}
 return{view:{x:v.x,y:v.y,width:v.width,height:v.height},shapes:[...svg.querySelectorAll('rect,circle,line,path')].filter(e=>!e.closest('defs')).map(e=>{const s=getComputedStyle(e);return{type:e.tagName,cls:e.getAttribute('class'),box:world(e),path:e.getAttribute('d'),paint:{fill:s.fill,stroke:s.stroke,width:s.strokeWidth,dash:s.strokeDasharray}}}),labels:labels.sort((a,b)=>a.text.localeCompare(b.text)||a.box.y-b.box.y||a.box.x-b.box.x)};
};
