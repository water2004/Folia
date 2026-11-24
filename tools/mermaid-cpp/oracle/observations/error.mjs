// Read-only browser capture shared by official generation and native observation.
export default function observeErrorSvg() {
  const svg = document.querySelector('svg');
  const { x, y, width, height } = svg.viewBox.baseVal;
  const shape = (element) => {
    const paint = getComputedStyle(element);
    const box = element.getBBox();
    return {
      box: [box.x, box.y, box.width, box.height],
      fill: paint.fill,
      stroke: paint.stroke,
    };
  };
  return {
    view: [x, y, width, height],
    width: svg.getAttribute('width'),
    maxWidth: svg.style.maxWidth,
    paths: [...svg.querySelectorAll('.error-icon')].map((element) => ({
      ...shape(element),
      d: element.getAttribute('d'),
    })),
    labels: [...svg.querySelectorAll('.error-text')].map((element) => ({
      ...shape(element),
      text: element.textContent,
      anchor: getComputedStyle(element).textAnchor,
    })),
  };
};
