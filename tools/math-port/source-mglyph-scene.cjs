'use strict';

// Pinned MathJax Source witness for the SVG mglyph image and legacy-index paths.
// Image data is embedded; this process never fetches external resources.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

assert.ok(process.argv.length === 2 ||
  (process.argv.length === 3 && process.argv[2] === '--write-fixture'),
'usage: source-mglyph-scene.cjs [--write-fixture]');
const root = path.resolve(__dirname, '../..');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const DATA = 'data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%222%22%20height%3D%221%22%3E%3Crect%20width%3D%222%22%20height%3D%221%22%2F%3E%3C%2Fsvg%3E';
const cases = [
  {name:'mglyph-image-auto-default',kind:'image',mathml:`<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph src="${DATA}" width="auto" height="auto"/></math>`,display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-image-explicit-positive-valign-alt',kind:'image',mathml:`<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph src="${DATA}" width="2em" height=".75em" valign=".2em" alt="Glyph &amp; &lt; &quot; &apos;"/></math>`,display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-image-negative-valign-empty-alt',kind:'image',mathml:`<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph src="${DATA}" width="2em" height="0em" valign="-.25em" alt=""/></math>`,display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-image-nonem-pixels',kind:'image',mathml:`<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph src="${DATA}" width="16px" height="12px" valign="2px" alt="pixel"/></math>`,display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-image-signed-zero-extents',kind:'image',mathml:`<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph src="${DATA}" width="-1em" height="0em" valign="-.5em" alt="signed"/></math>`,display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-image-after-math-styled',kind:'image',mathml:`<math xmlns="http://www.w3.org/1998/Math/MathML"><mstyle mathsize="150%"><mi>x</mi><mglyph src="${DATA}" width="2em" height=".75em" valign=".1em" alt="scaled"/></mstyle></math>`,display:false,em:16,ex:8,width:1280,fontCache:'local'},
  {name:'mglyph-image-display-block',kind:'image',mathml:`<math xmlns="http://www.w3.org/1998/Math/MathML" display="block"><mglyph src="${DATA}" width="1.5em" height=".5em" valign=".125em" alt="block"/></math>`,display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-image-source-escaped-query-local',kind:'image',mathml:`<math xmlns="http://www.w3.org/1998/Math/MathML"><mi>x</mi><mglyph src="${DATA}?label=a&amp;mode=b" width="1.25em" height=".5em" alt="A &amp; B"/></math>`,display:false,em:16,ex:8,width:1280,fontCache:'local'},
  {name:'mglyph-legacy-ascii-index',kind:'legacy',mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph src="" fontfamily="MathJaxNewcm" index="65"/></math>',display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-legacy-supplementary-index',kind:'legacy',mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph fontfamily="MathJaxNewcm" index="128512"/></math>',display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-legacy-supplementary-local',kind:'legacy',mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><mi>x</mi><mglyph fontfamily="MathJaxNewcm" index="128512"/></math>',display:false,em:16,ex:8,width:1280,fontCache:'local'},
  {name:'mglyph-invalid-missing-source-and-index',kind:'verification',mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph/></math>',display:false,em:16,ex:8,width:1280,fontCache:'none'},
  {name:'mglyph-invalid-empty-source-and-index',kind:'verification',mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><mglyph src="" fontfamily="" index=""/></math>',display:false,em:16,ex:8,width:1280,fontCache:'none'},
];



function flattenMml(rootNode) {
  const nodes = [], attributes = [], indices = new Map();
  const visit = (node, parent) => {
    const index = nodes.length; indices.set(node, index);
    const first = attributes.length;
    for (const [name,value] of Object.entries(node.attributes?.getAllAttributes?.() ?? {})) {
      if (value === undefined || value === null) continue;
      if (typeof value === 'boolean') attributes.push({name,kind:'boolean',boolean:value});
      else if (typeof value === 'number') attributes.push({name,kind:'number',number:value});
      else if (typeof value === 'string') attributes.push({name,kind:'text',text:value});
      else throw new Error(`unsupported actual MML attribute ${name}: ${typeof value}`);
    }
    nodes.push({kind:node.kind,inferred:node.isKind?.('inferredMrow') ?? false,parent,
      first,count:attributes.length-first,text:node.kind === 'text' ? node.getText() : ''});
    for (const child of node.childNodes ?? []) visit(child,index);
  };
  visit(rootNode,'std::numeric_limits<std::size_t>::max()');
  return {nodes,attributes,indices};
}

function imageAttributes(svg) {
  const tags = [...svg.matchAll(/<image\b([^>]*)\/?\s*>/g)];
  return tags.map(tag => [...tag[1].matchAll(/([^\s=]+)="([^"]*)"/g)]
    .map(match => ({name:match[1],value:match[2]})));
}



async function main() {
  const {createReference}=require('./reference-profile.cjs');
  const ref=createReference();
  assert.deepEqual(ref.provenance.packages,scope.profile.tex_packages);
  const packageRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
  const upstreamAsyncLoad=ref.mathjax.asyncLoad;
  const entityModules=[];
  ref.mathjax.asyncLoad=name=>{
    if(typeof name==='string' && /^\.\/util\/entities\/[a-z]\.js$/.test(name)) {
      const file=path.join(packageRoot,name);
      if(!entityModules.some(item=>item.name===name)) entityModules.push({name,sha256:sha(fs.readFileSync(file))});
      return Promise.resolve(require(file));
    }
    return upstreamAsyncLoad(name);
  };
  const {MathML}=require(path.join(packageRoot,'input/mathml.js'));
  const document=ref.mathjax.document('',{InputJax:new MathML(),OutputJax:ref.output});
  const originalProcess=ref.output.processMath;
  const originalMeasure=ref.output.measureText;
  const cachePrototype=Object.getPrototypeOf(ref.output.fontCache);
  const originalUseLocalID=cachePrototype.useLocalID;
  let current=null;
  cachePrototype.useLocalID=function(...args) {
    const result=originalUseLocalID.apply(this,args);
    if (current) current.prefixes.push(this.localID);
    return result;
  };
  ref.output.measureText=function(text,variant,font) {
    const result=originalMeasure.apply(this,arguments);
    if (current) current.measurements.push({text,variant,fontFamily:font?.[0] ?? '',italic:font?.[1] ?? false,bold:font?.[2] ?? false,
      bbox:{w:result.w,h:result.h,d:result.d}});
    return result;
  };
  const records=[];
  let observed=new WeakSet();
  ref.output.processMath=function(wrapper,node) {
    const captureRoot=current && !current.rootSeen;
    if (captureRoot) {
      current.mml=flattenMml(wrapper.node);
      current.display=this.math.display;
    }
    const result=originalProcess.apply(this,arguments);
    if (current) {
      if (captureRoot) {
        const b=wrapper.getBBox();
        current.rootBBox={w:b.w,h:b.h,d:b.d,scale:b.scale,rscale:b.rscale,L:b.L,R:b.R};
        current.rootSeen=true;
      }
      const walk=w=>{
        if (!w || observed.has(w)) return;
        observed.add(w);
        if (w.node?.kind==='mglyph') {
          const b=w.getBBox();
          const char=w.charWrapper;
          current.mglyphs.push({index:current.mml?.indices.get(w.node),bbox:{w:b.w,h:b.h,d:b.d,scale:b.scale,rscale:b.rscale,L:b.L,R:b.R},
            charText:char?.node?.getText?.() ?? '',charKind:char?.node?.kind ?? '',charBBox:char?(()=>{const x=char.getBBox();return{w:x.w,h:x.h,d:x.d};})():null});
        }
        for (const child of w.childNodes ?? []) walk(child);
      };
      walk(wrapper);
    }
    return result;
  };
  try {
    for (const entry of cases) {
      ref.output.fontCache.clearCache();
      ref.output.options.fontCache=entry.fontCache;
      ref.output.options.localID=null;
      current={entry,requestedDisplay:entry.display,display:entry.display,rootBBox:null,rootSeen:false,mglyphs:[],prefixes:[],measurements:[],mml:null};
      observed=new WeakSet();
      let svg='',error=null;
      try {
        const node=await document.convertPromise(entry.mathml,{display:entry.display,em:entry.em,ex:entry.ex,containerWidth:entry.width});
        svg=ref.adaptor.serializeXML(node);
        assert.ok(svg.includes('<svg'),`${entry.name}: Source did not serialize an SVG root`);
        current.display=ref.output.math.display;
      } catch (caught) {
        error={name:String(caught?.name ?? 'Error'),message:String(caught?.message ?? caught)};
      }
      const mml=current.mml ?? {nodes:[],attributes:[],indices:new Map()};
      const images=imageAttributes(svg);
      if (entry.kind==='image' && !error) {
        assert.equal(images.length,1,`${entry.name}: one actual Source image node`);
        assert.equal(current.mglyphs.length,1,`${entry.name}: one actual SvgMglyph wrapper`);
        assert.deepEqual(images[0].map(x=>x.name),
          ['width','height','transform','preserveAspectRatio','aria-label','href'],
          `${entry.name}: Source image property order`);
        assert.ok(images[0].at(-1).value.startsWith('data:image/svg+xml,'),
          `${entry.name}: image href is an embedded SVG data URI`);
        const box=current.mglyphs[0].bbox;
        assert.ok(Object.values(box).every(Number.isFinite),
          `${entry.name}: Source image geometry is finite (signed/zero values allowed)`);
      }
      if (entry.kind==='legacy' && !error) {
        assert.equal(images.length,0,`${entry.name}: legacy font/index branch emits no image`);
        assert.equal(current.mglyphs.length,1,`${entry.name}: one actual SvgMglyph wrapper`);
        assert.ok(current.mglyphs[0].charKind==='text',`${entry.name}: actual charWrapper owns TextNode`);
        assert.equal(current.measurements.length,1,`${entry.name}: capture actual text measurement`);
        assert.equal(current.measurements[0].variant,'-explicitFont');
        assert.equal(current.measurements[0].fontFamily,'MathJaxNewcm');
        assert.ok(Object.values(current.mglyphs[0].bbox).every(Number.isFinite),
          `${entry.name}: Source legacy geometry is finite`);
      }
      if (entry.kind==='verification' && !error) {
        assert.ok(mml.nodes.some(n=>n.kind==='merror'),`${entry.name}: invalid MML is represented as Source merror`);
        assert.equal(images.length,0,`${entry.name}: verification error is not an image success`);
      }
      assert.ok(current.prefixes.every(x=>typeof x==='string'));
      if (entry.fontCache==='none') assert.equal(current.prefixes.length,0,`${entry.name}: no local prefixes`);
      else assert.ok(current.prefixes.length>0,`${entry.name}: local mode captured actual Source cache prefixes`);
      records.push({name:entry.name,kind:entry.kind,mathml:entry.mathml,requestedDisplay:entry.display,
        display:current.display,em:entry.em,ex:entry.ex,width:entry.width,fontCache:entry.fontCache,localID:null,
        rootBBox:current.rootBBox??{w:0,h:0,d:0,scale:1,rscale:1,L:0,R:0},mglyphBBox:current.mglyphs[0]?.bbox??null,
        mglyphs:current.mglyphs,prefixes:current.prefixes.slice(),imageAttributes:images,measurements:current.measurements.slice(),mml,error,svg});
      process.stdout.write(`${entry.name}: ${error?`ERROR ${error.name}: ${error.message}`:`${images.length} image(s), ${current.mglyphs.length} mglyph wrapper(s), bbox=${JSON.stringify(current.mglyphs[0]?.bbox??null)}, prefixes=${JSON.stringify(current.prefixes)}, measure=${JSON.stringify(current.measurements.map(x=>({text:x.text,variant:x.variant,bbox:x.bbox})))}`}\n`);
      current=null;
    }
  } finally {
    ref.output.processMath=originalProcess;
    ref.output.measureText=originalMeasure;
    cachePrototype.useLocalID=originalUseLocalID;
    current=null;
  }
  const provenance={upstream:scope.upstream,profile:ref.provenance,scopeSha256:sha(fs.readFileSync(scopePath)),entityModules,
    input:'MathML; embedded data:image/svg+xml only; no external fetch',casesSha256:sha(Buffer.from(JSON.stringify(cases)))};
  const evidence=path.join(root,'build/math-port/mglyph-scene-source.json');
  fs.mkdirSync(path.dirname(evidence),{recursive:true});
  fs.writeFileSync(evidence,JSON.stringify({provenance,records},null,2)+'\n');
  const {writeObservations, flatMml} = require('./mathml-observation-data.cjs');
  const observations = {sourceMglyphCases: records.map(r => ({
    name: r.name, kind: r.kind, mathml: r.mathml, requested_display: r.requestedDisplay,
    display: r.display, em: r.em, ex: r.ex, width: r.width,
    font_cache: r.fontCache, local_id: r.localID ?? '',
    root_w: r.rootBBox.w, root_h: r.rootBBox.h, root_d: r.rootBBox.d,
    mglyph_bbox: r.mglyphBBox, error_name: r.error?.name ?? '', error_message: r.error?.message ?? '',
    svg: r.svg ?? '', local_cache_prefixes: r.prefixes,
    image_attributes: r.imageAttributes.flatMap((attrs, image_index) =>
      attrs.map(a => ({image_index, ...a}))),
    measurements: r.measurements.map(m => ({text: m.text, variant: m.variant,
      font_family: m.fontFamily ?? '', italic: m.italic ?? false, bold: m.bold ?? false, ...m.bbox})),
    ...flatMml(r.mml.nodes, r.mml.attributes)
  }))};
  writeObservations('mglyph', observations);
  process.stdout.write(`Verified ${records.length} pinned Source mglyph cases; evidence ${path.relative(root,evidence)}\n`);
}

main().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
