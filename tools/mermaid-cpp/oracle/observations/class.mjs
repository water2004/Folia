// Read-only browser capture shared by official reference generation and native observation.
export default function inspectClass() {
 const root=document.querySelector('svg');
 const matrix=e=>root.getCTM().inverse().multiply(e.getCTM());
 const point=(e,x,y)=>new DOMPoint(x,y).matrixTransform(matrix(e));
 const box=e=>{const b=e.getBBox(),p=point(e,b.x,b.y),q=point(e,b.x+b.width,b.y+b.height);return {x:p.x,y:p.y,width:q.x-p.x,height:q.y-p.y};};
 const paint=e=>{const s=getComputedStyle(e);return {fill:s.fill,stroke:s.stroke,strokeWidth:s.strokeWidth,strokeDasharray:s.strokeDasharray};};
 const textStyles=e=>{
   const walker=document.createTreeWalker(e,NodeFilter.SHOW_TEXT),runs=[];
   while(walker.nextNode()) {
     const node=walker.currentNode;if(!node.textContent)continue;
     const s=getComputedStyle(node.parentElement),last=runs.at(-1);
     if(last&&last.weight===s.fontWeight&&last.italic===s.fontStyle)last.text+=node.textContent;
     else runs.push({text:node.textContent,weight:s.fontWeight,italic:s.fontStyle});
   }
   return runs;
 };
 const textBox=e=>{
   // HTML Range width is the inline advance. SVG getBBox also includes glyph
   // overhang; compare advance origin/width and retain vertical glyph bounds.
   if(e.tagName!=='foreignObject') {
     const lines=[...e.querySelectorAll(':scope > tspan')];
     if(lines.length>1) {
       const spans=lines.filter(l=>l.textContent).map(l=>{const p=l.getStartPositionOfChar(0),o=point(l,p.x,p.y);return {x:o.x,width:l.getComputedTextLength()};});
       const x=Math.min(...spans.map(s=>s.x)),right=Math.max(...spans.map(s=>s.x+s.width));
       return {...box(e),x,width:right-x};
     }
     const first=e.getStartPositionOfChar(0),origin=point(e,first.x,first.y);
     return {...box(e),x:origin.x,width:e.getComputedTextLength()};
   }
   const walker=document.createTreeWalker(e,NodeFilter.SHOW_TEXT);const texts=[];while(walker.nextNode())if(walker.currentNode.textContent)texts.push(walker.currentNode);
   if(!texts.length)return box(e);
   const range=document.createRange();range.setStart(texts[0],0);range.setEnd(texts.at(-1),texts.at(-1).textContent.length);
   const b=range.getBoundingClientRect(),m=root.getScreenCTM().inverse(),p=new DOMPoint(b.x,b.y).matrixTransform(m),q=new DOMPoint(b.right,b.bottom).matrixTransform(m);
   return {x:p.x,y:p.y,width:q.x-p.x,height:q.y-p.y};
 };
 const nodes=[...root.querySelectorAll('g.node')].map(g=>{
   const outer=g.querySelector('.outer-path')??g.querySelector('rect');
   const id=g.getAttribute('data-class-id')??g.id.replace(/^class\d+-/,'').replace(/^.*classId-/,'').replace(/^.*-(note\d+)$/,'$1').replace(/-\d+$/,'');
   const groups=[...g.querySelectorAll('.text')].map(t=>({kind:[...t.classList].find(c=>c.endsWith('-group')),
     labels:[...t.querySelectorAll('.label')].map(l=>{
       const fo=l.querySelector('foreignObject'),text=l.querySelector('text'),element=fo??text;
       const b=textBox(element);
       const s=getComputedStyle(fo?.querySelector('span')??text);
       return {text:element.textContent,runs:textStyles(element),...b,color:fo?s.color:s.fill,size:s.fontSize,weight:s.fontWeight,italic:s.fontStyle,decoration:fo?getComputedStyle(l).textDecorationLine:s.textDecorationLine};
     })}));
   if(id.startsWith('interface')&&!groups.length) {
     const e=g.querySelector('foreignObject'),style=getComputedStyle(e.querySelector('span'));
     groups.push({kind:'interface-group',labels:[{text:e.textContent,runs:textStyles(e),...textBox(e),
       color:style.color,size:style.fontSize,weight:style.fontWeight,italic:style.fontStyle,decoration:style.textDecorationLine}]});
   }
   const note=g.querySelector('.noteLabel');
   if(note) {
     const fo=note.querySelector('foreignObject');
     const elements=[];
     if(fo) {const walker=document.createTreeWalker(fo,NodeFilter.SHOW_TEXT);while(walker.nextNode())if(walker.currentNode.textContent)elements.push(walker.currentNode);}
     else elements.push(...[...note.querySelectorAll('text')].flatMap(t=>t.querySelector('tspan')?[...t.querySelectorAll('tspan')].filter(s=>!s.querySelector('tspan')&&s.textContent):[t]));
     groups.push({kind:'note-group',labels:elements.map(e=>{
       let b;
       if(fo) {
         const range=document.createRange();range.selectNodeContents(e);
         const r=range.getBoundingClientRect(),m=root.getScreenCTM().inverse(),p=new DOMPoint(r.x,r.y).matrixTransform(m),q=new DOMPoint(r.right,r.bottom).matrixTransform(m);
         b={x:p.x,y:p.y,width:q.x-p.x,height:q.y-p.y};
       } else b=textBox(e);
       const s=getComputedStyle(fo?e.parentElement:e);
       return {text:e.textContent,...b,color:fo?s.color:s.fill,size:s.fontSize,weight:s.fontWeight,italic:s.fontStyle,decoration:s.textDecorationLine};
     })});
   }
   return {id,opacity:getComputedStyle(outer).opacity,...box(outer),...paint(outer.querySelector('path')??outer),groups,
     dividers:[...g.querySelectorAll('.divider')].map(e=>({...box(e),...paint(e.querySelector('path')??e)}))};
 });
 const edges=[...root.querySelectorAll('path.relation')].map(e=>{
   const marker=side=>{const url=e.getAttribute('marker-'+side);if(!url)return null;const id=url.slice(url.lastIndexOf('#')+1,-1),m=root.querySelector('[id="'+id+'"]'),p=m.querySelector('path,circle');return {refX:m.getAttribute('refX'),refY:m.getAttribute('refY'),d:p.getAttribute('d'),shape:p.tagName,cx:p.getAttribute('cx'),cy:p.getAttribute('cy'),r:p.getAttribute('r'),...paint(p)};};
   return {...paint(e),dash:getComputedStyle(e).strokeDasharray,
     points:Array.from({length:21},(_,i)=>{const p=e.getPointAtLength(e.getTotalLength()*i/20),q=point(e,p.x,p.y);return [q.x,q.y];}),start:marker('start'),end:marker('end')};
 });
 const labels=[...root.querySelectorAll('.edgeLabels foreignObject,.edgeLabels text')].filter(e=>e.textContent).map(e=>({text:e.textContent,runs:textStyles(e),...textBox(e)}));
 const v=root.viewBox.baseVal;
 const titleElement=root.querySelector('.classDiagramTitleText');
 const title=titleElement?{text:titleElement.textContent,...textBox(titleElement),size:getComputedStyle(titleElement).fontSize,fill:getComputedStyle(titleElement).fill,baseline:Number(titleElement.getAttribute('y'))}:null;
 const clusters=[...root.querySelectorAll('g.cluster')].map(g=>{
   const e=g.querySelector('.cluster-label foreignObject,.cluster-label text');
   const style=getComputedStyle(e.tagName==='foreignObject'?e.querySelector('span'):e);
   return {id:g.getAttribute('data-namespace')??g.id.replace(/^class\d+-/,''),...box(g.querySelector('rect')),...paint(g.querySelector('rect')),
     label:{text:e.textContent,...textBox(e),color:e.tagName==='foreignObject'?style.color:style.fill,size:style.fontSize}};
 });
 return {clusters,nodes,edges,labels,title,view:{x:v.x,y:v.y,width:v.width,height:v.height}};
};
