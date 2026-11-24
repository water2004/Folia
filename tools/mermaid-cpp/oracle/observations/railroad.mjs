// Read-only browser capture. Acceptance decisions are implemented in Python.
export default (root) => {
    const measure = (element) => {
      const matrix = root.getScreenCTM().inverse().multiply(element.getScreenCTM());
      const { x, y, width, height } = element.getBBox();
      return [matrix.e + x, matrix.f + y, width, height];
    };
    const shapes = [...root.querySelectorAll('.railroad-rule :is(path, rect, text, circle)')];
    const view = root.viewBox.baseVal;
    return {
      view: [view.x, view.y, view.width, view.height],
      sizing: root.getAttribute('width') === '100%'
        ? ['responsive', parseFloat(root.style.maxWidth)]
        : ['fixed', Number(root.getAttribute('width')), Number(root.getAttribute('height'))],
      accessibility: [
        root.getAttribute('role'), root.getAttribute('aria-roledescription'),
        ...['aria-labelledby', 'aria-describedby'].map((key) =>
          document.getElementById(root.getAttribute(key))?.textContent ?? null),
      ],
      shapes: shapes.map((element) => {
        const style = getComputedStyle(element);
        const type = element.localName;
        const context = [];
        for (let parent = element; parent !== root; parent = parent.parentElement) {
          const name = parent.getAttribute('class');
          if (name) context.push(name);
        }
        const result = {
          identity: [type, context, type === 'text' ? element.textContent : ''],
          bounds: measure(element),
          style: [style.fill, style.stroke, style.strokeDasharray],
          numeric: [parseFloat(style.strokeWidth)],
        };
        if (type === 'text') {
          result.style.push(style.fontFamily, style.fontWeight, style.textAnchor, style.dominantBaseline);
          result.numeric.push(parseFloat(style.fontSize), element.x.baseVal[0].value, element.y.baseVal[0].value);
        } else if (type === 'rect') {
          result.numeric.push(element.rx.baseVal.value, element.ry.baseVal.value);
        } else if (type === 'path') {
          let arcParameter = -1;
          result.commands = [];
          for (const token of element.getAttribute('d').trim().split(/\s+/)) {
            if (/^[MLA]$/.test(token)) {
              result.commands.push(token);
              arcParameter = token === 'A' ? 0 : -1;
            } else if (arcParameter >= 2 && arcParameter <= 4) {
              result.commands.push(token);
              ++arcParameter;
            } else {
              result.numeric.push(Number(token));
              if (arcParameter >= 0) ++arcParameter;
            }
          }
        }
        return result;
      }),
    };
  };
