// Read-only browser capture. No force-layout or route acceptance is run here.
export default function inspectMindmap(root) {
 const view=root.viewBox.baseVal;
 const nodes=Array.from(root.querySelectorAll('g.mindmap-node')).map(n=>{
  const transform=n.transform.baseVal.consolidate().matrix,b=n.getBBox(),shape=n.querySelector('.label-container'),text=n.querySelector('text');
  return {position:{x:transform.e,y:transform.f},bounds:{x:b.x,y:b.y,width:b.width,height:b.height},fill:getComputedStyle(shape).fill,color:getComputedStyle(text).fill,text:text.textContent};
 });
 const edges=Array.from(root.querySelectorAll('.edgePaths path')).map(e=>({id:e.id,path:e.getAttribute('d'),stroke:getComputedStyle(e).stroke,width:parseFloat(getComputedStyle(e).strokeWidth)}));
 const bounds=root.getBBox();
 return {nodes,edges,bounds:{x:bounds.x,y:bounds.y,width:bounds.width,height:bounds.height},view:{x:view.x,y:view.y,width:view.width,height:view.height}};
};
