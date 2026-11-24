from pathlib import Path
import difflib
root=Path.cwd()
out=root/'tools/math-port/candidates/math-cancellation-g147/math-cancellation-g147.patch'
changes={}

def edit(path, pairs):
    p=root/path
    if path in changes:
        before,after=changes[path]
    else:
        before=p.read_text(encoding='utf-8')
        after=before
    for old,new in pairs:
        if old not in after:
            raise RuntimeError(f'missing anchor in {path}: {old[:100]!r}')
        after=after.replace(old,new,1)
    changes[path]=(before,after)

edit('src/math/math.ixx', [
('export import folia.math.output.scene;\nimport std;', 'export import folia.math.output.scene;\nimport std;'),
('    double containerWidth=1280;\n};', '    double containerWidth=1280;\n    std::optional<std::chrono::steady_clock::time_point> deadline;\n};'),
('    RenderResult render(std::string_view,RenderOptions={});\n', '    RenderResult render(std::string_view,RenderOptions={});\n    RenderResult render(std::u16string_view,std::stop_token,RenderOptions);\n    RenderResult render(std::string_view,std::stop_token,RenderOptions);\n'),
('    output::scene::Scene renderScene(std::string_view,RenderOptions={});\n', '    output::scene::Scene renderScene(std::string_view,RenderOptions={});\n    output::scene::Scene renderScene(std::u16string_view,std::stop_token,RenderOptions);\n    output::scene::Scene renderScene(std::string_view,std::stop_token,RenderOptions);\n'),
('    std::string toSvg(std::string_view,RenderOptions={});\n', '    std::string toSvg(std::string_view,RenderOptions={});\n    std::string toSvg(std::u16string_view,std::stop_token,RenderOptions);\n    std::string toSvg(std::string_view,std::stop_token,RenderOptions);\n')])
# Thread-local request implementation is an internal module; tests import its testing namespace directly.
changes['src/math/cancellation.ixx']=(None,'''// Cooperative, request-scoped cancellation for native math rendering.\nexport module folia.math.cancellation;\nimport std;\n\nexport namespace folia::math::cancellation {\nenum class CheckpointStage { TeX, Layout, Scene, Serialization };\n\nclass Cancelled final : public std::exception {\npublic:\n    const char* what() const noexcept override { return "Math rendering cancelled"; }\n};\n\nclass Request final {\n    std::stop_token stop_;\n    std::optional<std::chrono::steady_clock::time_point> deadline_;\npublic:\n    explicit Request(std::stop_token stop = {},\n        std::optional<std::chrono::steady_clock::time_point> deadline = {}) noexcept\n        : stop_(stop), deadline_(deadline) {}\n    bool requested() const {\n        return stop_.stop_requested() ||\n            (deadline_ && std::chrono::steady_clock::now() >= *deadline_);\n    }\n    void checkpoint() const { if (requested()) throw Cancelled{}; }\n};\n\n// A request scope is thread-local and restores the prior scope when nested.\nclass Scope final {\n    Request const* previous_;\npublic:\n    explicit Scope(Request const&) noexcept;\n    ~Scope();\n    Scope(const Scope&) = delete;\n    Scope& operator=(const Scope&) = delete;\n};\nvoid checkpoint(CheckpointStage stage=CheckpointStage::TeX);\n\nnamespace testing {\nclass CheckpointObserverScope final {\n    std::function<void(CheckpointStage)> previous_;\npublic:\n    explicit CheckpointObserverScope(std::function<void(CheckpointStage)>);\n    ~CheckpointObserverScope();\n    CheckpointObserverScope(const CheckpointObserverScope&) = delete;\n    CheckpointObserverScope& operator=(const CheckpointObserverScope&) = delete;\n};\n}\n}\n''')
changes['src/math/cancellation.cpp']=(None,'''module folia.math.cancellation;\nimport std;\n\nnamespace folia::math::cancellation {\nnamespace {\nthread_local Request const* activeRequest=nullptr;\nthread_local std::function<void(CheckpointStage)> observer;\n}\nScope::Scope(Request const& request) noexcept : previous_(activeRequest) { activeRequest=&request; }\nScope::~Scope() { activeRequest=previous_; }\nvoid checkpoint(CheckpointStage stage) {\n    if(observer)observer(stage);\n    if(activeRequest)activeRequest->checkpoint();\n}\ntesting::CheckpointObserverScope::CheckpointObserverScope(\n    std::function<void(CheckpointStage)> value) : previous_(std::move(observer)) { observer=std::move(value); }\ntesting::CheckpointObserverScope::~CheckpointObserverScope() { observer=std::move(previous_); }\n}\n''')
edit('src/math/math.cpp', [
('import folia.math.output.svg.native_document;\n', 'import folia.math.output.svg.native_document;\nimport folia.math.cancellation;\n'),
('    std::shared_ptr<handlers::html::HTMLMathDocument> document;\n', '    std::shared_ptr<handlers::html::HTMLMathDocument> document;\n    bool unusable=false;\n'),
('RenderResult Renderer::render(std::u16string_view tex,RenderOptions options) {\n    try {return state_->render(tex,options);}', '''RenderResult Renderer::render(std::u16string_view tex,RenderOptions options) {
    return render(tex,std::stop_token{},options);
}
RenderResult Renderer::render(std::u16string_view tex,std::stop_token stop,RenderOptions options) {
    cancellation::Request request(stop,options.deadline);
    if(state_->unusable)
        throw RenderError("SessionUnusable","Native math renderer session is unusable after in-flight cancellation");
    try { request.checkpoint(); }
    catch(const cancellation::Cancelled&) { throw RenderError("Cancelled","Math rendering cancelled"); }
    cancellation::Scope scope(request);
    try {
        auto completed=state_->render(tex,options);
        request.checkpoint();
        return completed;
    } catch(const cancellation::Cancelled&) {
        state_->unusable=true;
        throw RenderError("Cancelled","Math rendering cancelled; native session is unusable");
    }
'''),
('RenderResult Renderer::render(std::string_view tex,RenderOptions options) {\n    return render(std::u16string_view(property_host::text(tex)),options);\n}\n', '''RenderResult Renderer::render(std::string_view tex,RenderOptions options) {
    return render(tex,std::stop_token{},options);
}
RenderResult Renderer::render(std::string_view tex,std::stop_token stop,RenderOptions options) {
    cancellation::Request request(stop,options.deadline);
    if(state_->unusable)
        throw RenderError("SessionUnusable","Native math renderer session is unusable after in-flight cancellation");
    try {
        request.checkpoint();
        cancellation::Scope scope(request);
        auto converted=property_host::text(tex);
        request.checkpoint();
        return render(std::u16string_view(converted),stop,options);
    } catch(const cancellation::Cancelled&) {
        throw RenderError("Cancelled","Math rendering cancelled");
    }
}
'''),
('''output::scene::Scene Renderer::renderScene(std::u16string_view tex,RenderOptions options) {
    try {return state_->renderScene(tex,options);}
    catch(const opts::Exception& error) {
        throw RenderError(error.name,property_host::debugText(error.message));
    }
}
output::scene::Scene Renderer::renderScene(std::string_view tex,RenderOptions options) {
    return renderScene(std::u16string_view(property_host::text(tex)),options);
}
''', '''output::scene::Scene Renderer::renderScene(std::u16string_view tex,RenderOptions options) {
    return renderScene(tex,std::stop_token{},options);
}
output::scene::Scene Renderer::renderScene(std::u16string_view tex,std::stop_token stop,RenderOptions options) {
    cancellation::Request request(stop,options.deadline);
    if(state_->unusable)
        throw RenderError("SessionUnusable","Native math renderer session is unusable after in-flight cancellation");
    try { request.checkpoint(); }
    catch(const cancellation::Cancelled&) { throw RenderError("Cancelled","Math rendering cancelled"); }
    cancellation::Scope scope(request);
    try {
        auto completed=state_->renderScene(tex,options);
        request.checkpoint();
        return completed;
    } catch(const cancellation::Cancelled&) {
        state_->unusable=true;
        throw RenderError("Cancelled","Math rendering cancelled; native session is unusable");
    } catch(const opts::Exception& error) {
        throw RenderError(error.name,property_host::debugText(error.message));
    }
}
output::scene::Scene Renderer::renderScene(std::string_view tex,RenderOptions options) {
    return renderScene(tex,std::stop_token{},options);
}
output::scene::Scene Renderer::renderScene(std::string_view tex,std::stop_token stop,RenderOptions options) {
    cancellation::Request request(stop,options.deadline);
    if(state_->unusable)
        throw RenderError("SessionUnusable","Native math renderer session is unusable after in-flight cancellation");
    try {
        request.checkpoint();
        cancellation::Scope scope(request);
        auto converted=property_host::text(tex);
        request.checkpoint();
        return renderScene(std::u16string_view(converted),stop,options);
    } catch(const cancellation::Cancelled&) {
        throw RenderError("Cancelled","Math rendering cancelled");
    }
}
'''),
('std::string Renderer::toSvg(std::string_view tex,RenderOptions options) {\n    return render(tex,options).svg;\n}\n', '''std::string Renderer::toSvg(std::string_view tex,RenderOptions options) {
    return render(tex,options).svg;
}
std::string Renderer::toSvg(std::u16string_view tex,std::stop_token stop,RenderOptions options) {
    return render(tex,stop,options).svg;
}
std::string Renderer::toSvg(std::string_view tex,std::stop_token stop,RenderOptions options) {
    return render(tex,stop,options).svg;
}
''')])
# Parser work: token loop + scan loops. Match current imports, which include the context-fields import.
edit('src/math/input/tex/tex_parser.cpp', [
('import folia.math.util.utf16;\n', 'import folia.math.util.utf16;\nimport folia.math.cancellation;\n'),
('ParseResult TexParser::parse(Property kind,const ParseInput& input){\n', 'ParseResult TexParser::parse(Property kind,const ParseInput& input){\n    cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n'),
('void TexParser::Parse(){\n    while(i<static_cast<double>(string_.size())){\n', 'void TexParser::Parse(){\n    while(i<static_cast<double>(string_.size())){\n        cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n'),
('while(i<static_cast<double>(string_.size())){\n            const char16_t c=string_[static_cast<std::size_t>(i++)];', 'while(i<static_cast<double>(string_.size())){\n            cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n            const char16_t c=string_[static_cast<std::size_t>(i++)];'),
('while(i<static_cast<double>(string_.size())){\n        const char16_t c=string_[static_cast<std::size_t>(i++)];', 'while(i<static_cast<double>(string_.size())){\n        cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n        const char16_t c=string_[static_cast<std::size_t>(i++)];'),
('while(i<static_cast<double>(string_.size())){\n        const double k=i;std::u16string c=GetNext();', 'while(i<static_cast<double>(string_.size())){\n        cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n        const double k=i;std::u16string c=GetNext();')])
edit('src/math/input/tex/tex_parser.cpp', [
('std::u16string TexParser::GetNext(){while(nextIsSpace())++i;return getCodePoint();}', 'std::u16string TexParser::GetNext(){while(nextIsSpace()){cancellation::checkpoint(cancellation::CheckpointStage::TeX);++i;}return getCodePoint();}'),
('std::u16string TexParser::GetCS(){\n', 'std::u16string TexParser::GetCS(){\n    cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n'),
("end=start+1;while(end<string_.size()&&((string_[end]>=u'a'&&string_[end]<=u'z')||(string_[end]>=u'A'&&string_[end]<=u'Z')))++end;", "end=start+1;while(end<string_.size()&&((string_[end]>=u'a'&&string_[end]<=u'z')||(string_[end]>=u'A'&&string_[end]<=u'Z'))){cancellation::checkpoint(cancellation::CheckpointStage::TeX);++end;}")])
edit('src/math/input/tex/newcommand/util.cpp', [
('import folia.math.input.tex.parser;\n', 'import folia.math.input.tex.parser;\nimport folia.math.cancellation;\n'),
('while (parser.i < static_cast<double>(source.size())) {\n', 'while (parser.i < static_cast<double>(source.size())) {\n    cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n')])
# A second scan handles optional arguments independently from macro replacement scanning.
edit('src/math/input/tex/newcommand/util.cpp', [('while (parser.i < static_cast<double>(source.size())) {\n    const char16_t c = source[static_cast<std::size_t>(parser.i)];', 'while (parser.i < static_cast<double>(source.size())) {\n    cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n    const char16_t c = source[static_cast<std::size_t>(parser.i)];')])
# UTF-8 decoding can be large; cancellation at input codepoint boundaries.
edit('src/math/util/utf16.ixx', [
('export import folia.math.util.utf16_types;\n', 'export import folia.math.util.utf16_types;\nimport folia.math.cancellation;\n'),
('    for (std::size_t i = 0; i < bytes.size();) {\n', '    for (std::size_t i = 0; i < bytes.size();) {\n        cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n'),
('    for (std::size_t i = 0; i < text.size();) {\n', '    for (std::size_t i = 0; i < text.size();) {\n        cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n')])
edit('src/math/util/property_host.ixx', [
('import folia.math.util.utf16;\n', 'import folia.math.util.utf16;\nimport folia.math.cancellation;\n'),
('    for(auto cp:utf16::codePoints(text)){\n', '    for(auto cp:utf16::codePoints(text)){\n        cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n'),
('    for(std::size_t i=0;i<bytes.size();){\n', '    for(std::size_t i=0;i<bytes.size();){\n        cancellation::checkpoint(cancellation::CheckpointStage::TeX);\n')])
edit('src/math/output/common/linebreak_visitor.cpp', [
('import folia.math.util.js_number;\n', 'import folia.math.util.js_number;\nimport folia.math.cancellation;\n'),
('std::any LinebreakVisitor::visitNode(CommonWrapper& wrapper,std::span<const std::any> args){\n', 'std::any LinebreakVisitor::visitNode(CommonWrapper& wrapper,std::span<const std::any> args){\n    cancellation::checkpoint(cancellation::CheckpointStage::Layout);\n'),
('while(!state.potential.empty()&&state.w>state.width){\n', 'while(!state.potential.empty()&&state.w>state.width){\n        cancellation::checkpoint(cancellation::CheckpointStage::Layout);\n')])
edit('src/math/output/common/wrapper.cpp', [
('module folia.math.output.common;\n', 'module folia.math.output.common;\nimport folia.math.cancellation;\n'),
('CommonWrapper::BBoxRef CommonWrapper::getBBox(bool save){\n    if(bboxComputed_)return bbox;\n', 'CommonWrapper::BBoxRef CommonWrapper::getBBox(bool save){\n    if(bboxComputed_)return bbox;\n    cancellation::checkpoint(cancellation::CheckpointStage::Layout);\n'),
('for(std::size_t index=0;index<children->size();++index){\n', 'for(std::size_t index=0;index<children->size();++index){\n            cancellation::checkpoint(cancellation::CheckpointStage::Layout);\n')])
edit('src/math/output/scene/wrapper.cpp', [
('import folia.math.util.property_host;\n', 'import folia.math.util.property_host;\nimport folia.math.cancellation;\n'),
('std::span<const NodeRef> parents){\n', 'std::span<const NodeRef> parents){\n    cancellation::checkpoint(cancellation::CheckpointStage::Layout);\n')])
edit('src/math/output/scene/scene.cpp', [
('import folia.math.util.property_host;\n', 'import folia.math.util.property_host;\nimport folia.math.cancellation;\n'),
('    if(!source)return {};\n', '    if(!source)return {};\n    cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n'),
('    const auto sourceAttributeKeys=source->attributes.keys();\n    target->attributes.reserve(sourceAttributeKeys.size());\n    for(const auto& key:sourceAttributeKeys){\n', '    const auto sourceAttributeKeys=source->attributes.keys();\n    target->attributes.reserve(sourceAttributeKeys.size());\n    for(const auto& key:sourceAttributeKeys){\n        cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n'),
('    const auto sourceChildren=source->children.snapshot();\n    target->children.reserve(sourceChildren.size());\n    for(const auto& child:sourceChildren)target->children.push_back(freezeNode(child,copies,outlines));\n', '    const auto sourceChildren=source->children.snapshot();\n    target->children.reserve(sourceChildren.size());\n    for(const auto& child:sourceChildren){\n        cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n        target->children.push_back(freezeNode(child,copies,outlines));\n    }\n'),
('    resources_.reserve(resources.size());\n    for(const auto& resource:resources){\n', '    resources_.reserve(resources.size());\n    for(const auto& resource:resources){\n        cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n')])
edit('src/math/output/scene/scene.cpp', [
('    auto frozen=std::make_shared<const GlyphOutline>(*outline);\n', '    cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n    auto frozen=std::make_shared<const GlyphOutline>(*outline);\n    cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n'),
('        frozen.text=text->currentText();\n', '        cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n        frozen.text=text->currentText();\n        cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n'),
('    source->applyGeometryOwner(currentGeometry,currentTransform,target->attributes);\n', '    cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n    source->applyGeometryOwner(currentGeometry,currentTransform,target->attributes);\n    cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n')])
edit('src/math/output/svg/serializer.cpp', [
('import folia.math.util.lengths;\n', 'import folia.math.util.lengths;\nimport folia.math.cancellation;\n'),
('DOMNodeRef Serializer::materialize(const FrozenNodeRef& source){\n', 'DOMNodeRef Serializer::materialize(const FrozenNodeRef& source){\n    cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n'),
('for(const auto& child:source->children)children.push(nodeProperty(materialize(child)));\n', 'for(const auto& child:source->children){\n        cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n        children.push(nodeProperty(materialize(child)));\n    }\n'),
('DOMNodeRef Serializer::materializeHtml(const HtmlNodePrimitive& source){\n', 'DOMNodeRef Serializer::materializeHtml(const HtmlNodePrimitive& source){\n    cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n'),
('    for(const auto& [key,value]:source.attributes)\n        attributes.set(property_host::debugText(key),Property(property_host::debugText(value)));\n', '    for(const auto& [key,value]:source.attributes){\n        cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n        attributes.set(property_host::debugText(key),Property(property_host::debugText(value)));\n    }\n'),
('for(const auto& child:source.children)children.push(nodeProperty(materializeHtml(child)));\n', 'for(const auto& child:source.children){\n        cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n        children.push(nodeProperty(materializeHtml(child)));\n    }\n'),
('    for(const auto& root:source.roots())roots.push_back(materialize(root));\n', '    for(const auto& root:source.roots()){\n        cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n        roots.push_back(materialize(root));\n    }\n')])
edit('src/math/adaptors/lite/parser.cpp', [
('std::u16string LiteParser::serialize(AbstractDOMAdaptor& adaptor, LiteNodeRef node, bool xml) {\n', 'std::u16string LiteParser::serialize(AbstractDOMAdaptor& adaptor, LiteNodeRef node, bool xml) {\n    cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n'),
('std::u16string LiteParser::serializeInner(AbstractDOMAdaptor& adaptor, LiteNodeRef node, bool xml) {\n', 'std::u16string LiteParser::serializeInner(AbstractDOMAdaptor& adaptor, LiteNodeRef node, bool xml) {\n    cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n'),
('        const auto callback=callback_([this,xml](options::Value item)->options::Value{\n', '        const auto callback=callback_([this,xml](options::Value item)->options::Value{\n            cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n'),
('        const auto callback=callback_([this,&adaptor,receiver,xml,rawText](options::Value child)->options::Value{\n', '        const auto callback=callback_([this,&adaptor,receiver,xml,rawText](options::Value child)->options::Value{\n            cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n'),
('        for(const auto& attribute:attributes){\n', '        for(const auto& attribute:attributes){\n            cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n'),
('        for(std::size_t i=0;i<length;++i){\n            if(!children.has(i))continue;\n            const auto child=propertyNode(children.get(i));', '        for(std::size_t i=0;i<length;++i){\n            cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n            if(!children.has(i))continue;\n            const auto child=propertyNode(children.get(i));'),
('    for(std::size_t i=0;i<length;++i){\n        if(!children.has(i))continue;\n        const auto childValue=propertyNode(children.get(i));', '    for(std::size_t i=0;i<length;++i){\n        cancellation::checkpoint(cancellation::CheckpointStage::Serialization);\n        if(!children.has(i))continue;\n        const auto childValue=propertyNode(children.get(i));')])
# Import cancellation in lite parser module source.
edit('src/math/adaptors/lite/parser.cpp', [('module folia.math.adaptors.lite.parser;\n','module folia.math.adaptors.lite.parser;\nimport folia.math.cancellation;\n')])

