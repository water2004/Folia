// MathJax 2.7.9 extensions/TeX/mediawiki-texvc.js; Apache-2.0.
// Low-priority compatibility aliases preserve the active v4 package meanings.
module folia.math.input.tex.mediawiki_texvc.configuration;
import folia.math.input.tex.base_methods;
import folia.math.input.tex.parse_methods;
import std;

namespace folia::math::tex {
ConfigurationRef MediawikiTexvcConfiguration::initializeModule(ConfigurationHandler &registry,
                                                              MapHandler &maps) {
    maps.registerMap(std::make_shared<CommandMap>(CommandMapInit{MacroMapInit{
        {u"mediawiki-texvc-macros", std::nullopt}, {
#include "alias_data.inc"
        }}}));
    // Delimiters are a separate MathJax handler: macro expansion alone does not
    // make an alias legal after \left, \right, \middle or \bigl.
    maps.registerMap(std::make_shared<DelimiterMap>(CharacterMapInit{
        {u"mediawiki-texvc-delimiters", TokenMapParser{&ParseMethods::delimiter}},
        {Token(u"\\lang", u"\u27E8"), Token(u"\\rang", u"\u27E9")}}));
    ConfigurationDefinition definition;
    definition.handler = {{HandlerKind::macro, {u"mediawiki-texvc-macros"}},
                          {HandlerKind::delimiter, {u"mediawiki-texvc-delimiters"}}};
    definition.priority = 6; // Base/v4 packages (5) and user definitions (0/3) win.
    return Configuration::create(registry, u"mediawiki-texvc", std::move(definition));
}
} // namespace folia::math::tex
