// Read-only browser capture shared by official reference generation and native observation.
export default function inspectEr() {
  const svg = document.querySelector('svg');
  const v = svg.viewBox.baseVal;
  const root = svg.getScreenCTM().inverse();
  const matrix = e => root.multiply(e.getScreenCTM());
  const box = (e,b=e.getBBox()) => {
    const m = matrix(e);
    const ps = [[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]]
      .map(([x,y]) => new DOMPoint(x,y).matrixTransform(m));
    const x = Math.min(...ps.map(p=>p.x)), y = Math.min(...ps.map(p=>p.y));
    return {x,y,width:Math.max(...ps.map(p=>p.x))-x,height:Math.max(...ps.map(p=>p.y))-y};
  };
  const clientBox = r => {
    const a = new DOMPoint(r.x,r.y).matrixTransform(root);
    const b = new DOMPoint(r.right,r.bottom).matrixTransform(root);
    return {x:a.x,y:a.y,width:b.x-a.x,height:b.y-a.y};
  };
  const paint = e => {
    const s = getComputedStyle(e);
    return {fill:s.fill,stroke:s.stroke,width:s.strokeWidth,dash:s.strokeDasharray,
      opacity:s.opacity,rx:e.tagName==='rect'?s.rx:null,ry:e.tagName==='rect'?s.ry:null};
  };
  const elements = [...svg.querySelectorAll('rect,circle,path')]
    .filter(e=>!e.closest('defs')&&!e.classList.contains('er-label-background')&&
      (e.getBBox().width||e.getBBox().height))
    .map(e=>{
      const m = matrix(e);
      let index = 0;
      const path = e.getAttribute('d')?.replace(/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi,
        n=>String(Number(n)+(index++%2?m.f:m.e)));
      const rough = [...(e.closest('.outer-path,.row-rect-even,.row-rect-odd,.divider')?.classList??[])]
        .find(c=>c==='outer-path'||c==='row-rect-even'||c==='row-rect-odd'||c==='divider')??null;
      return {type:e.tagName,box:box(e),path:path??null,rough,paint:paint(e),
        markers:['marker-start','marker-end'].map(a=>e.getAttribute(a))};
    });
  const labels = [...svg.querySelectorAll('foreignObject,text')].filter(e=>!e.closest('defs')).map(e=>{
    let text,rect,font;
    if(e.tagName==='foreignObject') {
      const span=e.querySelector('span')??e.firstElementChild,fontNode=span.querySelector('p')??span;
      const walker=document.createTreeWalker(span,NodeFilter.SHOW_TEXT),rs=[];
      let n;
      while(n=walker.nextNode()) {
        if(!n.textContent.trim()) continue;
        const range=document.createRange();
        range.selectNodeContents(n);
        rs.push(...range.getClientRects());
      }
      const x=Math.min(...rs.map(r=>r.x)),y=Math.min(...rs.map(r=>r.y));
      rect=clientBox({x,y,right:Math.max(...rs.map(r=>r.right)),bottom:Math.max(...rs.map(r=>r.bottom))});
      text=span.textContent.trim().replace(/\s+/g,' ');
      font=getComputedStyle(fontNode);
    } else {
      const lines=[...e.querySelectorAll('tspan')];
      text=(lines.length?lines.map(l=>l.textContent).join(' '):e.textContent).trim().replace(/\s+/g,' ');
      font=getComputedStyle(e);
      rect=box(e);
    }
    return {text,box:rect,font:{family:font.fontFamily,size:font.fontSize,weight:font.fontWeight,
      color:e.tagName==='foreignObject'?font.color:font.fill}};
  }).filter(l=>l.text);
  const backgrounds = [...svg.querySelectorAll('foreignObject div.labelBkg,foreignObject span.edgeLabel,rect.er-label-background')]
    .map(e=>{
      const s=getComputedStyle(e);
      if(e.tagName==='rect') return {part:e.classList.contains('labelBkg')?'div':'span',box:box(e),color:s.fill};
      return {part:e.tagName.toLowerCase(),box:clientBox(e.getBoundingClientRect()),color:s.backgroundColor};
    });
  const markers = [...svg.querySelectorAll('defs marker')].map(e=>{
    const fields=(n,keys)=>Object.fromEntries(keys.map(key=>[key,n.getAttribute(key)]));
    return {
      ...fields(e,['id','refX','refY','markerWidth','markerHeight','markerUnits','orient','viewBox']),
      contents:[...e.children].map(n=>({type:n.tagName,paint:paint(n),
        path:n.getAttribute('d'),circle:n.tagName==='circle'?fields(n,['cx','cy','r']):null})),
    };
  });
  const sort=(a,b)=>a.box.y-b.box.y||a.box.x-b.box.x||
    String(a.type??a.text??a.part).localeCompare(String(b.type??b.text??b.part));
  elements.sort(sort);
  labels.sort(sort);
  backgrounds.sort(sort);
  return {view:{x:v.x,y:v.y,width:v.width,height:v.height},elements,labels,backgrounds,markers};
};
