'use strict';
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
function findRoot(start=__dirname) { for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current)) if(fs.existsSync(path.join(current,'tools/math-port/profiles/scope.json'))) return current; return null; }
const rootArg=process.argv.indexOf('--repo-root');
assert.ok(rootArg<0?process.argv.length===2:(process.argv[rootArg+1]&&process.argv.length===rootArg+2),'usage: node common-table-branch-source-witness.cjs [--repo-root <repository>]');
const root=rootArg<0?findRoot():path.resolve(process.argv[rootArg+1]);
assert.ok(root,'pass --repo-root <repository> or run from the repository');
const packages = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const src = path.join(packages, 'src');
const scope = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/profiles/scope.json'), 'utf8'));
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
assert.equal(digest(path.join(root, scope.profile.file)), scope.profile.sha256);
const load = name => require(path.join(src, 'cjs', name));
const {mathjax} = load('mathjax.js');
const {TeX} = load('input/tex.js');
const {SVG} = load('output/svg.js');
const {SvgWrapper} = load('output/svg/Wrapper.js');
const {liteAdaptor} = load('adaptors/liteAdaptor.js');
const {RegisterHTMLHandler} = load('handlers/html.js');
const {MathJaxNewcmFont} = require(path.join(packages, 'mathjax-newcm-font/cjs/svg.js'));
const profile = fs.readFileSync(path.join(root, scope.profile.file), 'utf8');
for (const match of profile.matchAll(/require\('@mathjax\/src\/js\/(input\/tex\/[^']+Configuration\.js)'\)/g)) load(match[1]);
const adaptor = liteAdaptor({fontSize:16}); RegisterHTMLHandler(adaptor);
const input = new TeX({packages:scope.profile.tex_packages});
const output = new SVG({fontData:MathJaxNewcmFont,fontCache:'none',exFactor:.5,
  displayOverflow:'linebreak',linebreaks:{inline:true},
  dynamicPrefix:path.join(packages,'mathjax-newcm-font/cjs/svg/dynamic')});
const document = mathjax.document('', {InputJax:input,OutputJax:output});
const cases = [
  {name:'labels', tex:'\\begin{align}x&=1\\tag{A}\\\\y&=2\\tag{B}\\end{align}', display:true},
  {name:'fixed', tex:'\\begin{array}{cc}a&bb\\\\ccc&d\\end{array}', display:true,
   attrs:{width:'4em',columnwidth:'auto fit'}, cellSpaces:[[[1.25,.8,.2],[.7,.3,.1]],[[.7,.3,.1],[1.25,.8,.2]]]},
  {name:'percent', tex:'\\begin{array}{cc}a&bb\\\\ccc&d\\end{array}', display:true,
   attrs:{width:'75%',columnwidth:'30% auto'}, cellSpaces:[[[1.25,.8,.2],[.7,.3,.1]],[[.7,.3,.1],[1.25,.8,.2]]]},
  {name:'equal', tex:'\\begin{array}{cc}a&bb\\\\ccc&d\\end{array}', display:true,
   attrs:{width:'auto',equalcolumns:true}, cellSpaces:[[[1.25,.8,.2],[.7,.3,.1]],[[.7,.3,.1],[1.25,.8,.2]]]},
  {name:'frame_spacing', tex:'\\begin{array}{cc}a&b\\\\c&d\\end{array}', display:true,
   attrs:{frame:'solid',framespacing:'0.5em 0.6em',columnspacing:'0.4em',rowspacing:'0.3em',columnlines:'solid',rowlines:'solid'},
   cellSpaces:[[[1.25,.8,.2],[.7,.3,.1]],[[.7,.3,.1],[1.25,.8,.2]]]},
  {name:'stretch', tex:'\\begin{matrix}\\left(\\frac{x}{y}\\right)&\\frac{a}{b}\\end{matrix}', display:true},
  {name:'break_text', tex:'\\begin{array}{c}\\text{x x}\\end{array}', display:true,
   attrs:{width:'auto',columnwidth:'0.5em'}, overflow:'linebreak', containerWidth:1280},
  {name:'break', tex:'\\begin{array}{c}abcdefghij+klmnopqrst\\end{array}', display:true,
   attrs:{width:'auto',columnwidth:'2em'}, overflow:'linebreak', containerWidth:100}
];
let active = null;
const originalCompile = input.compile;
input.compile = function() {
  const rootNode = originalCompile.apply(this, arguments);
  if (!active) return rootNode;
  const tables=[];
  const visit=node=>{if(!node)return;if(node.kind==='mtable')tables.push(node);for(const child of node.childNodes||[])visit(child);};
  visit(rootNode);
  if(active.attrs) { assert.equal(tables.length,1); for(const [name,value] of Object.entries(active.attrs))tables[0].attributes.set(name,value); }
  if(active.cellSpaces) {
    for (const [r,row] of tables[0].childNodes.entries()) for (const [c,cell] of row.childNodes.entries()) {
      const [w,h,d]=active.cellSpaces[r][c];
      const space=tables[0].factory.create('mspace');
      space.attributes.set('width',`${w}em`); space.attributes.set('height',`${h}em`); space.attributes.set('depth',`${d}em`);
      cell.setChildren([space]);
    }
  }
  if(active.overflow) rootNode.attributes.set('overflow',active.overflow);
  active.sourceRoot=rootNode;
  active.mmlTables=tables;
  return rootNode;
};
const originalProcess=output.processMath;
output.processMath=function(wrapper,node){
  if(active){
    const table=Array.from(this.nodeMap.values()).find(w=>w.node===active.mmlTables[0]);
    active.beforeProcess=table?tableState(wrapper,table):null;
  }
  const result=originalProcess.apply(this,arguments);
  if(active){
    const tableNode=active.mmlTables[0];
    const table=Array.from(this.nodeMap.values()).find(w=>w.node===tableNode);
    assert.ok(table,`missing CommonMtable for ${active.name}`);
    const data=table.getTableData();
    active.result={
      table:{bbox:box(table.getBBox()),numRows:table.numRows,numCols:table.numCols,hasLabels:table.hasLabels,
        frame:table.frame,fframe:table.fframe,fLine:table.fLine,fSpace:table.fSpace,
        cSpace:table.cSpace,rSpace:table.rSpace,cLines:table.cLines,rLines:table.rLines,
        cWidths:table.cWidths,pWidth:table.pWidth,tableData:data},
      rows:table.tableRows.map(row=>({kind:row.node.kind,bbox:box(row.getBBox()),labeled:row.labeled,
        numCells:row.numCells,cells:row.childNodes.map(cell=>({kind:cell.node.kind,bbox:box(cell.getBBox()),
          wrapWidth:cell.getWrapWidth(0),children:cell.childNodes.map(c=>({kind:c.node.kind,bbox:box(c.getBBox()),breakCount:typeof c.breakCount==="function"?c.breakCount():c.breakCount}))}))})),
      linebreakCalls:active.name==='break'?Array.from(this.nodeMap.values()).filter(w=>w.node.kind==='mspace'||w.node.kind==='mtext'||w.node.kind==='mi').map(w=>({kind:w.node.kind,breakCount:typeof w.breakCount==="function"?w.breakCount():w.breakCount,bbox:box(w.getBBox())})):[]
    };
    active.afterProcess=tableState(wrapper,table);
  }
  return result;
};
function box(b){return Object.fromEntries(['w','h','d','scale','rscale','L','R','pwidth','ic','oc','sk','dx'].map(k=>[k,b[k]]));}
function tableState(root,table){return {root:{kind:root.node.kind,bbox:box(root.bbox)},table:{kind:table.node.kind,
  bbox:box(table.bbox),cWidths:[...table.cWidths],pWidth:table.pWidth,isTop:table.isTop,
  parent:table.parent?.node?.kind??null,container:table.container?.node?.kind??null}};}
function sourceNode(node){
  const attributes=node.attributes?.getAllAttributes?.()||{};
  return {kind:node.kind,attributes,properties:node.properties||{},text:node.text||undefined,
    children:(node.childNodes||[]).map(sourceNode)};
}
mathjax.asyncIsSynchronous=true;
mathjax.asyncLoad=name=>{const target=path.isAbsolute(name)?name:path.resolve(src,'cjs',name);assert.ok(target.startsWith(packages+path.sep));return require(target);};
(async()=>{
  const records=[];
  for(const item of cases){active={...item,result:null,mmlTables:[]};
    await document.convertPromise(item.tex,{display:item.display,em:16,ex:8,containerWidth:item.containerWidth||1280});
    records.push({name:item.name,root:sourceNode(active.sourceRoot),tree:sourceNode(active.mmlTables[0]),
      beforeProcess:active.beforeProcess,afterProcess:active.afterProcess,result:active.result}); active=null;
  }
  const observed=records.map(r=>({name:r.name,table:{w:r.result.table.bbox.w,h:r.result.table.bbox.h,d:r.result.table.bbox.d,
    numRows:r.result.table.numRows,numCols:r.result.table.numCols,hasLabels:r.result.table.hasLabels,
    frame:r.result.table.frame,fLine:r.result.table.fLine,fSpace:r.result.table.fSpace,
    cSpace:r.result.table.cSpace,rSpace:r.result.table.rSpace,cLines:r.result.table.cLines,rLines:r.result.table.rLines,
    cWidths:r.result.table.cWidths,pWidth:r.result.table.pWidth,H:r.result.table.tableData.H,
    D:r.result.table.tableData.D,W:r.result.table.tableData.W,L:r.result.table.tableData.L},
    rows:r.result.rows.map(row=>({kind:row.kind,labeled:row.labeled,numCells:row.numCells,
      cells:row.cells.map(cell=>({wrapWidth:cell.wrapWidth??null,bbox:[cell.bbox.w,cell.bbox.h,cell.bbox.d],breakCount:cell.children[0].breakCount}))}))}));
  const expected=JSON.parse(fs.readFileSync(path.join(root,'tests/math/fixtures/common-source/table-branch-source-expected.json'),'utf8'));
  assert.deepEqual(observed,expected.records,'table branch Source measurements changed');
  const percent=records.find(record=>record.name==='percent');
  assert.deepEqual(percent.beforeProcess.table.cWidths,['30%',null]);
  assert.equal(percent.beforeProcess.table.bbox.pwidth,'75%');
  assert.equal(percent.beforeProcess.table.pWidth,0);
  assert.deepEqual(percent.afterProcess.table.cWidths,[15.312,35.728]);
  assert.equal(percent.afterProcess.table.pWidth,53.04);
  assert.equal(percent.afterProcess.table.bbox.w,53.04);
  assert.equal(percent.afterProcess.root.bbox.w,53.04);
  const evidence={source:expected.source,sourceTrees:records.map(({name,root,tree})=>({name,root,tree})),
    phases:records.map(({name,beforeProcess,afterProcess})=>({name,beforeProcess,afterProcess})),records:observed};
  const output=path.join(root,'build/math-port/common-source/table-branch-source.json');
  fs.mkdirSync(path.dirname(output),{recursive:true});
  fs.writeFileSync(output,JSON.stringify(evidence,null,2)+'\n');
  process.stdout.write(`verified ${observed.length} exact Common table branches; evidence ${path.relative(root,output)}\n`);
})().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
