const {mathjax} = require('@mathjax/src/js/mathjax.js');
const {TeX} = require('@mathjax/src/js/input/tex.js');
const {SVG} = require('@mathjax/src/js/output/svg.js');
const {MathJaxNewcmFont} = require('@mathjax/mathjax-newcm-font/js/svg.js');
const {liteAdaptor} = require('@mathjax/src/js/adaptors/liteAdaptor.js');
const {RegisterHTMLHandler} = require('@mathjax/src/js/handlers/html.js');
require('@mathjax/src/js/input/tex/base/BaseConfiguration.js');
require('@mathjax/src/js/input/tex/action/ActionConfiguration.js');
require('@mathjax/src/js/input/tex/ams/AmsConfiguration.js');
require('@mathjax/src/js/input/tex/amscd/AmsCdConfiguration.js');
require('@mathjax/src/js/input/tex/bbm/BbmConfiguration.js');
require('@mathjax/src/js/input/tex/bboldx/BboldxConfiguration.js');
require('@mathjax/src/js/input/tex/bbox/BboxConfiguration.js');
require('@mathjax/src/js/input/tex/begingroup/BegingroupConfiguration.js');
require('@mathjax/src/js/input/tex/boldsymbol/BoldsymbolConfiguration.js');
require('@mathjax/src/js/input/tex/braket/BraketConfiguration.js');
require('@mathjax/src/js/input/tex/bussproofs/BussproofsConfiguration.js');
require('@mathjax/src/js/input/tex/cancel/CancelConfiguration.js');
require('@mathjax/src/js/input/tex/cases/CasesConfiguration.js');
require('@mathjax/src/js/input/tex/centernot/CenternotConfiguration.js');
require('@mathjax/src/js/input/tex/color/ColorConfiguration.js');
require('@mathjax/src/js/input/tex/colortbl/ColortblConfiguration.js');
require('@mathjax/src/js/input/tex/configmacros/ConfigMacrosConfiguration.js');
require('@mathjax/src/js/input/tex/dsfont/DsfontConfiguration.js');
require('@mathjax/src/js/input/tex/empheq/EmpheqConfiguration.js');
require('@mathjax/src/js/input/tex/enclose/EncloseConfiguration.js');
require('@mathjax/src/js/input/tex/extpfeil/ExtpfeilConfiguration.js');
require('@mathjax/src/js/input/tex/fontsizev3/FontSizeV3Configuration.js');
require('@mathjax/src/js/input/tex/gensymb/GensymbConfiguration.js');
require('@mathjax/src/js/input/tex/html/HtmlConfiguration.js');
require('@mathjax/src/js/input/tex/mathtools/MathtoolsConfiguration.js');
require('@mathjax/src/js/input/tex/mhchem/MhchemConfiguration.js');
require('@mathjax/src/js/input/tex/newcommand/NewcommandConfiguration.js');
require('@mathjax/src/js/input/tex/noundefined/NoUndefinedConfiguration.js');
require('@mathjax/src/js/input/tex/physics/PhysicsConfiguration.js');
require('@mathjax/src/js/input/tex/setoptions/SetOptionsConfiguration.js');
require('@mathjax/src/js/input/tex/tagformat/TagFormatConfiguration.js');
require('@mathjax/src/js/input/tex/texhtml/TexHtmlConfiguration.js');
require('@mathjax/src/js/input/tex/textcomp/TextcompConfiguration.js');
require('@mathjax/src/js/input/tex/textmacros/TextMacrosConfiguration.js');
require('@mathjax/src/js/input/tex/unicode/UnicodeConfiguration.js');
require('@mathjax/src/js/input/tex/units/UnitsConfiguration.js');
require('@mathjax/src/js/input/tex/upgreek/UpgreekConfiguration.js');
require('@mathjax/src/js/input/tex/verb/VerbConfiguration.js');

const adaptor = liteAdaptor({fontSize: 16});
RegisterHTMLHandler(adaptor);
mathjax.asyncLoad = name => globalThis.FoliaLoadMathJaxModule(String(name));
mathjax.asyncIsSynchronous = true;
globalThis.FoliaMathJaxFont = MathJaxNewcmFont;
const input = new TeX({
  // Register the non-conflicting MathJax v4 TeX syntax packages eagerly.  The
  // dynamic `autoload` and `require` loaders are intentionally unnecessary in
  // the offline QuickJS runtime; `noerrors` would also hide Folia's red errors,
  // and `colorv2` is the legacy alternative to the active `color` package.
  packages: [
    'base', 'action', 'ams', 'amscd', 'bbm', 'bboldx', 'bbox', 'begingroup',
    'boldsymbol', 'braket', 'bussproofs', 'cancel', 'cases', 'centernot',
    'color', 'colortbl', 'configmacros', 'dsfont', 'empheq', 'enclose',
    'extpfeil', 'fontsizev3', 'gensymb', 'html', 'mathtools', 'mhchem',
    'newcommand', 'noundefined', 'physics', 'setoptions', 'tagformat',
    'texhtml', 'textcomp', 'textmacros', 'unicode', 'units', 'upgreek', 'verb'
  ]
});
const output = new SVG({
  fontData: MathJaxNewcmFont,
  fontCache: 'none',
  exFactor: 0.5,
  displayOverflow: 'linebreak',
  linebreaks: {inline: true}
});
const document = mathjax.document('', {InputJax: input, OutputJax: output});

globalThis.FoliaMathJax = {
  render(tex, display, em, width) {
    const node = document.convert(String(tex), {
      display: Boolean(display),
      em: Number(em) || 16,
      ex: (Number(em) || 16) * 0.5,
      containerWidth: Number(width) || 1280
    });
    return adaptor.serializeXML(node);
  }
};
