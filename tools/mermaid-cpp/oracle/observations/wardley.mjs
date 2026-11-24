// Browser capture only: shared by official fixture generation and Python validation.
export default root => {
  const svg = root.querySelector('svg');
  const box = e => { const b=e.getBBox(); return [b.x,b.y,b.width,b.height]; };
  const attrs = (selector, names) => [...svg.querySelectorAll(selector)].map(e => {
    const style=getComputedStyle(e);
    const rotation=(e.getAttribute('transform')||'').match(/^rotate\(\s*(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s*\)$/);
    return {...Object.fromEntries(names.map(n=>[n,e.getAttribute(n)])),
      family:style.fontFamily,fill:style.fill,stroke:style.stroke,
      strokeWidth:style.strokeWidth,strokeDasharray:style.strokeDasharray,
      opacity:style.opacity,anchor:style.textAnchor,text:e.textContent,
      markerEnd:e.hasAttribute('marker-end'),markerStart:e.hasAttribute('marker-start'),
      rotation:rotation?{angle:Number(rotation[1]),cx:Number(rotation[2]),cy:Number(rotation[3])}:null,
      box:box(e)};
  });
  return {
    view:[svg.viewBox.baseVal.x,svg.viewBox.baseVal.y,svg.viewBox.baseVal.width,svg.viewBox.baseVal.height],
    axes:attrs('.wardley-axes line',['x1','y1','x2','y2']),
    stages:attrs('.wardley-stage-label',['x','y']),
    stageBoundaries:attrs('.wardley-stages line',['x1','y1','x2','y2']),
    nodes:attrs('.wardley-node-label',['x','y','font-size']),
    nodeShapes:attrs('.wardley-node circle, circle.wardley-node, circle.wardley-outsource-overlay, circle.wardley-buy-overlay, circle.wardley-build-overlay, circle.wardley-market-overlay, circle.wardley-market-dot, rect.wardley-pipeline-parent, .wardley-node rect',['cx','cy','r','x','y','width','height']),
    links:attrs('.wardley-link',['x1','y1','x2','y2','stroke-dasharray']),
    linkLabels:attrs('.wardley-link-label',['x','y','font-size']),
    trends:attrs('.wardley-trend',['x1','y1','x2','y2']),
    pipelines:attrs('.wardley-pipeline-box',['x','y','width','height']),
    pipelineLinks:attrs('.wardley-pipeline-evolution-link',['x1','y1','x2','y2']),
    annotations:attrs('circle.wardley-annotation, .wardley-annotation circle',['cx','cy','r']),
    annotationNumbers:attrs('.wardley-annotation-number, .wardley-annotation text',['x','y']),
    annotationBoxes:attrs('.wardley-annotations-box rect, rect.wardley-annotations-box',['x','y','width','height']),
    annotationLabels:attrs('.wardley-annotations-box text, text.wardley-annotation-box-label',['x','y']),
    accelerators:attrs('.wardley-accelerators path, .wardley-deaccelerators path, path.wardley-accelerator',[]),
    acceleratorLabels:attrs('.wardley-accelerators text, .wardley-deaccelerators text, text.wardley-accelerator-label',['x','y']),
    strategyOverlays:attrs('.wardley-outsource-overlay, .wardley-buy-overlay, .wardley-build-overlay, .wardley-market-overlay',['cx','cy','r']),
    marketDots:attrs('.wardley-market-dot',['cx','cy','r']),
    inertia:attrs('.wardley-inertia',['x1','y1','x2','y2']),
    notes:attrs('.wardley-notes text, text.wardley-note',['x','y']),
    width:svg.getAttribute('width'),height:svg.getAttribute('height'),maxWidth:svg.style.maxWidth,
  };
};
