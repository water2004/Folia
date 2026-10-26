#include "../support/folia_test.hpp"
import folia.math.output.common.font_data;
import folia.math.output.common.font_path;
import folia.math.output.svg.font_data;
import folia.math.output.scene.types;
import folia.math.fonts.newcm_font_module_provider;

namespace {
using namespace boost::ut;
using namespace folia::math::output::common;
using namespace folia::math::output::svg;
namespace output = folia::math::output;
using output::scene::ClosePath;
using output::scene::MoveTo;
using output::scene::LineTo;

suite font_repository_contract = [] {
    "NewCM request scalar and batch reads share stable loaded values"_test = [] {
        using namespace folia::math::fonts;
        const auto source = newcmFontSource();
        NewcmFontRequest request(source->tables);
        const SvgFontGlyphKey a{"normal", 0x41};
        const SvgFontGlyphKey b{"normal", 0x42};
        const SvgFontGlyphKey missing{"absent-variant", 0x41};
        const auto* first = request.glyph(a.variant, a.codepoint);
        expect(first != nullptr);
        const auto* svg = request.find(a);
        expect(svg != nullptr);
        const std::array<SvgFontGlyphKey, 5> keys{a, missing, b, a, missing};
        const auto values = request.read(keys);
        expect(values.size() == keys.size());
        expect(values[0] == values[3]);
        expect(values[1] == values[4]);
        expect(&*values[0]->glyph == first);
        expect(&*values[0]->svg == svg);
        expect(!values[1]->glyph.has_value());
        expect(!values[1]->svg.has_value());
        expect(request.loadedGlyphCount() == 3_u);
        expect(request.glyph(missing.variant, missing.codepoint) == nullptr);
        expect(request.find(missing) == nullptr);
        expect(request.glyph(a.variant, a.codepoint) == first);
        expect(request.find(a) == svg);
        expect(request.read(keys) == values);
        expect(request.read({}).empty());
        expect(request.loadedGlyphCount() == 3_u);
        NewcmFontRequest next(source->tables);
        expect(next.loadedGlyphCount() == 0_u);
    };
    "NewCM table lookup preserves extension and SVG alias precedence"_test = [] {
        using namespace folia::math::fonts;
        const std::array<NewcmFontGlyphRow, 2> base{{
            {.variant = "normal", .code = 0x41},
            {.variant = "normal", .code = 0x42}}};
        const std::array<NewcmFontGlyphRow, 1> extension{{
            {.variant = "normal", .code = 0x41}}};
        const std::array<NewcmSvgAliasRow, 1> aliases{{
            {"alias", 0x41, "normal", 0x42}}};
        const std::array<NewcmSvgAliasRow, 1> overrides{{
            {"alias", 0x41, "normal", 0x41}}};
        NewcmFontTables tables;
        tables.addRows(base);
        tables.addRows(extension);
        tables.addAliases(aliases);
        tables.addAliases(overrides);
        const std::array<SvgFontGlyphKey, 4> keys{{
            {"normal", 0x41}, {"normal", 0x42}, {"alias", 0x41}, {"missing", 0x41}}};
        const auto values = tables.lookup(keys);
        expect(values[0].glyph == &extension[0]);
        expect(values[0].svg == &extension[0]);
        expect(values[1].glyph == &base[1]);
        expect(values[1].svg == &base[1]);
        expect(values[2].glyph == nullptr);
        expect(values[2].svg == &extension[0]);
        expect(values[3].glyph == nullptr);
        expect(values[3].svg == nullptr);
    };
    "NewCM batch font bodies belong only to the active request"_test = [] {
        using namespace folia::math::fonts;
        std::weak_ptr<FontGlyphLookup const> requestLifetime;
        std::weak_ptr<output::scene::GlyphResource const> outlineLifetime;
        std::shared_ptr<output::scene::GlyphResource const> retained;
        {
            const auto bundle = makeNewcmFontData();
            auto request = std::dynamic_pointer_cast<NewcmFontRequest const>(
                bundle.fonts.face("MathJaxNewcm").glyphSource);
            expect(request != nullptr);
            if (!request) return;
            requestLifetime = request;
            expect(request->loadedGlyphCount() == 0_u);
            expect(request->contains("normal", 0x41));
            FontRepositoryBuilder duplicate;
            duplicate.addFace(bundle.fonts.face("MathJaxNewcm"));
            expect(throws<std::invalid_argument>([&] {
                duplicate.addGlyph("MathJaxNewcm", "normal", 0x41, FontGlyph{});
            }));
            expect(request->loadedGlyphCount() == 0_u);
            const std::array<SvgFontGlyphKey, 3> keys{{{"normal", 0x41}, {"bold", 0x42}, {"normal", 0x41}}};
            auto values = request->read(keys);
            expect(values.size() == 3_u);
            expect(values[0] == values[2]);
            expect(request->loadedGlyphCount() == 2_u);
            expect(values[0]->glyph.has_value());
            if (!values[0]->glyph) return;
            retained = values[0]->glyph->resource;
            outlineLifetime = retained;
        }
        expect(requestLifetime.expired());
        expect(!outlineLifetime.expired());
        expect(!retained->outline().outline.segments.empty());
        retained.reset();
        expect(outlineLifetime.expired());
    };
    "NewCM variant ranges preserve sparse module and alias overlays"_test = [] {
        using namespace folia::math::fonts;
        const std::array<NewcmFontGlyphRow, 4> base{{
            {.variant = "bold", .code = 1}, {.variant = "bold", .code = 9},
            {.variant = "normal", .code = 1}, {.variant = "normal", .code = 9}}};
        const std::array<NewcmFontGlyphRow, 2> extension{{
            {.variant = "bold", .code = 9}, {.variant = "normal", .code = 2}}};
        const std::array<NewcmSvgAliasRow, 3> aliases{{
            {"bold", 1, "normal", 9}, {"bold", 3, "normal", 1},
            {"normal", 1, "bold", 9}}};
        const std::array<NewcmSvgAliasRow, 1> overlay{{{"bold", 1, "normal", 2}}};
        NewcmFontTables tables;
        tables.addRows({});
        tables.addRows(base);
        tables.addRows(extension);
        tables.addAliases(aliases);
        tables.addAliases({});
        tables.addAliases(overlay);
        const std::array<SvgFontGlyphKey, 8> keys{{
            {"bold", 1}, {"bold", 3}, {"bold", 9}, {"normal", 1},
            {"normal", 2}, {"normal", 9}, {"bold", 5}, {"unknown", 1}}};
        const auto values = tables.lookup(keys);
        const std::array<NewcmGlyphRecord, 8> expected{{
            {&base[0], &extension[1]}, {nullptr, &base[2]},
            {&extension[0], &extension[0]}, {&base[2], &extension[0]},
            {&extension[1], &extension[1]}, {&base[3], &base[3]}, {}, {}}};
        for (std::size_t i = 0; i < keys.size(); ++i) {
            expect(values[i].glyph == expected[i].glyph);
            expect(values[i].svg == expected[i].svg);
            expect(tables.contains(keys[i]) == (expected[i].glyph != nullptr));
        }
        expect(tables.keys().size() == 5_u);
        expect(tables.aliases().size() == 3_u);
    };
    "font parameters distinguish required values from optional lookup"_test = [] {
        FontParameters parameters;
        expect(throws<std::out_of_range>([&] { (void)parameters.required("missing"); }));
        expect(!parameters.find("missing").has_value());
        parameters.values.emplace("axis_height", .25);
        expect(parameters.required("axis_height") == .25);
        expect(parameters.find("axis_height") == .25);
    };

    "SVG font catalog uses exact variant keys and immutable aliases"_test = [] {
        SvgFontResourcesBuilder builder;
        builder.add({"normal",0x41},{true,"MnormalZ",{}});
        builder.add({"bold",0x41},{true,"MboldZ",{}});
        builder.add({"normal",0x42},{false,"",{}});
        builder.add({"normal",0x43},{true,"",{}});
        builder.addAlias({"bold-fraktur",0x41},{"bold",0x41});
        auto resources = std::move(builder).freeze();
        expect(resources.find({"normal",0x41})->path_data == "MnormalZ");
        expect(resources.find({"bold",0x41})->path_data == "MboldZ");
        expect(resources.find({"bold-fraktur",0x41})->path_data == "MboldZ");
        expect(resources.find({"unknown",0x41}) == nullptr);
        expect(resources.find({"normal",0x42}) != nullptr);
        expect(!resources.find({"normal",0x42})->path_defined);
        expect(resources.find({"normal",0x43})->path_defined);
        expect(resources.find({"normal",0x43})->path_data.empty());
    };
    "font outlines decode once to normalized y-up typed geometry"_test = [] {
        const auto outline = decodeFontOutline("M0 0L1000 500C1000 750 500 1000 250 500Z");
        expect(outline.outline.segments.size() == 4_u);
        const auto* line = std::get_if<output::scene::LineTo>(&outline.outline.segments[1]);
        expect(line != nullptr);
        expect(line->to.x == 1.0);
        expect(line->to.y == .5);
        const auto sourceFragment = decodeFontOutline("M583 3L717 0Z",1000.0);
        expect(sourceFragment.outline.segments.size() == 3_u);
    };

    "font outline prefix decoder preserves complete geometry and diagnostics"_test = [] {
        using R = FontOutlineErrorReason;
        const auto partial = decodeFontOutlinePrefix("M0 0L10 10L20");
        expect(!partial.complete());
        expect(partial.error->reason == R::incomplete_segment);
        expect(partial.error->command == 'L');
        expect(partial.error->byte_offset == 13_u);
        expect(partial.outline.outline.segments.size() == 2_u);
        expect(std::get_if<ClosePath>(&partial.outline.outline.segments.back()) == nullptr);

        // Witness the four pinned malformed lexical shapes without repairing
        // or splitting positive integers: each keeps only its complete prefix.
        const auto cyrillic = decodeFontOutlinePrefix("M827 680L794 680L5190C526-19");
        expect(cyrillic.error->reason == R::incomplete_segment);
        expect(cyrillic.error->command == 'L');
        expect(cyrillic.outline.outline.segments.size() == 2_u);
        const auto greek = decodeFontOutlinePrefix("M0 0C1 2 3 4 5 6C5540C566.7 3");
        expect(greek.error->reason == R::incomplete_segment);
        expect(greek.error->command == 'C');
        expect(greek.outline.outline.segments.size() == 2_u);
        const auto repeatedMove = decodeFontOutlinePrefix("M351.60L627.50L635.5 157");
        expect(repeatedMove.error->reason == R::incomplete_segment);
        expect(repeatedMove.error->command == 'M');
        expect(repeatedMove.outline.outline.segments.size() == 0_u);
        const auto secondCubic = decodeFontOutlinePrefix("M0 0C1 2 3 4 5 6C104.10C159.4 76.1");
        expect(secondCubic.error->reason == R::incomplete_segment);
        expect(secondCubic.error->command == 'C');
        expect(secondCubic.outline.outline.segments.size() == 2_u);

        expect(throws<std::invalid_argument>([] { (void)decodeFontOutline("M0 0L10"); }));
        expect(throws<std::invalid_argument>([] { (void)decodeFontOutlinePrefix("M0 0",0.0); }));
        const auto noCommand = decodeFontOutlinePrefix("0 0");
        expect(noCommand.error->reason == R::missing_command);
        const auto initialLine = decodeFontOutlinePrefix("L0 0");
        expect(initialLine.error->reason == R::missing_command);
        expect(initialLine.error->command == 'L');
        expect(initialLine.outline.outline.segments.empty());
        const auto initialClose = decodeFontOutlinePrefix("Z");
        expect(initialClose.error->reason == R::missing_command);
        expect(initialClose.error->command == 'Z');
        expect(initialClose.outline.outline.segments.empty());
        const auto unsupported = decodeFontOutlinePrefix("M0 0H10 10");
        expect(unsupported.error->reason == R::unsupported_command);
        expect(unsupported.outline.outline.segments.size() == 1_u);
        const auto invalidNumber = decodeFontOutlinePrefix("M0 0L1e 2");
        expect(invalidNumber.error->reason == R::invalid_number);
        expect(invalidNumber.outline.outline.segments.size() == 1_u);
        const auto overflow = decodeFontOutlinePrefix("M1e308 0l1e308 0");
        expect(overflow.error->reason == R::invalid_number);
        expect(overflow.outline.outline.segments.size() == 1_u);
        const auto badComma = decodeFontOutlinePrefix("M0,,0");
        expect(badComma.error->reason == R::invalid_number);
        expect(badComma.outline.outline.segments.empty());
    };

    "font outline decoder honors plus signs repeats and closepath current point"_test = [] {
        const auto definedEmpty = decodeFontOutlinePrefix("");
        expect(definedEmpty.complete());
        expect(definedEmpty.outline.outline.segments.empty());
        const auto decoded = decodeFontOutlinePrefix(
            "M+1 +2 3 4l+5 +6z l+7 +8M10 10L11 11 12 12");
        expect(decoded.complete());
        auto const& segments = decoded.outline.outline.segments;
        expect(segments.size() == 8_u);
        auto const* first = std::get_if<MoveTo>(&segments[0]);
        expect(first != nullptr);
        expect(first->to == output::scene::Point{.001,.002});
        auto const* implicitMoveLine = std::get_if<LineTo>(&segments[1]);
        expect(implicitMoveLine != nullptr);
        expect(implicitMoveLine->to == output::scene::Point{.003,.004});
        auto const* afterClose = std::get_if<LineTo>(&segments[4]);
        expect(afterClose != nullptr);
        expect(afterClose->to == output::scene::Point{.008,.01});
        expect(std::get_if<ClosePath>(&segments[3]) != nullptr);
        expect(std::get_if<LineTo>(&segments[7]) != nullptr);
    };

    "frozen font data is immutable and glyph resources are shared"_test = [] {
        FontRepositoryBuilder builder;
        FontFaceData face; face.name = "Probe";
        FontVariant variant; variant.name = "normal";
        auto outline = output::scene::GlyphOutline{};
        outline.outline.segments.emplace_back(output::scene::MoveTo{{output::scene::Unit{0},output::scene::Unit{0}}});
        auto resource = output::scene::GlyphResource::make("Probe", std::move(outline));
        variant.glyphs.emplace(0x41, FontGlyph{.metrics={.h=.7,.d=0,.w=.5},.resource=resource});
        variant.remap.emplace(0x1D400, GlyphReference{"normal", 0x41});
        face.variants.emplace("normal", std::move(variant));
        face.delimiters.emplace(0x28, DelimiterRecipe{.direction=StretchDirection::vertical,.sizes={1,2},.variants={0x239B}});
        builder.addFace(std::move(face));
        auto repository = std::move(builder).freeze();
        expect(repository.faceCount() == 1_u);
        expect(repository.glyphCount() == 2_u);
        const auto* glyph = repository.glyph("Probe","normal",0x41);
        expect(glyph != nullptr);
        expect(glyph->resource == resource);
        expect(repository.remap("Probe","normal",0x1D400).value_or(GlyphReference{} ) == GlyphReference{"normal",0x41});
        expect(repository.glyph("Probe","normal",0x1D400) == glyph);
        const auto* delimiter = repository.delimiter("Probe",0x28);
        expect(delimiter != nullptr);
        expect(delimiter->direction == StretchDirection::vertical);
        expect(repository.glyph("Probe","normal",0x42) == nullptr);
    };

    "SMP references resolve only their declared variant and codepoint"_test = [] {
        FontRepositoryBuilder builder;
        FontFaceData face; face.name = "Exact";
        FontVariant exactNormal; exactNormal.name="normal"; exactNormal.cache_id="Exact-N";
        FontVariant exactBold; exactBold.name="bold"; exactBold.cache_id="Exact-B";
        face.variants.emplace("normal",std::move(exactNormal));
        face.variants.emplace("bold",std::move(exactBold));
        builder.addFace(std::move(face));
        FontGlyph normal; normal.metrics.w = .25;
        FontGlyph bold; bold.metrics.w = .75;
        builder.addGlyph("Exact", "normal", 0x41, normal);
        builder.addGlyph("Exact", "bold", 0x41, bold);
        builder.addGlyph("Exact", "bold", 0x1D400, bold);
        builder.addRemap("Exact", "bold", 0x1D400, GlyphReference{"bold", 0x41});
        auto repository = std::move(builder).freeze();
        expect(repository.glyph("Exact", "bold", 0x1D400)->metrics.w == .75);
        expect(repository.remap("Exact", "bold", 0x1D400)->variant == "bold");
        const auto resolved = repository.resolveGlyph("Exact", "bold", 0x1D400);
        expect(resolved.has_value());
        expect(resolved->key.codepoint == 0x41);
        expect(resolved->key.variant == "bold");
        expect(resolved->key.variant_cache_id == "Exact-B");
        expect(resolved->data->metrics.w == .75);
        expect(repository.glyph("Exact", "italic", 0x1D400) == nullptr);
    };

    "Source linked variants overlay only linked own glyphs before recursive inherit"_test = [] {
        FontRepositoryBuilder builder;
        FontFaceData face; face.name = "Inheritance";
        builder.addFace(std::move(face));
        FontGlyph inherited; inherited.metrics.w = .4;
        FontGlyph linked; linked.metrics.w = .6;
        FontGlyph linkedFallback; linkedFallback.metrics.w = .7;
        FontGlyph requestedFallback; requestedFallback.metrics.w = .8;
        builder.addGlyph("Inheritance", "base", 0x41, inherited);
        builder.addGlyph("Inheritance", "linked", 0x41, linked);
        builder.addGlyph("Inheritance", "base", 0x42, linkedFallback);
        builder.addGlyph("Inheritance", "requested", 0x42, requestedFallback);
        builder.setVariantInheritance("Inheritance", "linked", "base");
        builder.setVariantInheritance("Inheritance", "derived", "requested", "linked");
        auto repository = std::move(builder).freeze();
        expect(repository.glyph("Inheritance", "derived", 0x41)->metrics.w == .6);
        expect(repository.glyph("Inheritance", "derived", 0x42)->metrics.w == .8);
    };

    "Source direct glyph ownership blocks inherited remap while overlay wins first"_test = [] {
        FontRepositoryBuilder builder;
        FontFaceData face;
        face.name = "RemapPrecedence";
        builder.addFace(std::move(face));

        FontGlyph baseGlyph;
        baseGlyph.metrics.w = .3;
        FontGlyph ownGlyph;
        ownGlyph.metrics.w = .4;
        FontGlyph overlayGlyph;
        overlayGlyph.metrics.w = .5;
        FontGlyph explicitGlyph;
        explicitGlyph.metrics.w = .6;
        builder.addGlyph("RemapPrecedence", "base", 0x69, baseGlyph);
        builder.addGlyph("RemapPrecedence", "direct", 0x69, ownGlyph);
        builder.addGlyph("RemapPrecedence", "linked", 0x6A, overlayGlyph);
        builder.addGlyph("RemapPrecedence", "explicit", 0x69, explicitGlyph);

        // The inherited base mapping models a generated SMP entry. A concrete
        // character record in the requested variant must own the slot.
        builder.addRemap("RemapPrecedence", "base", 0x69, GlyphReference{"base", 0x1D456});
        // Preserve an explicitly declared mapping at the requested level.
        builder.addRemap("RemapPrecedence", "explicit", 0x69, GlyphReference{"explicit", 0x1D456});
        // A linked overlay's own mapping/glyph is consulted before the
        // requested variant's inherited chain, matching Source's shallow link.
        builder.addRemap("RemapPrecedence", "requested", 0x6A,
                         GlyphReference{"requested", 0x1D457});
        builder.addRemap("RemapPrecedence", "requested", 0x6B,
                         GlyphReference{"requested", 0x1D458});
        builder.addRemap("RemapPrecedence", "linked", 0x6B, GlyphReference{"linked", 0x1D458});
        builder.setVariantInheritance("RemapPrecedence", "direct", "base");
        builder.setVariantInheritance("RemapPrecedence", "explicit", "base");
        builder.setVariantInheritance("RemapPrecedence", "overlay-glyph", "requested", "linked");
        builder.setVariantInheritance("RemapPrecedence", "overlay-remap", "requested", "linked");

        auto repository = std::move(builder).freeze();
        expect(repository.glyph("RemapPrecedence", "direct", 0x69)->metrics.w == .4);
        expect(!repository.remap("RemapPrecedence", "direct", 0x69).has_value());
        expect(repository.remap("RemapPrecedence", "explicit", 0x69) ==
               GlyphReference{"explicit", 0x1D456});
        expect(repository.glyph("RemapPrecedence", "overlay-glyph", 0x6A)->metrics.w == .5);
        expect(!repository.remap("RemapPrecedence", "overlay-glyph", 0x6A).has_value());
        expect(repository.remap("RemapPrecedence", "overlay-remap", 0x6B) ==
               GlyphReference{"linked", 0x1D458});
    };

    "duplicate faces are rejected before publication"_test = [] {
        FontRepositoryBuilder builder;
        FontFaceData first; first.name="Probe"; builder.addFace(first);
        expect(throws<std::invalid_argument>([&] { builder.addFace(first); }));
    };

    "pinned NewCM repository retains complete source profile and extensions"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        const auto& repository = bundle.fonts;
        const auto coverage = folia::math::fonts::newcmProviderCoverage(repository);
        expect(repository.faceCount() == 1_u);
        expect(repository.face("MathJaxNewcm").sourceProfileGlyphCount() == 24646_u);
        expect(coverage.source_dynamic_module_count == 40_u);
        expect(coverage.source_provider_trace_events == 80_u);
        expect(coverage.native_actual_direct_glyph_records == repository.face("MathJaxNewcm").glyphCount() - coverage.native_actual_remap_records);
        expect(coverage.native_actual_glyph_resource_objects == 0_u);
        const auto* capitalA = repository.glyph("MathJaxNewcm", "normal", 0x41);
        expect(capitalA != nullptr);
        expect(capitalA->metrics.h == .716);
        expect(capitalA->metrics.d == 0);
        expect(capitalA->metrics.w == .75);
        expect(capitalA->resource != nullptr);
        expect(folia::math::fonts::newcmProviderCoverage(repository).native_actual_glyph_resource_objects == 1_u);
        const auto* svgA = bundle.svg_resources.find({"normal",0x41});
        expect(svgA != nullptr);
        expect(svgA->path_defined);
        expect(svgA->path_data.starts_with("M"));
        expect(svgA->path_data.ends_with("Z"));
        expect(capitalA->resource->font_id() == "MathJaxNewcm/normal");
        expect(!capitalA->resource->outline().outline.segments.empty());
        const auto normalExclamation=repository.resolveGlyph("MathJaxNewcm","normal",0x21);
        const auto boldExclamation=repository.resolveGlyph("MathJaxNewcm","bold",0x21);
        expect(normalExclamation.has_value());
        expect(boldExclamation.has_value());
        expect(normalExclamation->key.variant == "normal");
        expect(boldExclamation->key.variant == "bold");
        expect(normalExclamation->key.variant_cache_id == "NCM-N");
        expect(boldExclamation->key.variant_cache_id == "NCM-B");
        expect(normalExclamation->key.codepoint == boldExclamation->key.codepoint);
        expect(normalExclamation->data->resource != boldExclamation->data->resource);
        expect(bundle.svg_resources.find({normalExclamation->key.variant,normalExclamation->key.codepoint})->path_data !=
               bundle.svg_resources.find({boldExclamation->key.variant,boldExclamation->key.codepoint})->path_data);
        const auto bbmBold = repository.resolveGlyph("MathJaxNewcm", "-bbm-bold", 0x21);
        const auto bboldx = repository.resolveGlyph("MathJaxNewcm", "-bboldx", 0x21);
        expect(bbmBold.has_value());
        expect(bboldx.has_value());
        if (!bbmBold || !bboldx) return;
        expect(bbmBold->key.variant == "-bbm-bold");
        expect(bboldx->key.variant == "-bboldx");
        expect(bbmBold->key.codepoint == 0x21);
        expect(bboldx->key.codepoint == 0x21);
        expect(bbmBold->key.variant_cache_id.empty());
        expect(bboldx->key.variant_cache_id.empty());
        expect(repository.face("MathJaxNewcm").variants.at("-bbm-bold").cache_id ==
               repository.face("MathJaxNewcm").variants.at("-bboldx").cache_id);
        const auto* bbmBoldPath = bundle.svg_resources.find({"-bbm-bold", 0x21});
        const auto* bboldxPath = bundle.svg_resources.find({"-bboldx", 0x21});
        expect(bbmBoldPath != nullptr);
        expect(bboldxPath != nullptr);
        if (!bbmBoldPath || !bboldxPath) return;
        expect(bbmBoldPath->path_defined);
        expect(bboldxPath->path_defined);
        expect(bbmBoldPath->path_data.starts_with("M254 634C254 673 218 705"));
        expect(bboldxPath->path_data.starts_with("M292 647C292 680 288 683"));
        expect(bbmBoldPath->path_data != bboldxPath->path_data);
        expect(bbmBold->data->resource != bboldx->data->resource);
        expect(bbmBold->data->resource->font_id() == "MathJaxNewcm/bold");
        expect(bboldx->data->resource->font_id() == "MathJaxNewcm/-bboldx");
        expect(!bbmBold->data->resource->outline().outline.segments.empty());
        expect(!bboldx->data->resource->outline().outline.segments.empty());
        const auto* boldSourceA = repository.glyph("MathJaxNewcm", "bold", 0x41);
        expect(boldSourceA != nullptr);
        expect(repository.remap("MathJaxNewcm", "bold", 0x41) == GlyphReference{"bold", 0x1D400});
        const auto* boldCapitalA = repository.glyph("MathJaxNewcm", "bold", 0x1D400);
        expect(boldCapitalA != nullptr);
        expect(boldCapitalA->metrics.h == .698);
        expect(boldCapitalA->metrics.w == .869);
        // Requested cache key and shared outline provenance are distinct.
        expect(boldCapitalA->resource->font_id() == "MathJaxNewcm/normal");
        expect(boldCapitalA->resource != capitalA->resource);
        const auto* normalAStart = std::get_if<output::scene::MoveTo>(
            &capitalA->resource->outline().outline.segments.front());
        const auto* boldAStart = std::get_if<output::scene::MoveTo>(
            &boldCapitalA->resource->outline().outline.segments.front());
        expect(normalAStart != nullptr);
        expect(boldAStart != nullptr);
        expect(boldAStart->to != normalAStart->to);
        const auto boldAlphaRef = repository.remap("MathJaxNewcm", "bold", 0x03B1);
        expect(boldAlphaRef == GlyphReference{"bold", 0x1D6C2});
        const auto* boldAlpha = repository.glyph("MathJaxNewcm", "bold", 0x03B1);
        expect(boldAlpha != nullptr);
        expect(boldAlpha->metrics.h == .452);
        expect(boldAlpha->metrics.d == .008);
        expect(boldAlpha->metrics.w == .743);
        const auto* boldAlphaTarget = repository.glyph("MathJaxNewcm", "bold", 0x1D6C2);
        expect(boldAlphaTarget != nullptr);
        expect(boldAlpha->resource == boldAlphaTarget->resource);
        const auto* boldFrakturSpace = repository.glyph("MathJaxNewcm", "bold-fraktur", 0x20);
        expect(boldFrakturSpace != nullptr);
        expect(boldFrakturSpace->metrics.w == .383);
        expect(repository.delimiter("MathJaxNewcm", 0x28) != nullptr);
        const auto& face = repository.face("MathJaxNewcm");
        expect(face.extension_glyph_counts.at("mathjax-bbm-font-extension") == 290_u);
        expect(face.extension_glyph_counts.at("mathjax-bboldx-font-extension") == 391_u);
        expect(face.extension_glyph_counts.at("mathjax-dsfont-font-extension") == 60_u);
        expect(repository.glyph("MathJaxNewcm", "-bbm-normal", 0x41) != nullptr);
        expect(repository.glyph("MathJaxNewcm", "-bbm-normal", 0x41)->resource->font_id() ==
               "MathJaxNewcm/-bbm-normal");
        expect(repository.remap("MathJaxNewcm", "-ds-rm", 0x41) ==
               GlyphReference{"-ds-rm", 0x1D538});
    };
};
}
