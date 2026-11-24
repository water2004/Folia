'use strict';

// Independent pinned MathJax Source witness for maction rendering and passive
// event registration. The host never dispatches callbacks during typesetting.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

assert.ok(process.argv.length === 2 ||
  (process.argv.length === 3 && process.argv[2] === '--write-fixture'),
'usage: source-maction-scene.cjs [--write-fixture]');
const root = path.resolve(__dirname, '../..');
const scopePath = path.join(root, 'docs/math-port/scope.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const cases = [
  {name:'toggle-selection', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="toggle" selection="2"><mi>a</mi><mi>b</mi></maction><mi>c</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'none'},
  {name:'tooltip-text-special', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip"><mi>x</mi><mtext>Tip &amp; &lt; &quot; &apos;</mtext></maction><mi>y</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'none'},
  {name:'tooltip-math-none', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip"><mi>x</mi><mrow><mi>y</mi><mo>+</mo><mi>z</mi></mrow></maction><mi>w</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'none'},
  {name:'tooltip-math-local-following', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip" data-offsets="1em 2ex"><mi>x</mi><mrow><mi>y</mi><mo>+</mo><mi>z</mi></mrow></maction><mi>w</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'local'},
  {name:'statusline', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="statusline"><mi>x</mi><mtext>Status &amp; details</mtext></maction><mi>y</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'none'},
  {name:'tooltip-missing-tip', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip"><mi>x</mi></maction><mi>y</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'none'},
  {name:'unknown-action', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="unknown"><mi>x</mi><mi>y</mi></maction><mi>z</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'none'},
  {name:'toggle-line-segments', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><maction actiontype="toggle" selection="1"><mrow><mi>aaaaaaaa</mi><mo linebreak="newline" linebreakstyle="after">+</mo><mi>bbbbbbbb</mi></mrow><mi>c</mi></maction><mi>d</mi></mrow></math>', display:true, em:16, ex:8, width:120, fontCache:'none'},
  {name:'nested-tooltip-local', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip"><mi>x</mi><mrow><maction actiontype="tooltip"><mi>y</mi><mrow><mi>z</mi><mo>+</mo><mi>q</mi></mrow></maction><mi>w</mi></mrow></maction><mi>r</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'local'},
  {name:'nested-tooltip-local-fixed-id', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip"><mi>x</mi><mrow><mi>x</mi><mo>+</mo><mi>y</mi></mrow></maction><mi>x</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'local', localID:'FoliaAction'},
  {name:'tooltip-math-local-multisegment', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><maction actiontype="tooltip"><mrow><mi>aaaaaaaa</mi><mo linebreak="newline" linebreakstyle="after">+</mo><mi>bbbbbbbb</mi></mrow><mrow><mi>y</mi><mo>+</mo><mi>z</mi></mrow></maction><mi>d</mi></mrow></math>', display:false, em:16, ex:8, width:120, fontCache:'local'},
  {name:'tooltip-math-local-empty-fixed-id', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip"><mi>x</mi><mrow><mi>y</mi><mo>+</mo><mi>z</mi></mrow></maction><mi>w</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'local', localID:''},
  {name:'tooltip-math-local-empty-glyph', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip"><mspace width="1em"/><mspace width="1em"/></maction></math>', display:false, em:16, ex:8, width:1280, fontCache:'local'},
  {name:'nested-tooltip-none', mathml:'<math xmlns="http://www.w3.org/1998/Math/MathML"><maction actiontype="tooltip"><mi>x</mi><mrow><maction actiontype="tooltip"><mi>y</mi><mrow><mi>z</mi><mo>+</mo><mi>q</mi></mrow></maction><mi>w</mi></mrow></maction><mi>r</mi></math>', display:false, em:16, ex:8, width:1280, fontCache:'none'},
];




function flattenMml(rootNode) {
  const nodes = [], attributes = [], indices = new Map();
  const visit = (node, parent) => {
    const index = nodes.length;
    indices.set(node, index);
    const first = attributes.length;
    const raw = node.attributes?.getAllAttributes?.() ?? {};
    for (const [name, value] of Object.entries(raw)) {
      if (value === undefined || value === null) continue;
      if (typeof value === 'boolean') attributes.push({name, kind:'boolean', boolean:value});
      else if (typeof value === 'number') attributes.push({name, kind:'number', number:value});
      else if (typeof value === 'string') attributes.push({name, kind:'text', text:value});
      else throw new Error(`unsupported actual MML attribute ${name}: ${typeof value}`);
    }
    nodes.push({kind:node.kind, inferred:node.isKind?.('inferredMrow') ?? false,
      parent, first, count:attributes.length-first,
      text:node.kind === 'text' ? node.getText() : ''});
    for (const child of node.childNodes ?? []) visit(child, index);
  };
  visit(rootNode, 'std::numeric_limits<std::size_t>::max()');
  return {nodes, attributes, indices};
}

function flattenStyles(styles) {
  const entries = [];
  const visit = (rules, ancestry = []) => {
    for (const [selector, declarations] of Object.entries(rules)) {
      const path = [...ancestry, selector];
      for (const [property, value] of Object.entries(declarations)) {
        if (value && typeof value === 'object' && !Array.isArray(value)) continue;
        entries.push({selector:path.join(' | '), property,
          kind:typeof value === 'number' ? 'number' : 'text',
          text:typeof value === 'string' ? value : '', number:typeof value === 'number' ? value : 0});
      }
      const children = Object.fromEntries(Object.entries(declarations)
        .filter(([, value]) => value && typeof value === 'object' && !Array.isArray(value)));
      if (Object.keys(children).length) visit(children, path);
    }
  };
  visit(styles);
  return entries;
}


async function main() {
  const {createReference} = require('./reference-profile.cjs');
  const ref = createReference();
  assert.deepEqual(ref.provenance.packages, scope.profile.tex_packages);
  const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
  const loadedEntityModules = [];
  const upstreamAsyncLoad = ref.mathjax.asyncLoad;
  ref.mathjax.asyncLoad = name => {
    if (typeof name === 'string' && /^\.\/util\/entities\/[a-z]\.js$/.test(name)) {
      const file = path.join(packageRoot, name);
      if (!loadedEntityModules.some(item => item.name === name)) {
        const source = path.join(root, '.reference/MathJax-src/ts',
          name.replace(/^\.\//, '').replace(/\.js$/, '.ts'));
        loadedEntityModules.push({name, compiledSha256:sha(fs.readFileSync(file)),
          sourceSha256:sha(fs.readFileSync(source))});
      }
      return Promise.resolve(require(file));
    }
    return upstreamAsyncLoad(name);
  };
  const {MathML} = require(path.join(packageRoot, 'input/mathml.js'));
  const document = ref.mathjax.document('', {InputJax:new MathML(), OutputJax:ref.output});
  const mactionModule = require(path.join(packageRoot, 'output/svg/Wrappers/maction.js'));
  const SvgMaction = mactionModule.SvgMaction;
  const styles = JSON.stringify(SvgMaction.styles);
  const styleEntries = flattenStyles(SvgMaction.styles);
  const prototype = Object.getPrototypeOf(ref.adaptor.node('svg', {}));
  const originalAddEventListener = prototype.addEventListener;
  const fontCachePrototype = Object.getPrototypeOf(ref.output.fontCache);
  const originalUseLocalID = fontCachePrototype.useLocalID;
  let activePrefixes = null;
  fontCachePrototype.useLocalID = function(...args) {
    const result = originalUseLocalID.apply(this, args);
    if (activePrefixes) activePrefixes.push(this.localID);
    return result;
  };
  const receiverIds = new WeakMap();
  const eventHosts = new WeakMap();
  let nextReceiverId = 1;
  let nextHostId = 1;
  let activeListeners = null;
  prototype.addEventListener = function(type, handler) {
    assert.ok(activeListeners, 'unexpected event registration outside source capture');
    assert.equal(typeof type, 'string');
    assert.equal(typeof handler, 'function');
    if (!receiverIds.has(this)) receiverIds.set(this, nextReceiverId++);
    if (!eventHosts.has(this)) eventHosts.set(this, {id:nextHostId++, target:new EventTarget()});
    const host = eventHosts.get(this);
    const before = ref.adaptor.serializeXML(this);
    host.target.addEventListener(type, handler);
    // Retain the actual receiver, event host and callback through whole-SVG serialization.
    activeListeners.push({receiver:this, handler, eventTarget:host.target, type,
      targetId:receiverIds.get(this), hostId:host.id, before, after:ref.adaptor.serializeXML(this)});
  };
  const removeListeners = listeners => {
    for (const listener of listeners ?? [])
      listener.eventTarget.removeEventListener(listener.type, listener.handler);
    if (listeners) listeners.length = 0;
  };

  const records = [];
  const processMath = ref.output.processMath;
  let activeWrappers = null;
  let seenMactionNodes = null;
  let activeMml = null;
  let activeRootBBox = null;
  let activeSourceDisplay = null;
  ref.output.processMath = function(wrapper, node) {
    const captureRoot = activeMml === null;
    if (captureRoot) {
      activeMml = flattenMml(wrapper.node);
      activeSourceDisplay = this.math.display;
      assert.equal(typeof activeSourceDisplay, 'boolean', 'root processMath exposes actual Source math.display');
    }
    const result = processMath.apply(this, arguments);
    if (captureRoot) {
      const b = wrapper.getBBox();
      activeRootBBox = {w:b.w,h:b.h,d:b.d,scale:b.scale,rscale:b.rscale,L:b.L,R:b.R,pwidth:b.pwidth};
    }
    if (activeWrappers) {
      const visit = current => {
        if (current.node.kind === 'maction' && !seenMactionNodes.has(current.node)) {
          seenMactionNodes.add(current.node);
          const bbox = current.getBBox();
          const selected = current.selected;
          const nodeIndex = activeMml?.indices.get(current.node);
          assert.ok(nodeIndex !== undefined, 'rendered maction corresponds to captured actual Source MML node');
          activeWrappers.push({nodeIndex, actionType:current.node.attributes.get('actiontype'),
            selection:current.node.attributes.get('selection'),
            selectedKind:selected?.node?.kind ?? '',
            tipDx:current.tipDx ?? 0, tipDy:current.tipDy ?? 0,
            bbox:{w:bbox.w,h:bbox.h,d:bbox.d,scale:bbox.scale,rscale:bbox.rscale,
              L:bbox.L,R:bbox.R,pwidth:bbox.pwidth}});
        }
        for (const child of current.childNodes || []) visit(child);
      };
      visit(wrapper);
    }
    return result;
  };
  try {
    for (const entry of cases) {
      ref.output.fontCache.clearCache();
      ref.output.options.fontCache = entry.fontCache;
      ref.output.options.localID = entry.localID ?? null;
      activeListeners = [];
      activePrefixes = [];
      activeWrappers = [];
      activeMml = null;
      activeRootBBox = null;
      activeSourceDisplay = null;
      seenMactionNodes = new WeakSet();
      const node = await document.convertPromise(entry.mathml, {
        display:entry.display, em:entry.em, ex:entry.ex, containerWidth:entry.width
      });
      const svg = ref.adaptor.serializeXML(node);
      assert.ok(svg.includes('<svg'), `${entry.name}: Source SVG missing`);
      const bbox = activeWrappers[0]?.bbox ?? {w:0,h:0,d:0,scale:1,rscale:1,L:0,R:0,pwidth:0};
      assert.ok(activeRootBBox, `${entry.name}: actual root MathJax wrapper was not observed`);
      const rootBBox = activeRootBBox;
      const listeners = activeListeners.map(({targetId,hostId,type,before,after}) =>
        ({target:targetId,host:hostId,type,before,after}));
      for (const listener of activeListeners) assert.equal(typeof listener.handler, 'function');
      if (entry.name === 'toggle-selection') {
        assert.ok(svg.includes('data-toggle="2"'), 'selection=2 must mark the visible toggle hitbox');
        assert.deepEqual(listeners.map(item=>item.type), ['click']);
      } else if (entry.name === 'tooltip-text-special') {
        assert.ok(svg.includes('<title>Tip & < " \'</title>'), `text tooltip Source title differs: ${svg}`);
        assert.equal(listeners.length, 0, 'mtext tooltip uses title without event registrations');
      } else if (entry.name.startsWith('tooltip-math-')) {
        assert.ok(svg.includes('<foreignObject'), 'mathematical tooltip must include its hidden nested host');
        const expectedEvents = entry.name === 'tooltip-math-local-multisegment'
          ? ['mouseover','mouseout','mouseover','mouseout'] : ['mouseover','mouseout'];
        assert.deepEqual(listeners.map(item=>item.type), expectedEvents);
        if (entry.name === 'tooltip-math-local-following') {
          assert.equal(activeWrappers[0]?.tipDx, 1, 'data-offsets x must be one em');
          assert.equal(activeWrappers[0]?.tipDy, 0.862, 'data-offsets 2ex must use Source length2em/font metrics');
        }
        if (entry.name === 'tooltip-math-local-multisegment') {
          assert.ok((svg.match(/data-mjx-linebox="true"/g) || []).length >= 2,
            'Source selected mrow contains multiple line segments');
          assert.equal((svg.match(/<foreignObject/g) || []).length, 2,
            'Source emits one tooltip host for each own maction segment');
        }
        if (entry.name === 'tooltip-math-local-empty-fixed-id')
          assert.ok(svg.includes('MJX-NCM-'), 'empty explicit localID keeps Source empty-prefix glyph IDs');
        if (entry.name === 'tooltip-math-local-empty-glyph') {
          const foreign = svg.slice(svg.indexOf('<foreignObject'), svg.indexOf('</foreignObject>') + 16);
          assert.ok(foreign.includes('<defs/>'), 'empty tooltip cache serializes Source empty defs');
          assert.ok(!foreign.includes('<path id='), 'empty tooltip introduces no glyph definitions');
        }
      } else if (entry.name === 'statusline') {
        assert.ok(svg.includes('data-statusline="Status &amp; details"'));
        assert.deepEqual(listeners.map(item=>item.type), ['mouseover','mouseout']);
      } else if (entry.name === 'tooltip-missing-tip') assert.equal(listeners.length, 0);
      else if (entry.name === 'unknown-action') assert.equal(listeners.length, 0);
      else if (entry.name === 'toggle-line-segments') {
        assert.equal((svg.match(/data-mjx-linebox="true"/g) || []).length, 2);
        assert.deepEqual(listeners.map(item=>item.type), ['click','click']);
      } else if (entry.name === 'nested-tooltip-local') {
        assert.equal((svg.match(/<foreignObject/g) || []).length, 2);
        assert.equal(listeners.length, 4);
      } else if (entry.name === 'nested-tooltip-local-fixed-id') {
        assert.ok(svg.includes('MJX-FoliaAction-'), 'explicit localID prefix must appear in Source cache IDs');
        assert.equal((svg.match(/<foreignObject/g) || []).length, 1);
      } else if (entry.name === 'nested-tooltip-none') {
        assert.equal((svg.match(/<foreignObject/g) || []).length, 2);
        assert.equal((svg.match(/<defs>/g) || []).length, 0,
          'none cache mode emits inline paths without defs, including nested math');
      }
      for (const item of listeners) assert.equal(item.before, item.after,
        `${entry.name}: addEventListener must not mutate serialized receiver DOM`);
      const ids = [...svg.matchAll(/\sid="([^"]+)"/g)].map(match=>match[1]);
      const hrefs = [...svg.matchAll(/(?:href|xlink:href)="([^"]+)"/g)].map(match=>match[1]);
      const duplicateIds = [...new Set(ids.filter((id,i)=>ids.indexOf(id)!==i))];
      if (entry.name === 'nested-tooltip-local') assert.equal(duplicateIds.length, 0,
        'automatic nested local cache IDs must remain unique in this Source witness');
      if (entry.name === 'nested-tooltip-local-fixed-id')
        assert.ok(duplicateIds.includes('MJX-FoliaAction-NCM-I-1D465'),
          'fixed localID must retain Source duplicate glyph ID across nested SVG caches');
      const cacheObservation = {ids,hrefs,duplicateIds,
        defs:(svg.match(/<defs>/g)||[]).length,foreignObjects:(svg.match(/<foreignObject/g)||[]).length};
      assert.ok(activePrefixes.every(prefix=>typeof prefix === 'string'), 'all actual cache prefixes are strings');
      if (entry.fontCache === 'local') assert.ok(activePrefixes.length > 0, `${entry.name}: local mode uses LocalID`);
      else assert.equal(activePrefixes.length, 0, `${entry.name}: none mode does not use LocalID`);
      records.push({...entry, requestedDisplay:entry.display, display:activeSourceDisplay,
        mactionBBox:bbox, rootBBox, mml:activeMml, styles, styleEntries, svg, listeners,
        localCachePrefixes:activePrefixes.slice(), cacheObservation,
        maction:activeWrappers});
      process.stdout.write(`${entry.name}: svg=${Buffer.byteLength(svg)}B listeners=${listeners.map(x=>x.type).join(',') || 'none'}\n`);
      removeListeners(activeListeners);
      activeListeners = null;
      activePrefixes = null;
      activeWrappers = null;
      seenMactionNodes = null;
      activeMml = null;
      activeRootBBox = null;
      activeSourceDisplay = null;
    }
  } finally {
    if (activeListeners) removeListeners(activeListeners);
    activeListeners = null;
    activePrefixes = null;
    activeWrappers = null;
    seenMactionNodes = null;
    activeMml = null;
    activeRootBBox = null;
    activeSourceDisplay = null;
    prototype.addEventListener = originalAddEventListener;
    fontCachePrototype.useLocalID = originalUseLocalID;
  }
  assert.equal(prototype.addEventListener, originalAddEventListener,
    'Source Lite event prototype must be restored after capture');
  const provenance = {upstream:scope.upstream, profile:ref.provenance,
    scopeSha256:sha(fs.readFileSync(scopePath)), input:'MathML', host:'WeakMap-backed Node EventTarget per Lite receiver; actual callbacks registered and retained through SVG serialization; listeners removed after each case; no dispatch',
    entityModules:loadedEntityModules,
    casesSha256:sha(Buffer.from(JSON.stringify(cases)))};
  const evidencePath = path.join(root, 'build/math-port/maction-scene-source.json');
  fs.mkdirSync(path.dirname(evidencePath), {recursive:true});
  fs.writeFileSync(evidencePath, JSON.stringify({provenance,records}, null, 2)+'\n');
  const {writeObservations, flatMml} = require('./mathml-observation-data.cjs');
  const observations = {
    sourceMactionStyles: styleEntries.map(s => ({...s, numeric: s.kind === 'number'})),
    sourceMactionCases: records.map(r => ({
      name: r.name, mathml: r.mathml, display: r.display, em: r.em, ex: r.ex, width: r.width,
      font_cache: r.fontCache, local_id: r.localID ?? '',
      root_w: r.rootBBox.w, root_h: r.rootBBox.h, root_d: r.rootBBox.d,
      root_scale: r.rootBBox.scale, root_rscale: r.rootBBox.rscale,
      root_L: r.rootBBox.L, root_R: r.rootBBox.R, root_pwidth: String(r.rootBBox.pwidth ?? ''),
      local_cache_prefixes: r.localCachePrefixes, cache_summary: JSON.stringify(r.cacheObservation),
      svg: r.svg, listener_count: r.listeners.length, wrapper_count: r.maction.length,
      listeners: Array.from({length: 4}, (_, i) => r.listeners[i] ??
        {type: '', target: 0, host: 0, before: '', after: ''}),
      wrappers: Array.from({length: 4}, (_, i) => {
        const w = r.maction[i];
        return w ? {node_index: w.nodeIndex, action_type: w.actionType ?? '',
          selection: String(w.selection ?? ''), selected_kind: w.selectedKind,
          tip_dx: w.tipDx, tip_dy: w.tipDy, ...w.bbox, pwidth: String(w.bbox.pwidth ?? '')} :
          {node_index: 0, action_type: '', selection: '', selected_kind: '',
            tip_dx: 0, tip_dy: 0, w: 0, h: 0, d: 0, scale: 0, rscale: 0, L: 0, R: 0, pwidth: ''};
      }),
      ...flatMml(r.mml.nodes, r.mml.attributes)
    }))
  };
  writeObservations('maction', observations);
  process.stdout.write(`Verified ${records.length} pinned Source maction cases; evidence ${path.relative(root,evidencePath)}\n`);
}

main().catch(error => {process.stderr.write(error.stack+'\n'); process.exitCode=1;});
