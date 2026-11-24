// Browser capture only: shared by official fixture generation and Python validation.
export default function inspectVenn(root = document) {
  const svg = root.querySelector('svg');
  const view = svg.viewBox.baseVal;
  const matrix = element => svg.getCTM().inverse().multiply(element.getCTM());
  const box = element => {
    const {x, y, width, height} = element.getBBox();
    const position = new DOMPoint(x, y).matrixTransform(matrix(element));
    return [position.x, position.y, width, height];
  };
  const label = element => {
    const css = getComputedStyle(element);
    return {
      text: element.textContent, box: box(element), fill: css.fill,
      size: css.fontSize, family: css.fontFamily, anchor: css.textAnchor,
      baseline: css.dominantBaseline,
    };
  };
  const pathInfo = path => {
    const css = getComputedStyle(path);
    const length = path.getTotalLength();
    const transform = matrix(path);
    const samples = Array.from({length: length ? 128 : 0}, (_, i) => {
      const p = path.getPointAtLength(length * i / 128).matrixTransform(transform);
      return [p.x, p.y];
    });
    return {
      box: box(path), length, samples,
      fill: css.fill, fillOpacity: css.fillOpacity, stroke: css.stroke,
      strokeWidth: css.strokeWidth, strokeOpacity: css.strokeOpacity,
    };
  };
  const regions = [...svg.querySelectorAll('.venn-area')].map(region => ({
    sets: region.getAttribute('data-venn-sets'), classes: region.getAttribute('class'),
    paths: [...region.querySelectorAll('path')].map(pathInfo),
    label: label(region.querySelector('text')),
  }));
  const title = svg.querySelector('.venn-title');
  const textNodes = [...svg.querySelectorAll('.venn-text-node-fo, text.venn-text-node')].map(element => {
    const portable = element.tagName === 'text';
    const display = portable ? element : element.firstChild;
    const css = getComputedStyle(display);
    let cell, lines;
    if (portable) {
      const [x, y, width, height] = element.dataset.vennCell.split(' ').map(Number);
      const p = new DOMPoint(x, y).matrixTransform(matrix(element));
      cell = [p.x, p.y, width, height];
      lines = [...element.children].map(span => ({text: span.textContent, box: box(span)}));
    } else {
      cell = box(element);
      const inverse = svg.getScreenCTM().inverse();
      const text = display.firstChild;
      const rows = [];
      for (let i = 0; text && i < text.length; ++i) {
        const range = document.createRange();
        range.setStart(text, i);
        range.setEnd(text, i + 1);
        const rect = range.getBoundingClientRect();
        if (!rect.width && !rect.height) continue;
        let row = rows.find(r => Math.abs(r.top - rect.top) < .01);
        if (!row) rows.push(row = {text: '', left: rect.left, top: rect.top, right: rect.right, bottom: rect.bottom});
        row.text += text.data[i];
        if (/\S/u.test(text.data[i])) row.right = Math.max(row.right, rect.right);
      }
      lines = rows.map(row => {
        const top = new DOMPoint(row.left, row.top).matrixTransform(inverse);
        const bottom = new DOMPoint(row.right, row.bottom).matrixTransform(inverse);
        return {text: row.text.trim(), box: [top.x, top.y, bottom.x - top.x, bottom.y - top.y]};
      });
    }
    return {cell, lines, color: portable ? css.fill : css.color, size: css.fontSize, family: css.fontFamily};
  });
  const debug = [...svg.querySelectorAll('.venn-text-debug-circle, .venn-text-debug-cell')].map(box);
  return {
    view: [view.x, view.y, view.width, view.height], regions, textNodes, debug,
    title: title ? label(title) : null, width: svg.getAttribute('width'),
    height: svg.getAttribute('height'), maxWidth: svg.style.maxWidth,
  };
};
