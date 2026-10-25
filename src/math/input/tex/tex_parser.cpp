// MathJax 4.1.3 TexParser.ts; Apache-2.0. Scanner is the sole source cursor.
module folia.math.input.tex.parser;
import :tex_parser;
import :stack_item_factory;
import folia.math.input.tex.unit_util;
import std;

namespace folia::math::tex {
namespace {
std::u16string attribute(mml::MmlNode const &node, std::u16string_view name,
                         std::u16string_view fallback = {}) {
    if (auto value = node.attributes().lookup(name))
        if (auto text = std::get_if<std::u16string>(value))
            return *text;
    return std::u16string(fallback);
}
bool texWhitespace(char16_t c) noexcept {
    return (c >= 0x0009 && c <= 0x000D) || c == 0x0020 || c == 0x00A0 ||
           c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 ||
           c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000 || c == 0xFEFF;
}
bool letter(char16_t c) {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}
mml::MmlNode *child(mml::MmlNode &node, std::size_t index) {
    auto values = node.children();
    return index < values.size() ? values[index] : nullptr;
}
} // namespace
TexParser::ParserScope::ParserScope(ParseOptions &configuration, TexParser &parser)
    : configuration_(configuration), parser_(parser) {
    configuration_.pushParser(parser_);
}
TexParser::ParserScope::~ParserScope() noexcept {
    if (active_)
        configuration_.removeParser(parser_);
}
void TexParser::ParserScope::release() {
    if (active_) {
        configuration_.removeParser(parser_);
        active_ = false;
    }
}
TexParser::TexParser(std::u16string source, ParserEnvironmentSeed seed, ParseOptions &owner)
    : TexParser(std::move(source), std::move(seed), owner, DeferParsing{}) {
    initializeTexParser();
}
TexParser::TexParser(std::u16string source, ParserEnvironmentSeed seed, ParseOptions &owner,
                     DeferParsing)
    : TexParser(std::move(source), resolveParserEnvironmentSeed(std::move(seed)), owner,
                DeferParsing{}) {}
TexParser::TexParser(std::u16string source, ResolvedParserEnvironmentSeed seed, ParseOptions &owner,
                     DeferParsing)
    : scanner_(std::move(source)), configuration(owner), scope_(owner, *this),
      stack(*owner.itemFactory, std::move(seed.environment), seed.is_inner) {}
TexParser::~TexParser() = default;
void TexParser::initializeTexParser() {
    auto initial = stack.environmentScope();
    Parse();
    Push(itemFactory().create(StackKind::stop));
    stack.setEnv(std::move(initial));
}
void TexParser::finishParserScope() {
    scope_.release();
}
TexOptionValues &TexParser::options() const noexcept {
    return configuration.options;
}
StackItemFactory &TexParser::itemFactory() const noexcept {
    return *configuration.itemFactory;
}
Tags &TexParser::tags() const {
    if (!configuration.tags)
        throw std::logic_error("TeX tags have not been configured");
    return *configuration.tags;
}
void TexParser::setString(std::u16string source) {
    scanner_.replaceSource(std::move(source));
}
std::u16string_view TexParser::string() const noexcept {
    return scanner_.source();
}
std::u16string TexParser::currentCS() const {
    return std::u16string(scanner_.currentControlSequence());
}
void TexParser::setCurrentCS(std::u16string value) {
    scanner_.setCurrentControlSequence(std::move(value));
}
std::size_t TexParser::position() const noexcept {
    return scanner_.position();
}
void TexParser::setPosition(std::size_t value) noexcept {
    scanner_.setPosition(value);
}
SourceSnapshot TexParser::sourceSnapshot() const noexcept {
    return scanner_.sourceSnapshot();
}
DispatchOutcome TexParser::parse(HandlerKind kind, ParseInput const &input) {
    auto old = saveI;
    saveI = position() -
            ((kind == HandlerKind::character && input.token != u"&") ? input.token.size() : 0);
    auto handler = configuration.handlers.get(kind);
    if (!handler)
        throw std::logic_error("TeX handler domain has not been configured");
    auto result = handler->parse(input.parser, input.token);
    if (kind != HandlerKind::macro)
        updateResult(input.token, old);
    saveI = old;
    return result;
}
TokenLookup TexParser::lookup(HandlerKind kind, std::u16string_view token) {
    auto handler = configuration.handlers.get(kind);
    return handler ? handler->lookup(token) : TokenLookup{};
}
bool TexParser::contains(HandlerKind kind, std::u16string_view token) {
    auto handler = configuration.handlers.get(kind);
    return handler && handler->contains(token);
}
std::u16string TexParser::toString() const {
    std::u16string result;
    for (auto key : configuration.handlers.keys()) {
        switch (key) {
        case HandlerKind::character:
            result += u"character";
            break;
        case HandlerKind::delimiter:
            result += u"delimiter";
            break;
        case HandlerKind::macro:
            result += u"macro";
            break;
        case HandlerKind::environment:
            result += u"environment";
            break;
        }
        result += u": ";
        result += configuration.handlers.get(key)->toString();
        result += u'\n';
    }
    return result;
}
void TexParser::Parse() {
    while (position() < string().size()) {
        auto token = getCodePoint();
        setPosition(position() + token.size());
        (void)parse(HandlerKind::character, ParseInput{*this, std::move(token)});
    }
}
void TexParser::Push(StackEntry entry) {
    if (auto frame = std::get_if<FrameHandle>(&entry); frame && *frame) {
        (*frame)->startI = saveI;
        (*frame)->stopI = position();
        (*frame)->startStr = sourceSnapshot();
    }
    if (auto node = std::get_if<mml::MmlNode *>(&entry); node && *node && (*node)->isInferred()) {
        auto values =
            std::vector<mml::MmlNode *>((*node)->children().begin(), (*node)->children().end());
        PushAll(values);
    } else
        stack.Push(std::move(entry));
}
void TexParser::PushAll(std::span<StackEntry const> values) {
    for (auto const &entry : values)
        stack.Push(entry);
}
void TexParser::PushAll(std::span<mml::MmlNode *const> values) {
    for (auto entry : values)
        stack.Push(entry);
}
mml::MmlNode *TexParser::mml() {
    finishParserScope();
    auto top = stack.Top();
    if (!top || !top->isKind(StackKind::mml))
        return nullptr;
    auto node = top->First();
    auto latex = trimTex(std::u16string(string()));
    if (node && !latex.empty())
        node->attributes().set(u"data-latex", std::move(latex));
    return node;
}
std::optional<std::u16string> TexParser::convertDelimiter(std::u16string_view token) {
    auto value = lookup(HandlerKind::delimiter, token);
    if (auto found = std::get_if<Token const *>(&value); found && *found)
        return (*found)->character;
    return std::nullopt;
}
std::u16string TexParser::getCodePoint() {
    return scanner_.getCodePoint();
}
bool TexParser::nextIsSpace() {
    return scanner_.nextIsSpace();
}
std::u16string TexParser::GetNext() {
    return scanner_.getNext();
}
std::u16string TexParser::GetCS() {
    return scanner_.getCS();
}
std::optional<std::u16string> TexParser::GetArgument(std::u16string_view name, bool noneOK) {
    return scanner_.getArgument(name, noneOK);
}
std::optional<std::u16string> TexParser::GetBrackets(std::u16string_view name,
                                                     std::optional<std::u16string> fallback,
                                                     bool brackets) {
    return scanner_.getBrackets(name, std::move(fallback), brackets);
}
std::optional<std::u16string> TexParser::GetDelimiter(std::u16string_view name, bool braceOK) {
    auto token = GetNext();
    setPosition(position() + token.size());
    if (position() <= string().size()) {
        if (token == u"\\")
            token += GetCS();
        else if (token == u"{" && braceOK) {
            setPosition(position() - 1);
            token = UnitUtil::instance().trimSpaces(GetArgument(name).value());
        }
        if (contains(HandlerKind::delimiter, token))
            return convertDelimiter(token);
    }
    auto command = currentCS();
    throw TexError("MissingOrUnrecognizedDelim", u"Missing or unrecognized delimiter for %1",
                   {&command, 1});
}
std::u16string TexParser::GetDimen(std::u16string_view name) {
    auto &units = UnitUtil::instance();
    if (GetNext() == u"{") {
        if (auto dim = units.matchDimen(GetArgument(name).value()))
            return dim->value + dim->unit;
    } else if (auto dim =
                   units.matchDimen(string().substr(std::min(position(), string().size())), true)) {
        setPosition(position() + dim->matched_length);
        return dim->value + dim->unit;
    }
    auto command = currentCS();
    throw TexError("MissingDimOrUnits", u"Missing dimension or its units for %1", {&command, 1});
}
std::u16string TexParser::GetUpTo(std::u16string_view name, std::u16string_view token) {
    return scanner_.getUpTo(name, token);
}
mml::MmlNode *TexParser::ParseArg(std::u16string_view name) {
    return TexParser(GetArgument(name).value(), ParserEnvironmentSeed{stack.env()}, configuration)
        .mml();
}
mml::MmlNode *TexParser::ParseUpTo(std::u16string_view name, std::u16string_view token) {
    return TexParser(GetUpTo(name, token), ParserEnvironmentSeed{stack.env()}, configuration).mml();
}
std::optional<std::u16string> TexParser::GetDelimiterArg(std::u16string_view name) {
    auto token = UnitUtil::instance().trimSpaces(GetArgument(name).value());
    if (token.empty())
        return std::nullopt;
    if (contains(HandlerKind::delimiter, token))
        return token;
    auto command = currentCS();
    throw TexError("MissingOrUnrecognizedDelim", u"Missing or unrecognized delimiter for %1",
                   {&command, 1});
}
bool TexParser::GetStar() {
    return scanner_.getStar();
}
mml::MmlNode &TexParser::finishCreatedNode(mml::MmlNode &node) {
    if (node.isToken() && node.attributes().own(u"mathvariant")) {
        auto value = attribute(node, u"mathvariant");
        if (!value.empty() && value.front() == u'-')
            node.properties().tex.ignore_variant = true;
    }
    return node;
}
mml::MmlNode &TexParser::create(mml::NodeKind kind, std::span<mml::MmlNode *const> children,
                                NodeDefinition attributes, mml::MmlNode *text) {
    return finishCreatedNode(
        configuration.request().nodeFactory.makeNode(kind, children, std::move(attributes), text));
}
mml::MmlNode &TexParser::createToken(mml::NodeKind kind, std::optional<std::u16string_view> text,
                                     NodeDefinition attributes) {
    return finishCreatedNode(
        configuration.request().nodeFactory.makeToken(kind, text, std::move(attributes)));
}
mml::MmlNode &TexParser::createText(std::u16string_view text) {
    auto node = configuration.request().nodeFactory.makeText(text);
    if (!node)
        throw std::logic_error("Configured text creator returned no node");
    return finishCreatedNode(*node);
}
mml::MmlNode &TexParser::createError(std::u16string_view message) {
    return finishCreatedNode(configuration.request().nodeFactory.makeError(message));
}
std::u16string TexParser::trimTex(std::u16string tex) {
    std::size_t first{}, last = tex.size();
    while (first < last && texWhitespace(tex[first]))
        ++first;
    while (last > first && texWhitespace(tex[last - 1]))
        --last;

    std::size_t spaces{};
    if (last > first && last < tex.size() && tex[last - 1] == u'\\') {
        // UnitUtil.trimSpaces preserves a final ASCII space after any slash;
        // TexParser.trimTex additionally preserves one for an odd slash run.
        // These are independent Source rules (one escaped space yields two).
        spaces = tex.back() == u' ' ? 1 : 0;
        auto slashStart = last;
        while (slashStart > first && tex[slashStart - 1] == u'\\')
            --slashStart;
        spaces += (last - slashStart) % 2;
    }
    tex.resize(last);
    tex.erase(0, first);
    tex.append(spaces, u' ');
    return tex;
}
void TexParser::updateResult(std::u16string_view input, std::size_t old) {
    auto node = stack.Prev(true);
    if (!node)
        return;
    auto latex = attribute(*node, u"data-latex");
    if (node->attributes().lookup(u"data-latex-item")) {
        auto existing = attribute(*node, u"data-latex-item");
        if (latex.empty()) {
            if (input == u"}" || existing == u"}")
                composeBraces(*node);
            else
                node->attributes().set(u"data-latex", std::move(existing));
        }
        return;
    }
    old = std::max(old, saveI);
    auto source = string();
    auto start = std::min(old, source.size()), stop = std::min(position(), source.size());
    auto text = trimTex(old != position()
                            ? std::u16string(source.substr(start, stop > start ? stop - start : 0))
                            : std::u16string(input));
    if (text.empty() || text == latex || (input == u"\\" && text == u"\\"))
        return;
    if (text == u"_" || text == u"^")
        node->properties().tex.script_position =
            text == u"_" ? mml::ScriptPosition::subscript : mml::ScriptPosition::superscript;
    else if (auto position = node->properties().tex.script_position) {
        auto slot = *position == mml::ScriptPosition::superscript ? 2u : 1u;
        auto other = slot == 2 ? 1u : 2u;
        if (auto script = child(*node, slot)) {
            if (text == u"}")
                composeBraces(*script);
            else if (!script->attributes().own(u"data-latex"))
                script->attributes().set(u"data-latex", text);
        }
        std::u16string comp;
        if (auto script = child(*node, other)) {
            comp += other == 1 ? u'_' : u'^';
            comp += attribute(*script, u"data-latex", u"undefined");
        }
        comp += slot == 2 ? u'^' : u'_';
        composeLatex(*node, comp, 0, slot);
        return;
    } else if (text == u"}") {
        composeBraces(*node);
        return;
    }
    node->attributes().set(u"data-latex", std::move(text));
}
void TexParser::composeLatex(mml::MmlNode &node, std::u16string_view comp, std::size_t first,
                             std::size_t last) {
    auto left = child(node, first), right = child(node, last);
    if (!left || !right)
        return;
    node.attributes().set(u"data-latex", attribute(*left, u"data-latex") + std::u16string(comp) +
                                             attribute(*right, u"data-latex", u"undefined"));
}
void TexParser::composeBraces(mml::MmlNode &atom) {
    atom.attributes().set(u"data-latex", u"{" + composeBracedContent(atom) + u"}");
}
std::u16string TexParser::composeBracedContent(mml::MmlNode &atom) {
    std::u16string expr;
    if (auto row = child(atom, 0))
        for (auto item : row->children())
            if (item) {
                auto text = attribute(*item, u"data-latex");
                if (text.empty())
                    continue;
                if (!expr.empty() && letter(expr.back()) && letter(text.front()))
                    expr += u' ';
                expr += text;
            }
    return expr;
}
} // namespace folia::math::tex
