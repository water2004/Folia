'use strict';
// Real CommonOutputJax.getBBox, pinned profile fonts and MML factories.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
for(const [file,ts,cjs]of [
 ['output/common.ts','3686a8e803e26ae59d03c096ac2e6e5b1ab1332a5c4bcaa083cb9a02c2c61096','b8a58d983e465f21ed36574472140bfd65e2696bb72f6c5647e0a2ae47eb1bd9'],
 ['output/common/WrapperFactory.ts','a798803bab094ae1e72cd7f96eb37aca936ac458da145c1ebb22079a0280b6cd','57ce2e232ec1919fa518699226466ceef52cbcb2af75559137d2bb2dc83ea2ab'],
 ['output/common/Wrapper.ts','d1db6112db1cb3546658eb8868b5997a4f69e588632ecbe4e08a71949544a828','9e21f886b91624d170ff89df0eecef31e789ffecda001abc3e5649a38419412b'],
 ['output/common/Wrappers/mtable.ts','a1e060baab84d7b842c811c560ab73f32ca91614e3a5b02f7f1712db73b0100c','3b536684d4899f8603fa5db61a081db2bad9ed9d09b4e3de45f7d0f2fbade29c'],
 ['util/BBox.ts','a4232fac0fce35a15ad05a4dbb4af0999bf04f90ec3f0fe0c594b22c5da70b5b','792428ce9802696fe669aa8a13b1a92f5f2eaadd07e05fc28bd359acd8d2062d']
])for(const [name,hash]of [[path.join(root,'.reference/MathJax-src/ts',file),ts],[path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',file.replace(/\.ts$/,'.js')),cjs]])assert.equal(crypto.createHash('sha256').update(fs.readFileSync(name)).digest('hex'),hash,file);
const reference=require('./reference-profile.cjs').createReference();assert.equal(reference.provenance.compiled_source_sha256,'cbb0d3b6a08fbde6614b070f111fb58cc3cfe9d00a9713163fd4cd6d0300355a');
const load=name=>require(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs',name));
const {MmlFactory}=load('core/MmlTree/MmlFactory.js'),{AbstractMathItem}=load('core/MathItem.js');
const factory=new MmlFactory(),output=reference.output;
const node=(kind,children=[],attrs={})=>factory.create(kind,attrs,children);
const token=(kind,text)=>node(kind,[factory.create('text').setText(text)]);
const space=(width='1.25em',height='.8em',depth='.2em')=>node('mspace',[],{width,height,depth});
function table(frame=false){const rows=[[space(),space('.7em','.3em','.1em')],[space('.7em','.3em','.1em'),space()]].map(cells=>node('mtr',cells.map(c=>node('mtd',[c]))));return node('mtable',rows,{columnspacing:'.5em',rowspacing:'.2em',framespacing:frame?'.5em .125em':'0 0',...(frame?{frame:'solid',rowlines:'dashed'}:{})});}
const cases=[
 ['space',()=>space()],['negative-space',()=>space('-.25em','.3em','.1em')],['px-space',()=>space('8px','16px','4px')],
 ['ex-space',()=>space('1.5ex','1ex','.5ex')],
 ['ex-padded',()=>node('mpadded',[space('1ex','.5ex','.25ex')],{width:'+.5ex',lspace:'.25ex'})],
 ['padded',()=>node('mpadded',[space('1em','.4em','.2em')],{width:'+.5em',lspace:'.2em',height:'+.1em',depth:'+.1em',voffset:'.05em'})],
 ['styled',()=>node('mrow',[space()],{style:'margin-left: .2em; margin-right: .3em; padding: .1em; border: .05em solid red'})],
 ['fraction',()=>node('mfrac',[space(),space('.7em','.3em','.1em')])],
 ['atop',()=>node('mfrac',[space(),space('.7em','.3em','.1em')],{linethickness:0})],
 ['bevelled',()=>node('mfrac',[space(),space('.7em','.3em','.1em')],{bevelled:true})],
 ['sqrt',()=>node('msqrt',[space()])],
 ['root',()=>node('mroot',[space(),space('.3em','.3em','0em')])],
 ['table',()=>table()],['framed-table',()=>table(true)],
 ['nested',()=>node('mfrac',[node('msqrt',[space()]),node('mpadded',[table()],{width:'+.2em'})])],
 ['tokens',()=>node('mrow',[token('mi','x'),token('mo','+'),token('mn','12')])],
 ['bold-tokens',()=>node('mstyle',[node('mrow',[token('mi','AB'),token('mo','='),token('mi','C')])],{mathvariant:'bold'})],
 ['unicode',()=>node('mrow',[token('mi','α'),token('mo','→'),token('mi','β')])]
];
const fields=['w','h','d','scale','rscale','L','R','ic','oc','sk','dx'];
async function main(){const rows=[];for(const display of [false,true])for(const [name,make]of cases){
 const subtree=make();subtree.setInheritedAttributes({},display,0,false);
 const parent=node('mrow',[subtree]),item=new AbstractMathItem('',null,display);item.root=subtree;item.setMetrics(16,8,640,1);
 output.pxPerEm=8/output.font.params.x_height;
 const beforeParent=subtree.parent;
 const box=await reference.mathjax.handleRetriesFor(()=>output.getBBox(item,reference.document));
 assert.equal(subtree.parent===beforeParent,true);assert.equal(parent.childNodes.includes(subtree),true);assert.equal(output.nodeMap,null);
 rows.push({name,display,box:Object.fromEntries(fields.map(k=>[k,box[k]])),pwidth:box.pwidth});
 }
 const repeated=node('mrow',[node('msqrt',[space()]),node('mfrac',[space(),space('.7em','.3em','.1em')],{bevelled:true})]);
 repeated.setInheritedAttributes({},false,0,false);
 const repeatedParent=node('mrow',[repeated]),repeatedChildren=[...repeated.childNodes];
 const repeatedItem=new AbstractMathItem('',null,false);repeatedItem.root=repeated;repeatedItem.setMetrics(16,8,640,1);
 let first;for(let i=0;i<2;++i){const box=await reference.mathjax.handleRetriesFor(()=>output.getBBox(repeatedItem,reference.document));
 assert.equal(repeated.parent===repeatedParent,true);assert.equal(repeated.childNodes.length,repeatedChildren.length);
 assert.equal(repeated.childNodes.every((child,j)=>child===repeatedChildren[j]),true);assert.equal(output.nodeMap,null);
 if(first)for(const field of fields)assert.equal(box[field],first[field]);else first=Object.fromEntries(fields.map(k=>[k,box[k]]));}
 const {byteField, binary64, writeTables} = require('./fixture-data.cjs');
 writeTables(path.join(root, 'tests/math/fixtures/output/common-bbox'), {
   cases: {columns: ["name_bytes", "display_boolean", "w_binary64", "h_binary64", "d_binary64", "scale_binary64", "rscale_binary64", "L_binary64", "R_binary64", "ic_binary64", "oc_binary64", "sk_binary64", "dx_binary64", "pwidth_bytes"],
     rows: rows.map(r => [byteField(r.name), +r.display, ...fields.map(k => {
       assert.equal(Number.isFinite(r.box[k]), true, k);
       return binary64(r.box[k]);
     }), byteField(r.pwidth)])}
 });
 console.log(JSON.stringify({cases:rows.length,getBBoxCalls:rows.length+2,compiled_source_sha256:reference.provenance.compiled_source_sha256}));
}
main().catch(e=>{console.error(e);process.exitCode=1});