edit('src/math/output/svg/output_jax.cpp', [
('module folia.math.output.svg.output_jax;\n', 'module folia.math.output.svg.output_jax;\nimport folia.math.cancellation;\n'),
('    const std::array<scene::NodeRef,1> parents{flip};\n    (void)placements_.build(wrapper,builder,parents);', '    const std::array<scene::NodeRef,1> parents{flip};\n    cancellation::checkpoint(cancellation::CheckpointStage::Layout);\n    (void)placements_.build(wrapper,builder,parents);\n    cancellation::checkpoint(cancellation::CheckpointStage::Layout);'),
('    for(const auto& node:roots)builder.addRoot(node);\n    return std::make_shared<scene::Scene>(std::move(builder).finish());', '    for(const auto& node:roots)builder.addRoot(node);\n    cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n    auto sceneValue=std::make_shared<scene::Scene>(std::move(builder).finish());\n    cancellation::checkpoint(cancellation::CheckpointStage::Scene);\n    return sceneValue;')])

# CMake registrations: preserve surrounding work and make only additive changes.
edit('CMakeLists.txt', [
('    src/math/math.ixx\n', '    src/math/math.ixx\n    src/math/cancellation.ixx\n'),
('    src/math/math.cpp\n', '    src/math/math.cpp\n    src/math/cancellation.cpp\n'),
('    tests/math/context_raw_edge_trace.cpp\n', '    tests/math/integration/math_cancellation_g147.cpp\n    tests/math/context_raw_edge_trace.cpp\n')])

