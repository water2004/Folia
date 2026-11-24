// Read-only browser capture. Acceptance decisions are implemented in Python.
export default function inspectTreeViewSvg(svg) {
  const view = svg.viewBox.baseVal;
  const inverse = svg.getScreenCTM().inverse();
  const box = (element) => {
    const bounds = element.getBBox();
    const matrix = inverse.multiply(element.getScreenCTM());
    const points = [
      [bounds.x, bounds.y],
      [bounds.x + bounds.width, bounds.y],
      [bounds.x, bounds.y + bounds.height],
      [bounds.x + bounds.width, bounds.y + bounds.height],
    ].map(([x, y]) => ({
      x: matrix.a * x + matrix.c * y + matrix.e,
      y: matrix.b * x + matrix.d * y + matrix.f,
    }));
    const xs = points.map((point) => point.x);
    const ys = points.map((point) => point.y);
    return {
      x: Math.min(...xs),
      y: Math.min(...ys),
      width: Math.max(...xs) - Math.min(...xs),
      height: Math.max(...ys) - Math.min(...ys),
    };
  };
  const labels = [...svg.querySelectorAll('.treeView-node-label, .treeView-node-description')]
    .map((element) => {
      const style = getComputedStyle(element);
      return {
        text: element.textContent,
        cls: element.classList.contains('treeView-node-description') ? 'description' : 'label',
        box: box(element),
        fill: style.fill,
        fontFamily: style.fontFamily,
        fontWeight: style.fontWeight,
        fontStyle: style.fontStyle,
      };
    });
  const lines = [...svg.querySelectorAll('.treeView-node-line')]
    .map((element) => ({
      ...Object.fromEntries(['x1', 'y1', 'x2', 'y2'].map((key) => [
        key, Number(element.getAttribute(key)),
      ])),
      stroke: getComputedStyle(element).stroke,
      strokeWidth: getComputedStyle(element).strokeWidth,
    }))
    .sort((left, right) => left.y1 - right.y1 || left.x1 - right.x1 ||
      left.y2 - right.y2 || left.x2 - right.x2);
  const highlights = [...svg.querySelectorAll('.treeView-highlight-bg')]
    .map((element) => {
      const style = getComputedStyle(element);
      return {
        box: box(element),
        fill: style.fill,
        stroke: style.stroke,
        strokeWidth: style.strokeWidth,
      };
    });
  const icons = [...svg.querySelectorAll('.treeView-node-icon')]
    .map((element) => ({
      color: getComputedStyle(element).color,
      body: [...element.querySelectorAll('path')].map((path) => ({
        d: path.getAttribute('d'),
        fill: getComputedStyle(path).fill,
        fillRule: getComputedStyle(path).fillRule,
        clipRule: getComputedStyle(path).clipRule,
      })),
    }));
  return {
    view: { x: view.x, y: view.y, width: view.width, height: view.height },
    labels,
    lines,
    highlights,
    icons,
  };
};
