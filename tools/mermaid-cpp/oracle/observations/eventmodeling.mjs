// Read-only browser capture. Acceptance decisions are implemented in Python.
export default function inspect(source) {
  document.body.innerHTML = source;
  const root = document.querySelector('svg');
  const matrix = root.getScreenCTM().inverse();
  const box = element => {
    const {x, y, width, height} = element.getBBox();
    const transform = matrix.multiply(element.getScreenCTM());
    const a = new DOMPoint(x, y).matrixTransform(transform);
    const b = new DOMPoint(x + width, y + height).matrixTransform(transform);
    return [a.x, a.y, b.x - a.x, b.y - a.y];
  };
  const screenBox = rect => {
    const a = new DOMPoint(rect.left, rect.top).matrixTransform(matrix);
    const b = new DOMPoint(rect.right, rect.bottom).matrixTransform(matrix);
    return [a.x, a.y, b.x - a.x, b.y - a.y];
  };
  const rects = [...root.querySelectorAll('rect:not(.em-highlight)')].map(element => {
    const {fill, stroke, rx} = getComputedStyle(element);
    return {box: box(element), style: {fill, stroke, rx}};
  });
  const routes = [...root.querySelectorAll('path')].map(element => {
    const {fill, stroke, strokeWidth} = getComputedStyle(element);
    const transform = matrix.multiply(element.getScreenCTM());
    const points = [0, element.getTotalLength()].flatMap(length => {
      const point = element.getPointAtLength(length).matrixTransform(transform);
      return [point.x, point.y];
    });
    return {points, style: {fill, stroke, strokeWidth}};
  });
  const labels = [...root.querySelectorAll('.em-swimlane text')].map(element => {
    const {fontFamily, fontWeight, fontSize, fill} = getComputedStyle(element);
    return {box: box(element), text: element.textContent, style: {fontFamily, fontWeight, fontSize, fill}};
  });
  const payloads = [...root.querySelectorAll('foreignObject, .em-label')].map(element => {
    const native = element.classList.contains('em-label');
    const walker = document.createTreeWalker(element, NodeFilter.SHOW_TEXT);
    const runs = [];
    while (walker.nextNode()) {
      const node = walker.currentNode;
      const range = document.createRange();
      range.selectNodeContents(node);
      const boxes = [...range.getClientRects()].filter(rect => rect.width && rect.height).map(screenBox);
      if (!boxes.length) continue;
      const {fontFamily, fontSize, fontWeight, fontStyle, fill, color} = getComputedStyle(node.parentElement);
      runs.push({text: node.textContent, boxes, fontFamily: fontFamily === 'Consolas, monospace' ? 'monospace' : fontFamily,
        fontSize, fontWeight, fontStyle, color: native ? fill : color});
    }
    return {box: native ? ['x', 'y', 'width', 'height'].map(key => Number(element.dataset[key])) : box(element), runs};
  });
  const {x, y, width, height} = root.viewBox.baseVal;
  const marker = root.querySelector('marker');
  return {
    view: [x, y, width, height], rects, routes, labels, payloads,
    arrowhead: getComputedStyle(marker.querySelector('polygon')).fill,
  };
};
