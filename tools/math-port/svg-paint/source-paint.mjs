// Independent projection of the pinned MathJax stylesheet onto an SVG export.
// Values come from upstream's actual style registry, never from native output.
import assert from 'node:assert/strict';

const selectors = {
  root: 'mjx-container[jax="SVG"] > svg',
  link: 'mjx-container[jax="SVG"] > svg a',
  errorText: 'g[data-mml-node="merror"] > g',
  errorBackground: 'g[data-mml-node="merror"] > rect[data-background]',
  tableLine: 'g[data-mml-node="mtable"] > line[data-line], svg[data-table] > g > line[data-line]',
  tableFrame: 'g[data-mml-node="mtable"] > rect[data-frame], svg[data-table] > g > rect[data-frame]',
  dashed: 'g[data-mml-node="mtable"] > .mjx-dashed, svg[data-table] > g > .mjx-dashed',
  dotted: 'g[data-mml-node="mtable"] > .mjx-dotted, svg[data-table] > g > .mjx-dotted',
  tableViewport: 'g[data-mml-node="mtable"] > g > svg',
  glyph: 'mjx-container[jax="SVG"] path[data-c], mjx-container[jax="SVG"] use[data-c]',
};

export function sourceStyles(profile) {
  const styleNode = profile.output.styleSheet(profile.document);
  const css = profile.adaptor.textContent(styleNode);
  const registry = profile.output.styleJson.styles;
  const rules = Object.fromEntries(Object.entries(selectors).map(([name, selector]) => {
    assert(registry[selector], `Missing pinned MathJax style: ${selector}`);
    return [name, {...registry[selector]}];
  }));
  assert.deepEqual(rules.tableLine, {'stroke-width': '70px', fill: 'none'});
  assert.deepEqual(rules.tableFrame, rules.tableLine);
  assert.deepEqual(rules.dashed, {'stroke-dasharray': '140'});
  assert.deepEqual(rules.dotted, {'stroke-linecap': 'round', 'stroke-dasharray': '0,140'});
  assert.deepEqual(rules.glyph, {'stroke-width': 3});
  return {css, rules};
}

export function materializeSourcePaint(adaptor, root, rules) {
  const kind = node => node && adaptor.kind(node);
  const has = (node, name) => node && adaptor.hasAttribute(node, name);
  const attr = (node, name) => node && adaptor.getAttribute(node, name);
  const isTable = node => kind(node) === 'g' && attr(node, 'data-mml-node') === 'mtable';
  const apply = (node, values) => {
    for (const [name, value] of Object.entries(values)) {
      // CSS declarations outrank presentation attributes. Keep existing inline
      // declarations unchanged: their higher specificity still wins after the
      // lower-priority presentation value becomes explicit.
      let text = String(value);
      if (name === 'stroke-width') text = text.replace(/px$/, '');
      if (name === 'stroke-dasharray') text = text.replaceAll(',', ' ');
      adaptor.setAttribute(node, name, text);
    }
  };
  const walk = (node, parent, grandparent, parentSvg = true) => {
    const tag = kind(node);
    if (!tag || tag === '#text') return;
    const svg = tag === 'svg' || (parentSvg && kind(parent) !== 'foreignObject');
    if (node === root) apply(node, {overflow: rules.root.overflow});
    if (svg && tag === 'a') apply(node, rules.link);
    if (svg && kind(parent) === 'g' && attr(parent, 'data-mml-node') === 'merror') {
      if (tag === 'g') apply(node, rules.errorText);
      if (tag === 'rect' && has(node, 'data-background')) apply(node, rules.errorBackground);
    }
    const tableChild = svg && (isTable(parent) ||
      (kind(parent) === 'g' && kind(grandparent) === 'svg' && has(grandparent, 'data-table')));
    if (tableChild) {
      if (tag === 'line' && has(node, 'data-line')) apply(node, rules.tableLine);
      if (tag === 'rect' && has(node, 'data-frame')) apply(node, rules.tableFrame);
      const classes = (attr(node, 'class') ?? '').split(/\s+/);
      if (classes.includes('mjx-dashed')) apply(node, rules.dashed);
      if (classes.includes('mjx-dotted')) apply(node, rules.dotted);
    }
    if (svg && tag === 'svg' && kind(parent) === 'g' && isTable(grandparent))
      apply(node, rules.tableViewport);
    if (svg && (tag === 'path' || tag === 'use') && has(node, 'data-c')) apply(node, rules.glyph);
    for (const child of adaptor.childNodes(node)) walk(child, node, parent, svg);
  };
  walk(root, null, null);
  return root;
}

export function svgHostCss(css) {
  // Keep every declaration and the child/descendant relationships. Only the
  // HTML container tag changes, so librsvg can apply the real host selectors.
  return css.replaceAll('mjx-container', 'svg[data-mathjax-host]');
}

export function paintCanvas(adaptor, svgRoot, metrics, css = '') {
  const root = adaptor.clone(svgRoot);
  // The caller first checks the renderer's dimensions independently. This only
  // establishes identical CSS-pixel embedding; no drawing node is transformed.
  for (const key of ['width', 'height']) adaptor.setAttribute(root, key, String(metrics[key]));
  adaptor.setAttribute(root, 'x', '4');
  adaptor.setAttribute(root, 'y', '4');
  const width = Math.ceil(metrics.width) + 8, height = Math.ceil(metrics.height) + 8;
  return `<svg xmlns="http://www.w3.org/2000/svg" data-mathjax-host="" jax="SVG" ` +
    `width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">` +
    (css ? `<style>${svgHostCss(css)}</style>` : '') + adaptor.serializeXML(root) + '</svg>';
}