# Dedicated real Renderer tests with deterministic production checkpoints.
tests=(root/'tools/math-port/candidates/math-cancellation-g147/tests_g147.cpp').read_text(encoding='utf-8')
test_anchor='''    "successful_native_results_remain_complete_and_to_svg_matches_render"_test = [] {'''
test_extra=r'''    "render_scene_cancellation_never_publishes_partial_scene"_test = [] {
        Renderer renderer;
        std::stop_source preCancelled; preCancelled.request_stop();
        expect(eq(errorKind([&]{(void)renderer.renderScene(std::u16string_view(u"x"),preCancelled.get_token(),{});}),std::string("Cancelled")));
        RenderOptions expired; expired.deadline=std::chrono::steady_clock::now()-std::chrono::milliseconds(1);
        expect(eq(errorKind([&]{(void)renderer.renderScene(std::string_view("x"),{},expired);}),std::string("Cancelled")));
        expect(!renderer.renderScene(std::u16string_view(u"x")).roots().empty());

        Renderer active; std::stop_source source; unsigned sceneCheckpoints=0;
        std::optional<folia::math::output::scene::Scene> unpublished;
        CheckpointObserverScope observer([&](CheckpointStage stage){
            if(stage==CheckpointStage::Scene&&++sceneCheckpoints==2)source.request_stop();
        });
        expect(eq(errorKind([&]{unpublished.emplace(active.renderScene(std::u16string_view(u"\\frac{a}{b}"),source.get_token(),{}));}),std::string("Cancelled")));
        expect(!unpublished.has_value());
        expect(eq(errorKind([&]{(void)active.renderScene(std::string_view("x"),{},{});}),std::string("SessionUnusable")));
    };
'''
assert test_anchor in tests
tests=tests.replace(test_anchor,test_extra+test_anchor,1)
changes['tests/math/integration/math_cancellation_g147.cpp']=(None,tests)

parts=[]
for path,(before,after) in changes.items():
    if before is None:
        old=[]; new=after.splitlines(keepends=True)
    else:
        old=before.splitlines(keepends=True); new=after.splitlines(keepends=True)
    diff=list(difflib.unified_diff(old,new,fromfile='a/'+path,tofile='b/'+path,n=3))
    if diff:
        parts.extend(diff)
out.write_text(''.join(parts),encoding='utf-8',newline='')
print(f'wrote {out.relative_to(root)} with {len(changes)} paths and {sum(1 for _ in out.open(encoding="utf-8"))} lines')
