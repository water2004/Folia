// Read-only browser capture. Acceptance decisions are implemented in Python.
export default extraShapes=>{
   const svg=document.querySelector('svg'),outer=svg.querySelector('g[font-family]');
   const strokePaint=shape=>{
    const stroke=getComputedStyle(shape).stroke;
    if(!stroke.startsWith('url(')) return stroke;
    const id=stroke.match(/#([^"')]+)/)?.[1],gradient=id&&svg.querySelector('[id="'+id+'"]');
    return gradient?JSON.stringify([...gradient.querySelectorAll('stop')].map(s=>[s.getAttribute('offset'),getComputedStyle(s).stopColor,getComputedStyle(s).stopOpacity])):stroke;
   };
   const position=(element,x,y)=>{
    const p=new DOMPoint(x,y).matrixTransform(svg.getScreenCTM().inverse().multiply(element.getScreenCTM()));
    return {x:p.x,y:p.y};
   };
   const fontStyle=text=>{const s=getComputedStyle(text);return {family:s.fontFamily,size:s.fontSize,weight:s.fontWeight,style:s.fontStyle};};
   const styledCharacters=label=>{
    const chars=[],walker=document.createTreeWalker(label,NodeFilter.SHOW_TEXT);
    while(walker.nextNode()) {
     const n=walker.currentNode,s=getComputedStyle(n.parentElement);
     for(const c of n.textContent) if(!/\s/.test(c)) chars.push([c,s.fontWeight,s.fontStyle]);
    }
    return chars;
   };
   const clusters=[...svg.querySelectorAll('g.cluster')].map(n=>{
    const shape=n.querySelector(':scope > rect')??n.firstElementChild,b=shape.getBBox(),label=n.querySelector('text'),lb=label.getBBox();
    const paths=[...shape.querySelectorAll('path')],outline=paths.at(-1)??shape;
    const sketchPaths=paths.length?paths.map(p=>({
     length:p.getTotalLength(),stroke:strokePaint(p),width:getComputedStyle(p).strokeWidth,
     samples:Array.from({length:37},(_,i)=>{const q=p.getPointAtLength(p.getTotalLength()*i/36);return position(p,q.x,q.y);}),
    })):null;
    return {id:n.dataset.node,...position(shape,b.x+b.width/2,b.y+b.height/2),width:b.width,height:b.height,label:label.textContent,
     title:position(label,lb.x+lb.width/2,lb.y+lb.height/2),fill:getComputedStyle(shape).fill,stroke:strokePaint(outline),
     lines:[...label.children].map(n=>n.textContent.trimEnd()),characters:styledCharacters(label),thickness:getComputedStyle(outline).strokeWidth,dash:getComputedStyle(outline).strokeDasharray,color:getComputedStyle(label).fill,font:fontStyle(label),sketchPaths};
   });
   const nodes=[...svg.querySelectorAll('g.node,g.rough-node')].map(n=>{
    const rough=n.matches('.rough-node');
    const shape=n.querySelector('.label-container'),b=shape.getBBox(),text=n.querySelector('text.nodeLabel'),t=text.getBBox();
    const local=n.getScreenCTM().inverse().multiply(shape.getScreenCTM());
    const corner=new DOMPoint(b.x,b.y).matrixTransform(local);
    const geometry=extraShapes?'path,polygon,circle,rect,ellipse,line,polyline':'path,polygon,circle';
    const paths=rough?[...n.querySelectorAll('path')]:shape.matches(geometry)?[shape]:[...shape.querySelectorAll(geometry)];
    const strokeShape=rough?(paths.at(-1)??shape):shape;
    const fillShape=paths.find(p=>getComputedStyle(p).fill!=='none')??shape;
    const sketchPaths=rough?paths.map(p=>({
     length:p.getTotalLength(),stroke:strokePaint(p),width:getComputedStyle(p).strokeWidth,
     fill:getComputedStyle(p).fill,opacity:getComputedStyle(p).strokeOpacity,
     samples:Array.from({length:37},(_,i)=>{const q=p.getPointAtLength(p.getTotalLength()*i/36);return position(p,q.x,q.y);}),
    })):null;
    const outline=paths.flatMap(p=>Array.from({length:extraShapes?1001:201},(_,i)=>{
     const a=p.getPointAtLength(p.getTotalLength()*i/(extraShapes?1000:200));return position(p,a.x,a.y);
    }));
    return {transform:n.getAttribute('transform'),width:b.width,height:b.height,label:text.textContent,
     labelCenter:position(text,t.x+t.width/2,t.y+t.height/2),lines:[...text.children].map(n=>n.textContent.trimEnd()),characters:styledCharacters(text),outline,
     dashSegments:paths.filter(p=>getComputedStyle(p).strokeDasharray.split(/[, ]+/).some(v=>parseFloat(v)>0))
       .map(p=>[getComputedStyle(p).strokeDasharray,(p.getAttribute('d')?.match(/[Mm]/g)??[]).length]),
     fill:getComputedStyle(rough?fillShape:shape).fill,stroke:strokePaint(strokeShape),
     invisibleStrokes:paths.filter(p=>getComputedStyle(p).strokeOpacity==='0').length,
     filter:getComputedStyle(shape).filter!=='none'||paths.some(p=>getComputedStyle(p).filter!=='none'),
     thickness:getComputedStyle(strokeShape).strokeWidth,dash:getComputedStyle(strokeShape).strokeDasharray,color:getComputedStyle(text).fill,font:fontStyle(text),sketchPaths,
     contained:t.x>=corner.x-1&&t.y>=corner.y-1&&t.x+t.width<=corner.x+b.width+1&&t.y+t.height<=corner.y+b.height+1};
   });
   const edges=[...svg.querySelectorAll('.flowchart-link')].map(p=>{
    const marker=side=>{
     const ref=p.getAttribute('marker-'+side);if(!ref) return null;
     const m=svg.querySelector('[id="'+ref.slice(ref.lastIndexOf('#')+1,-1)+'"]');
     const shape=m.querySelector('path,circle,polygon'),style=getComputedStyle(shape);
     return {attributes:Object.fromEntries(['viewBox','refX','refY','markerUnits','markerWidth','markerHeight','orient'].map(key=>[key,m.getAttribute(key)])),
      fill:style.fill,stroke:style.stroke,width:style.strokeWidth,dash:style.strokeDasharray,
      points:Array.from({length:21},(_,i)=>{const a=shape.getPointAtLength(shape.getTotalLength()*i/20);return {x:a.x,y:a.y};})};
    };
    const length=p.getTotalLength();return {id:p.dataset.edge,length,start:marker('start'),end:marker('end'),stroke:getComputedStyle(p).stroke,dash:getComputedStyle(p).strokeDasharray,thickness:getComputedStyle(p).strokeWidth,points:Array.from({length:7},(_,i)=>{const a=p.getPointAtLength(length*i/6);return {x:a.x,y:a.y};})};
   });
   const labels=[...svg.querySelectorAll('g.edgeLabel')].map(n=>{
    const b=n.querySelector('rect').getBBox(),text=n.querySelector('text');return {text:n.textContent,x:b.x+b.width/2,y:b.y+b.height/2,width:b.width,height:b.height,color:getComputedStyle(text).fill,font:fontStyle(text),lines:[...text.children].map(n=>n.textContent.trimEnd()),characters:styledCharacters(text)};
   });
   const bounds=outer.getBBox(),view=svg.viewBox.baseVal;
   const heading=svg.querySelector('.flowchartTitleText');
   const diagramTitle=heading?{text:heading.textContent,...position(heading,heading.x.baseVal[0].value,heading.y.baseVal[0].value),
    width:heading.getBBox().width,height:heading.getBBox().height,fill:getComputedStyle(heading).fill,size:getComputedStyle(heading).fontSize}:null;
   return {offset:outer.getAttribute('transform'),nodes,edges,labels,clusters,diagramTitle,
    bounds:{x:bounds.x,y:bounds.y,width:bounds.width,height:bounds.height},
    view:{x:view.x,y:view.y,width:view.width,height:view.height}};
  };
