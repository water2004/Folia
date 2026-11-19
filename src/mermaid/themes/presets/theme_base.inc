// Generated from pinned official Mermaid 12.2.0 by generate-themes.mjs. MIT.
// Ordered assignments preserve constructor/update behavior; do not edit by hand.
void Theme::construct_base() {
    set(Value(u"background"), Value(u"#f4f4f4"));
    set(Value(u"primaryColor"), Value(u"#fff4dd"));
    set(Value(u"noteBkgColor"), Value(u"#fff5ad"));
    set(Value(u"noteTextColor"), Value(u"#333"));
    set(Value(u"THEME_COLOR_LIMIT"), Value(12));
    set(Value(u"radius"), Value(5));
    set(Value(u"strokeWidth"), Value(1));
    set(Value(u"fontFamily"), Value(u"\"trebuchet ms\", verdana, arial, sans-serif"));
    set(Value(u"fontSize"), Value(u"16px"));
    set(Value(u"useGradient"), Value(true));
    set(Value(u"dropShadow"), Value(u"drop-shadow( 1px 2px 2px rgba(185,185,185,1))"));
}
void Theme::update_base() {
    set(Value(u"primaryTextColor"), fallback(get(Value(u"primaryTextColor")), [&] { return (truthy(get(Value(u"darkMode"))) ? Value(u"#eee") : Value(u"#333")); }));
    set(Value(u"secondaryColor"), fallback(get(Value(u"secondaryColor")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", negate(Value(120))}})); }));
    set(Value(u"tertiaryColor"), fallback(get(Value(u"tertiaryColor")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(180)}, {u"l", Value(5)}})); }));
    set(Value(u"primaryBorderColor"), fallback(get(Value(u"primaryBorderColor")), [&] { return mkBorder(get(Value(u"primaryColor")), get(Value(u"darkMode"))); }));
    set(Value(u"secondaryBorderColor"), fallback(get(Value(u"secondaryBorderColor")), [&] { return mkBorder(get(Value(u"secondaryColor")), get(Value(u"darkMode"))); }));
    set(Value(u"tertiaryBorderColor"), fallback(get(Value(u"tertiaryBorderColor")), [&] { return mkBorder(get(Value(u"tertiaryColor")), get(Value(u"darkMode"))); }));
    set(Value(u"noteBorderColor"), fallback(get(Value(u"noteBorderColor")), [&] { return mkBorder(get(Value(u"noteBkgColor")), get(Value(u"darkMode"))); }));
    set(Value(u"noteBkgColor"), fallback(get(Value(u"noteBkgColor")), [&] { return Value(u"#fff5ad"); }));
    set(Value(u"noteTextColor"), fallback(get(Value(u"noteTextColor")), [&] { return Value(u"#333"); }));
    set(Value(u"secondaryTextColor"), fallback(get(Value(u"secondaryTextColor")), [&] { return invert(get(Value(u"secondaryColor"))); }));
    set(Value(u"tertiaryTextColor"), fallback(get(Value(u"tertiaryTextColor")), [&] { return invert(get(Value(u"tertiaryColor"))); }));
    set(Value(u"lineColor"), fallback(get(Value(u"lineColor")), [&] { return invert(get(Value(u"background"))); }));
    set(Value(u"arrowheadColor"), fallback(get(Value(u"arrowheadColor")), [&] { return invert(get(Value(u"background"))); }));
    set(Value(u"textColor"), fallback(get(Value(u"textColor")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"border2"), fallback(get(Value(u"border2")), [&] { return get(Value(u"tertiaryBorderColor")); }));
    set(Value(u"nodeBkg"), fallback(get(Value(u"nodeBkg")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"mainBkg"), fallback(get(Value(u"mainBkg")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"nodeBorder"), fallback(get(Value(u"nodeBorder")), [&] { return get(Value(u"primaryBorderColor")); }));
    set(Value(u"clusterBkg"), fallback(get(Value(u"clusterBkg")), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"clusterBorder"), fallback(get(Value(u"clusterBorder")), [&] { return get(Value(u"tertiaryBorderColor")); }));
    set(Value(u"defaultLinkColor"), fallback(get(Value(u"defaultLinkColor")), [&] { return get(Value(u"lineColor")); }));
    set(Value(u"titleColor"), fallback(get(Value(u"titleColor")), [&] { return get(Value(u"tertiaryTextColor")); }));
    set(Value(u"edgeLabelBackground"), fallback(get(Value(u"edgeLabelBackground")), [&] { return (truthy(get(Value(u"darkMode"))) ? darken(get(Value(u"secondaryColor")), Value(30)) : get(Value(u"secondaryColor"))); }));
    set(Value(u"flowContainerStroke"), fallback(get(Value(u"flowContainerStroke")), [&] { return get(Value(u"secondaryBorderColor")); }));
    set(Value(u"nodeTextColor"), fallback(get(Value(u"nodeTextColor")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"actorBorder"), fallback(get(Value(u"actorBorder")), [&] { return get(Value(u"primaryBorderColor")); }));
    set(Value(u"actorBkg"), fallback(get(Value(u"actorBkg")), [&] { return get(Value(u"mainBkg")); }));
    set(Value(u"actorTextColor"), fallback(get(Value(u"actorTextColor")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"actorLineColor"), fallback(get(Value(u"actorLineColor")), [&] { return get(Value(u"actorBorder")); }));
    set(Value(u"labelBoxBkgColor"), fallback(get(Value(u"labelBoxBkgColor")), [&] { return get(Value(u"actorBkg")); }));
    set(Value(u"signalColor"), fallback(get(Value(u"signalColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"signalTextColor"), fallback(get(Value(u"signalTextColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"labelBoxBorderColor"), fallback(get(Value(u"labelBoxBorderColor")), [&] { return get(Value(u"actorBorder")); }));
    set(Value(u"labelTextColor"), fallback(get(Value(u"labelTextColor")), [&] { return get(Value(u"actorTextColor")); }));
    set(Value(u"loopTextColor"), fallback(get(Value(u"loopTextColor")), [&] { return get(Value(u"actorTextColor")); }));
    set(Value(u"activationBorderColor"), fallback(get(Value(u"activationBorderColor")), [&] { return darken(get(Value(u"secondaryColor")), Value(10)); }));
    set(Value(u"activationBkgColor"), fallback(get(Value(u"activationBkgColor")), [&] { return get(Value(u"secondaryColor")); }));
    set(Value(u"sequenceNumberColor"), fallback(get(Value(u"sequenceNumberColor")), [&] { return invert(get(Value(u"lineColor"))); }));
    set(Value(u"rectBkgColor"), fallback(get(Value(u"rectBkgColor")), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"sectionBkgColor"), fallback(get(Value(u"sectionBkgColor")), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"altSectionBkgColor"), fallback(get(Value(u"altSectionBkgColor")), [&] { return Value(u"white"); }));
    set(Value(u"sectionBkgColor"), fallback(get(Value(u"sectionBkgColor")), [&] { return get(Value(u"secondaryColor")); }));
    set(Value(u"sectionBkgColor2"), fallback(get(Value(u"sectionBkgColor2")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"excludeBkgColor"), fallback(get(Value(u"excludeBkgColor")), [&] { return Value(u"#eeeeee"); }));
    set(Value(u"taskBorderColor"), fallback(get(Value(u"taskBorderColor")), [&] { return get(Value(u"primaryBorderColor")); }));
    set(Value(u"taskBkgColor"), fallback(get(Value(u"taskBkgColor")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"activeTaskBorderColor"), fallback(get(Value(u"activeTaskBorderColor")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"activeTaskBkgColor"), fallback(get(Value(u"activeTaskBkgColor")), [&] { return lighten(get(Value(u"primaryColor")), Value(23)); }));
    set(Value(u"gridColor"), fallback(get(Value(u"gridColor")), [&] { return Value(u"lightgrey"); }));
    set(Value(u"doneTaskBkgColor"), fallback(get(Value(u"doneTaskBkgColor")), [&] { return Value(u"lightgrey"); }));
    set(Value(u"doneTaskBorderColor"), fallback(get(Value(u"doneTaskBorderColor")), [&] { return Value(u"grey"); }));
    set(Value(u"critBorderColor"), fallback(get(Value(u"critBorderColor")), [&] { return Value(u"#ff8888"); }));
    set(Value(u"critBkgColor"), fallback(get(Value(u"critBkgColor")), [&] { return Value(u"red"); }));
    set(Value(u"todayLineColor"), fallback(get(Value(u"todayLineColor")), [&] { return Value(u"red"); }));
    set(Value(u"vertLineColor"), fallback(get(Value(u"vertLineColor")), [&] { return Value(u"navy"); }));
    set(Value(u"taskTextColor"), fallback(get(Value(u"taskTextColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"taskTextOutsideColor"), fallback(get(Value(u"taskTextOutsideColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"taskTextLightColor"), fallback(get(Value(u"taskTextLightColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"taskTextColor"), fallback(get(Value(u"taskTextColor")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"taskTextDarkColor"), fallback(get(Value(u"taskTextDarkColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"taskTextClickableColor"), fallback(get(Value(u"taskTextClickableColor")), [&] { return Value(u"#003163"); }));
    set(Value(u"noteFontWeight"), fallback(get(Value(u"noteFontWeight")), [&] { return Value(u"normal"); }));
    set(Value(u"fontWeight"), fallback(get(Value(u"fontWeight")), [&] { return Value(u"normal"); }));
    set(Value(u"personBorder"), fallback(get(Value(u"personBorder")), [&] { return get(Value(u"primaryBorderColor")); }));
    set(Value(u"personBkg"), fallback(get(Value(u"personBkg")), [&] { return get(Value(u"mainBkg")); }));
    if (truthy(get(Value(u"darkMode")))) {
        set(Value(u"rowOdd"), fallback(fallback(get(Value(u"rowOdd")), [&] { return darken(get(Value(u"mainBkg")), Value(5)); }), [&] { return Value(u"#ffffff"); }));
        set(Value(u"rowEven"), fallback(get(Value(u"rowEven")), [&] { return darken(get(Value(u"mainBkg")), Value(10)); }));
    } else {
        set(Value(u"rowOdd"), fallback(fallback(get(Value(u"rowOdd")), [&] { return lighten(get(Value(u"mainBkg")), Value(75)); }), [&] { return Value(u"#ffffff"); }));
        set(Value(u"rowEven"), fallback(get(Value(u"rowEven")), [&] { return lighten(get(Value(u"mainBkg")), Value(5)); }));
    }
    set(Value(u"transitionColor"), fallback(get(Value(u"transitionColor")), [&] { return get(Value(u"lineColor")); }));
    set(Value(u"transitionLabelColor"), fallback(get(Value(u"transitionLabelColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"stateLabelColor"), fallback(fallback(get(Value(u"stateLabelColor")), [&] { return get(Value(u"stateBkg")); }), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"stateBkg"), fallback(get(Value(u"stateBkg")), [&] { return get(Value(u"mainBkg")); }));
    set(Value(u"labelBackgroundColor"), fallback(get(Value(u"labelBackgroundColor")), [&] { return get(Value(u"stateBkg")); }));
    set(Value(u"compositeBackground"), fallback(fallback(get(Value(u"compositeBackground")), [&] { return get(Value(u"background")); }), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"altBackground"), fallback(get(Value(u"altBackground")), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"compositeTitleBackground"), fallback(get(Value(u"compositeTitleBackground")), [&] { return get(Value(u"mainBkg")); }));
    set(Value(u"compositeBorder"), fallback(get(Value(u"compositeBorder")), [&] { return get(Value(u"nodeBorder")); }));
    set(Value(u"innerEndBackground"), get(Value(u"nodeBorder")));
    set(Value(u"errorBkgColor"), fallback(get(Value(u"errorBkgColor")), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"errorTextColor"), fallback(get(Value(u"errorTextColor")), [&] { return get(Value(u"tertiaryTextColor")); }));
    set(Value(u"transitionColor"), fallback(get(Value(u"transitionColor")), [&] { return get(Value(u"lineColor")); }));
    set(Value(u"specialStateColor"), get(Value(u"lineColor")));
    set(Value(u"cScale0"), fallback(get(Value(u"cScale0")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"cScale1"), fallback(get(Value(u"cScale1")), [&] { return get(Value(u"secondaryColor")); }));
    set(Value(u"cScale2"), fallback(get(Value(u"cScale2")), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"cScale3"), fallback(get(Value(u"cScale3")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(30)}})); }));
    set(Value(u"cScale4"), fallback(get(Value(u"cScale4")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(60)}})); }));
    set(Value(u"cScale5"), fallback(get(Value(u"cScale5")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(90)}})); }));
    set(Value(u"cScale6"), fallback(get(Value(u"cScale6")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(120)}})); }));
    set(Value(u"cScale7"), fallback(get(Value(u"cScale7")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(150)}})); }));
    set(Value(u"cScale8"), fallback(get(Value(u"cScale8")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(210)}, {u"l", Value(150)}})); }));
    set(Value(u"cScale9"), fallback(get(Value(u"cScale9")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(270)}})); }));
    set(Value(u"cScale10"), fallback(get(Value(u"cScale10")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(300)}})); }));
    set(Value(u"cScale11"), fallback(get(Value(u"cScale11")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(330)}})); }));
    if (truthy(get(Value(u"darkMode")))) {
        for (int i = 0; i < color_limit(); ++i) {
            set(add(Value(u"cScale"), Value(i)), darken(get(add(Value(u"cScale"), Value(i))), Value(75)));
        }
    } else {
        for (int i = 0; i < color_limit(); ++i) {
            set(add(Value(u"cScale"), Value(i)), darken(get(add(Value(u"cScale"), Value(i))), Value(25)));
        }
    }
    for (int i = 0; i < color_limit(); ++i) {
        set(add(Value(u"cScaleInv"), Value(i)), fallback(get(add(Value(u"cScaleInv"), Value(i))), [&] { return invert(get(add(Value(u"cScale"), Value(i)))); }));
    }
    for (int i = 0; i < color_limit(); ++i) {
        if (truthy(get(Value(u"darkMode")))) {
            set(add(Value(u"cScalePeer"), Value(i)), fallback(get(add(Value(u"cScalePeer"), Value(i))), [&] { return lighten(get(add(Value(u"cScale"), Value(i))), Value(10)); }));
        } else {
            set(add(Value(u"cScalePeer"), Value(i)), fallback(get(add(Value(u"cScalePeer"), Value(i))), [&] { return darken(get(add(Value(u"cScale"), Value(i))), Value(10)); }));
        }
    }
    set(Value(u"scaleLabelColor"), fallback(get(Value(u"scaleLabelColor")), [&] { return get(Value(u"labelTextColor")); }));
    for (int i = 0; i < color_limit(); ++i) {
        set(add(Value(u"cScaleLabel"), Value(i)), fallback(get(add(Value(u"cScaleLabel"), Value(i))), [&] { return get(Value(u"scaleLabelColor")); }));
    }
    const auto multiplier = (truthy(get(Value(u"darkMode"))) ? negate(Value(4)) : negate(Value(1)));
    for (int i = 0; i < 5; ++i) {
        set(add(Value(u"surface"), Value(i)), fallback(get(add(Value(u"surface"), Value(i))), [&] { return adjust(get(Value(u"mainBkg")), object({{u"h", Value(180)}, {u"s", negate(Value(15))}, {u"l", multiply(multiplier, add(Value(5), multiply(Value(i), Value(3))))}})); }));
        set(add(Value(u"surfacePeer"), Value(i)), fallback(get(add(Value(u"surfacePeer"), Value(i))), [&] { return adjust(get(Value(u"mainBkg")), object({{u"h", Value(180)}, {u"s", negate(Value(15))}, {u"l", multiply(multiplier, add(Value(8), multiply(Value(i), Value(3))))}})); }));
    }
    set(Value(u"classText"), fallback(get(Value(u"classText")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"fillType0"), fallback(get(Value(u"fillType0")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"fillType1"), fallback(get(Value(u"fillType1")), [&] { return get(Value(u"secondaryColor")); }));
    set(Value(u"fillType2"), fallback(get(Value(u"fillType2")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(64)}})); }));
    set(Value(u"fillType3"), fallback(get(Value(u"fillType3")), [&] { return adjust(get(Value(u"secondaryColor")), object({{u"h", Value(64)}})); }));
    set(Value(u"fillType4"), fallback(get(Value(u"fillType4")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", negate(Value(64))}})); }));
    set(Value(u"fillType5"), fallback(get(Value(u"fillType5")), [&] { return adjust(get(Value(u"secondaryColor")), object({{u"h", negate(Value(64))}})); }));
    set(Value(u"fillType6"), fallback(get(Value(u"fillType6")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(128)}})); }));
    set(Value(u"fillType7"), fallback(get(Value(u"fillType7")), [&] { return adjust(get(Value(u"secondaryColor")), object({{u"h", Value(128)}})); }));
    set(Value(u"pie1"), fallback(get(Value(u"pie1")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"pie2"), fallback(get(Value(u"pie2")), [&] { return get(Value(u"secondaryColor")); }));
    set(Value(u"pie3"), fallback(get(Value(u"pie3")), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"pie4"), fallback(get(Value(u"pie4")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"l", negate(Value(10))}})); }));
    set(Value(u"pie5"), fallback(get(Value(u"pie5")), [&] { return adjust(get(Value(u"secondaryColor")), object({{u"l", negate(Value(10))}})); }));
    set(Value(u"pie6"), fallback(get(Value(u"pie6")), [&] { return adjust(get(Value(u"tertiaryColor")), object({{u"l", negate(Value(10))}})); }));
    set(Value(u"pie7"), fallback(get(Value(u"pie7")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", numeric(Value(60))}, {u"l", negate(Value(10))}})); }));
    set(Value(u"pie8"), fallback(get(Value(u"pie8")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", negate(Value(60))}, {u"l", negate(Value(10))}})); }));
    set(Value(u"pie9"), fallback(get(Value(u"pie9")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(120)}, {u"l", Value(0)}})); }));
    set(Value(u"pie10"), fallback(get(Value(u"pie10")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", numeric(Value(60))}, {u"l", negate(Value(20))}})); }));
    set(Value(u"pie11"), fallback(get(Value(u"pie11")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", negate(Value(60))}, {u"l", negate(Value(20))}})); }));
    set(Value(u"pie12"), fallback(get(Value(u"pie12")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(120)}, {u"l", negate(Value(10))}})); }));
    set(Value(u"pieTitleTextSize"), fallback(get(Value(u"pieTitleTextSize")), [&] { return Value(u"25px"); }));
    set(Value(u"pieTitleTextColor"), fallback(get(Value(u"pieTitleTextColor")), [&] { return get(Value(u"taskTextDarkColor")); }));
    set(Value(u"pieSectionTextSize"), fallback(get(Value(u"pieSectionTextSize")), [&] { return Value(u"17px"); }));
    set(Value(u"pieSectionTextColor"), fallback(get(Value(u"pieSectionTextColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"pieLegendTextSize"), fallback(get(Value(u"pieLegendTextSize")), [&] { return Value(u"17px"); }));
    set(Value(u"pieLegendTextColor"), fallback(get(Value(u"pieLegendTextColor")), [&] { return get(Value(u"taskTextDarkColor")); }));
    set(Value(u"pieStrokeColor"), fallback(get(Value(u"pieStrokeColor")), [&] { return Value(u"black"); }));
    set(Value(u"pieStrokeWidth"), fallback(get(Value(u"pieStrokeWidth")), [&] { return Value(u"2px"); }));
    set(Value(u"pieOuterStrokeWidth"), fallback(get(Value(u"pieOuterStrokeWidth")), [&] { return Value(u"2px"); }));
    set(Value(u"pieOuterStrokeColor"), fallback(get(Value(u"pieOuterStrokeColor")), [&] { return Value(u"black"); }));
    set(Value(u"pieOpacity"), fallback(get(Value(u"pieOpacity")), [&] { return Value(u"0.7"); }));
    set(Value(u"venn1"), nullish(get(Value(u"venn1")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"l", negate(Value(30))}})); }));
    set(Value(u"venn2"), nullish(get(Value(u"venn2")), [&] { return adjust(get(Value(u"secondaryColor")), object({{u"l", negate(Value(30))}})); }));
    set(Value(u"venn3"), nullish(get(Value(u"venn3")), [&] { return adjust(get(Value(u"tertiaryColor")), object({{u"l", negate(Value(30))}})); }));
    set(Value(u"venn4"), nullish(get(Value(u"venn4")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(60)}, {u"l", negate(Value(30))}})); }));
    set(Value(u"venn5"), nullish(get(Value(u"venn5")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", negate(Value(60))}, {u"l", negate(Value(30))}})); }));
    set(Value(u"venn6"), nullish(get(Value(u"venn6")), [&] { return adjust(get(Value(u"secondaryColor")), object({{u"h", Value(60)}, {u"l", negate(Value(30))}})); }));
    set(Value(u"venn7"), nullish(get(Value(u"venn7")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", Value(120)}, {u"l", negate(Value(30))}})); }));
    set(Value(u"venn8"), nullish(get(Value(u"venn8")), [&] { return adjust(get(Value(u"secondaryColor")), object({{u"h", Value(120)}, {u"l", negate(Value(30))}})); }));
    set(Value(u"vennTitleTextColor"), nullish(get(Value(u"vennTitleTextColor")), [&] { return get(Value(u"titleColor")); }));
    set(Value(u"vennSetTextColor"), nullish(get(Value(u"vennSetTextColor")), [&] { return get(Value(u"textColor")); }));
    set(Value(u"cynefin"), object({{u"domainFontSize", fallback(property(get(Value(u"cynefin")), Value(u"domainFontSize")), [&] { return Value(16); })}, {u"itemFontSize", fallback(property(get(Value(u"cynefin")), Value(u"itemFontSize")), [&] { return Value(12); })}, {u"boundaryColor", fallback(property(get(Value(u"cynefin")), Value(u"boundaryColor")), [&] { return get(Value(u"lineColor")); })}, {u"boundaryWidth", fallback(property(get(Value(u"cynefin")), Value(u"boundaryWidth")), [&] { return Value(2); })}, {u"cliffColor", fallback(property(get(Value(u"cynefin")), Value(u"cliffColor")), [&] { return Value(u"#8B0000"); })}, {u"cliffWidth", fallback(property(get(Value(u"cynefin")), Value(u"cliffWidth")), [&] { return Value(4); })}, {u"arrowColor", fallback(property(get(Value(u"cynefin")), Value(u"arrowColor")), [&] { return get(Value(u"lineColor")); })}, {u"arrowWidth", fallback(property(get(Value(u"cynefin")), Value(u"arrowWidth")), [&] { return Value(2); })}, {u"complexBg", fallback(property(get(Value(u"cynefin")), Value(u"complexBg")), [&] { return Value(u"#E8F5E9"); })}, {u"complicatedBg", fallback(property(get(Value(u"cynefin")), Value(u"complicatedBg")), [&] { return Value(u"#E3F2FD"); })}, {u"chaoticBg", fallback(property(get(Value(u"cynefin")), Value(u"chaoticBg")), [&] { return Value(u"#FBE9E7"); })}, {u"clearBg", fallback(property(get(Value(u"cynefin")), Value(u"clearBg")), [&] { return Value(u"#FFF8E1"); })}, {u"confusionBg", fallback(property(get(Value(u"cynefin")), Value(u"confusionBg")), [&] { return Value(u"#F3E5F5"); })}, {u"textColor", fallback(property(get(Value(u"cynefin")), Value(u"textColor")), [&] { return get(Value(u"textColor")); })}, {u"labelColor", fallback(property(get(Value(u"cynefin")), Value(u"labelColor")), [&] { return get(Value(u"primaryTextColor")); })}}));
    set(Value(u"radar"), object({{u"axisColor", fallback(property(get(Value(u"radar")), Value(u"axisColor")), [&] { return get(Value(u"lineColor")); })}, {u"axisStrokeWidth", fallback(property(get(Value(u"radar")), Value(u"axisStrokeWidth")), [&] { return Value(2); })}, {u"axisLabelFontSize", fallback(property(get(Value(u"radar")), Value(u"axisLabelFontSize")), [&] { return Value(12); })}, {u"curveOpacity", fallback(property(get(Value(u"radar")), Value(u"curveOpacity")), [&] { return Value(0.5); })}, {u"curveStrokeWidth", fallback(property(get(Value(u"radar")), Value(u"curveStrokeWidth")), [&] { return Value(2); })}, {u"graticuleColor", fallback(property(get(Value(u"radar")), Value(u"graticuleColor")), [&] { return Value(u"#DEDEDE"); })}, {u"graticuleStrokeWidth", fallback(property(get(Value(u"radar")), Value(u"graticuleStrokeWidth")), [&] { return Value(1); })}, {u"graticuleOpacity", fallback(property(get(Value(u"radar")), Value(u"graticuleOpacity")), [&] { return Value(0.3); })}, {u"legendBoxSize", fallback(property(get(Value(u"radar")), Value(u"legendBoxSize")), [&] { return Value(12); })}, {u"legendFontSize", fallback(property(get(Value(u"radar")), Value(u"legendFontSize")), [&] { return Value(12); })}}));
    set(Value(u"wardleyEvolutionColor"), fallback(get(Value(u"wardleyEvolutionColor")), [&] { return Value(u"#dc3545"); }));
    set(Value(u"wardley"), object({{u"backgroundColor", fallback(property(get(Value(u"wardley")), Value(u"backgroundColor")), [&] { return get(Value(u"background")); })}, {u"axisColor", fallback(property(get(Value(u"wardley")), Value(u"axisColor")), [&] { return get(Value(u"lineColor")); })}, {u"axisTextColor", fallback(property(get(Value(u"wardley")), Value(u"axisTextColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"gridColor", fallback(property(get(Value(u"wardley")), Value(u"gridColor")), [&] { return get(Value(u"gridColor")); })}, {u"componentFill", fallback(property(get(Value(u"wardley")), Value(u"componentFill")), [&] { return get(Value(u"background")); })}, {u"componentStroke", fallback(property(get(Value(u"wardley")), Value(u"componentStroke")), [&] { return get(Value(u"lineColor")); })}, {u"componentLabelColor", fallback(property(get(Value(u"wardley")), Value(u"componentLabelColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"linkStroke", fallback(property(get(Value(u"wardley")), Value(u"linkStroke")), [&] { return get(Value(u"lineColor")); })}, {u"evolutionStroke", fallback(property(get(Value(u"wardley")), Value(u"evolutionStroke")), [&] { return get(Value(u"wardleyEvolutionColor")); })}, {u"annotationStroke", fallback(property(get(Value(u"wardley")), Value(u"annotationStroke")), [&] { return get(Value(u"lineColor")); })}, {u"annotationTextColor", fallback(property(get(Value(u"wardley")), Value(u"annotationTextColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"annotationFill", fallback(property(get(Value(u"wardley")), Value(u"annotationFill")), [&] { return get(Value(u"background")); })}}));
    set(Value(u"archEdgeColor"), fallback(get(Value(u"archEdgeColor")), [&] { return Value(u"#777"); }));
    set(Value(u"archEdgeArrowColor"), fallback(get(Value(u"archEdgeArrowColor")), [&] { return Value(u"#777"); }));
    set(Value(u"archEdgeWidth"), fallback(get(Value(u"archEdgeWidth")), [&] { return Value(u"3"); }));
    set(Value(u"archGroupBorderColor"), fallback(get(Value(u"archGroupBorderColor")), [&] { return Value(u"#000"); }));
    set(Value(u"archGroupBorderWidth"), fallback(get(Value(u"archGroupBorderWidth")), [&] { return Value(u"2px"); }));
    set(Value(u"quadrant1Fill"), fallback(get(Value(u"quadrant1Fill")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"quadrant2Fill"), fallback(get(Value(u"quadrant2Fill")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"r", Value(5)}, {u"g", Value(5)}, {u"b", Value(5)}})); }));
    set(Value(u"quadrant3Fill"), fallback(get(Value(u"quadrant3Fill")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"r", Value(10)}, {u"g", Value(10)}, {u"b", Value(10)}})); }));
    set(Value(u"quadrant4Fill"), fallback(get(Value(u"quadrant4Fill")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"r", Value(15)}, {u"g", Value(15)}, {u"b", Value(15)}})); }));
    set(Value(u"quadrant1TextFill"), fallback(get(Value(u"quadrant1TextFill")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"quadrant2TextFill"), fallback(get(Value(u"quadrant2TextFill")), [&] { return adjust(get(Value(u"primaryTextColor")), object({{u"r", negate(Value(5))}, {u"g", negate(Value(5))}, {u"b", negate(Value(5))}})); }));
    set(Value(u"quadrant3TextFill"), fallback(get(Value(u"quadrant3TextFill")), [&] { return adjust(get(Value(u"primaryTextColor")), object({{u"r", negate(Value(10))}, {u"g", negate(Value(10))}, {u"b", negate(Value(10))}})); }));
    set(Value(u"quadrant4TextFill"), fallback(get(Value(u"quadrant4TextFill")), [&] { return adjust(get(Value(u"primaryTextColor")), object({{u"r", negate(Value(15))}, {u"g", negate(Value(15))}, {u"b", negate(Value(15))}})); }));
    set(Value(u"quadrantPointFill"), (truthy(fallback(get(Value(u"quadrantPointFill")), [&] { return isDark(get(Value(u"quadrant1Fill"))); })) ? lighten(get(Value(u"quadrant1Fill"))) : darken(get(Value(u"quadrant1Fill")))));
    set(Value(u"quadrantPointTextFill"), fallback(get(Value(u"quadrantPointTextFill")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"quadrantXAxisTextFill"), fallback(get(Value(u"quadrantXAxisTextFill")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"quadrantYAxisTextFill"), fallback(get(Value(u"quadrantYAxisTextFill")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"quadrantInternalBorderStrokeFill"), fallback(get(Value(u"quadrantInternalBorderStrokeFill")), [&] { return get(Value(u"primaryBorderColor")); }));
    set(Value(u"quadrantExternalBorderStrokeFill"), fallback(get(Value(u"quadrantExternalBorderStrokeFill")), [&] { return get(Value(u"primaryBorderColor")); }));
    set(Value(u"quadrantTitleFill"), fallback(get(Value(u"quadrantTitleFill")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"xyChart"), object({{u"backgroundColor", fallback(property(get(Value(u"xyChart")), Value(u"backgroundColor")), [&] { return get(Value(u"background")); })}, {u"titleColor", fallback(property(get(Value(u"xyChart")), Value(u"titleColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"dataLabelColor", fallback(property(get(Value(u"xyChart")), Value(u"dataLabelColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"legendTextColor", fallback(property(get(Value(u"xyChart")), Value(u"legendTextColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"xAxisTitleColor", fallback(property(get(Value(u"xyChart")), Value(u"xAxisTitleColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"xAxisLabelColor", fallback(property(get(Value(u"xyChart")), Value(u"xAxisLabelColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"xAxisTickColor", fallback(property(get(Value(u"xyChart")), Value(u"xAxisTickColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"xAxisLineColor", fallback(property(get(Value(u"xyChart")), Value(u"xAxisLineColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"yAxisTitleColor", fallback(property(get(Value(u"xyChart")), Value(u"yAxisTitleColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"yAxisLabelColor", fallback(property(get(Value(u"xyChart")), Value(u"yAxisLabelColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"yAxisTickColor", fallback(property(get(Value(u"xyChart")), Value(u"yAxisTickColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"yAxisLineColor", fallback(property(get(Value(u"xyChart")), Value(u"yAxisLineColor")), [&] { return get(Value(u"primaryTextColor")); })}, {u"plotColorPalette", fallback(property(get(Value(u"xyChart")), Value(u"plotColorPalette")), [&] { return Value(u"#FFF4DD,#FFD8B1,#FFA07A,#ECEFF1,#D6DBDF,#C3E0A8,#FFB6A4,#FFD74D,#738FA7,#FFFFF0"); })}}));
    set(Value(u"requirementBackground"), fallback(get(Value(u"requirementBackground")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"requirementBorderColor"), fallback(get(Value(u"requirementBorderColor")), [&] { return get(Value(u"primaryBorderColor")); }));
    set(Value(u"requirementBorderSize"), fallback(get(Value(u"requirementBorderSize")), [&] { return Value(u"1"); }));
    set(Value(u"requirementTextColor"), fallback(get(Value(u"requirementTextColor")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"relationColor"), fallback(get(Value(u"relationColor")), [&] { return get(Value(u"lineColor")); }));
    set(Value(u"relationLabelBackground"), fallback(get(Value(u"relationLabelBackground")), [&] { return (truthy(get(Value(u"darkMode"))) ? darken(get(Value(u"secondaryColor")), Value(30)) : get(Value(u"secondaryColor"))); }));
    set(Value(u"relationLabelColor"), fallback(get(Value(u"relationLabelColor")), [&] { return get(Value(u"actorTextColor")); }));
    set(Value(u"git0"), fallback(get(Value(u"git0")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"git1"), fallback(get(Value(u"git1")), [&] { return get(Value(u"secondaryColor")); }));
    set(Value(u"git2"), fallback(get(Value(u"git2")), [&] { return get(Value(u"tertiaryColor")); }));
    set(Value(u"git3"), fallback(get(Value(u"git3")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", negate(Value(30))}})); }));
    set(Value(u"git4"), fallback(get(Value(u"git4")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", negate(Value(60))}})); }));
    set(Value(u"git5"), fallback(get(Value(u"git5")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", negate(Value(90))}})); }));
    set(Value(u"git6"), fallback(get(Value(u"git6")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", numeric(Value(60))}})); }));
    set(Value(u"git7"), fallback(get(Value(u"git7")), [&] { return adjust(get(Value(u"primaryColor")), object({{u"h", numeric(Value(120))}})); }));
    if (truthy(get(Value(u"darkMode")))) {
        set(Value(u"git0"), lighten(get(Value(u"git0")), Value(25)));
        set(Value(u"git1"), lighten(get(Value(u"git1")), Value(25)));
        set(Value(u"git2"), lighten(get(Value(u"git2")), Value(25)));
        set(Value(u"git3"), lighten(get(Value(u"git3")), Value(25)));
        set(Value(u"git4"), lighten(get(Value(u"git4")), Value(25)));
        set(Value(u"git5"), lighten(get(Value(u"git5")), Value(25)));
        set(Value(u"git6"), lighten(get(Value(u"git6")), Value(25)));
        set(Value(u"git7"), lighten(get(Value(u"git7")), Value(25)));
    } else {
        set(Value(u"git0"), darken(get(Value(u"git0")), Value(25)));
        set(Value(u"git1"), darken(get(Value(u"git1")), Value(25)));
        set(Value(u"git2"), darken(get(Value(u"git2")), Value(25)));
        set(Value(u"git3"), darken(get(Value(u"git3")), Value(25)));
        set(Value(u"git4"), darken(get(Value(u"git4")), Value(25)));
        set(Value(u"git5"), darken(get(Value(u"git5")), Value(25)));
        set(Value(u"git6"), darken(get(Value(u"git6")), Value(25)));
        set(Value(u"git7"), darken(get(Value(u"git7")), Value(25)));
    }
    set(Value(u"gitInv0"), fallback(get(Value(u"gitInv0")), [&] { return invert(get(Value(u"git0"))); }));
    set(Value(u"gitInv1"), fallback(get(Value(u"gitInv1")), [&] { return invert(get(Value(u"git1"))); }));
    set(Value(u"gitInv2"), fallback(get(Value(u"gitInv2")), [&] { return invert(get(Value(u"git2"))); }));
    set(Value(u"gitInv3"), fallback(get(Value(u"gitInv3")), [&] { return invert(get(Value(u"git3"))); }));
    set(Value(u"gitInv4"), fallback(get(Value(u"gitInv4")), [&] { return invert(get(Value(u"git4"))); }));
    set(Value(u"gitInv5"), fallback(get(Value(u"gitInv5")), [&] { return invert(get(Value(u"git5"))); }));
    set(Value(u"gitInv6"), fallback(get(Value(u"gitInv6")), [&] { return invert(get(Value(u"git6"))); }));
    set(Value(u"gitInv7"), fallback(get(Value(u"gitInv7")), [&] { return invert(get(Value(u"git7"))); }));
    set(Value(u"branchLabelColor"), fallback(get(Value(u"branchLabelColor")), [&] { return (truthy(get(Value(u"darkMode"))) ? Value(u"black") : get(Value(u"labelTextColor"))); }));
    set(Value(u"gitBranchLabel0"), fallback(get(Value(u"gitBranchLabel0")), [&] { return get(Value(u"branchLabelColor")); }));
    set(Value(u"gitBranchLabel1"), fallback(get(Value(u"gitBranchLabel1")), [&] { return get(Value(u"branchLabelColor")); }));
    set(Value(u"gitBranchLabel2"), fallback(get(Value(u"gitBranchLabel2")), [&] { return get(Value(u"branchLabelColor")); }));
    set(Value(u"gitBranchLabel3"), fallback(get(Value(u"gitBranchLabel3")), [&] { return get(Value(u"branchLabelColor")); }));
    set(Value(u"gitBranchLabel4"), fallback(get(Value(u"gitBranchLabel4")), [&] { return get(Value(u"branchLabelColor")); }));
    set(Value(u"gitBranchLabel5"), fallback(get(Value(u"gitBranchLabel5")), [&] { return get(Value(u"branchLabelColor")); }));
    set(Value(u"gitBranchLabel6"), fallback(get(Value(u"gitBranchLabel6")), [&] { return get(Value(u"branchLabelColor")); }));
    set(Value(u"gitBranchLabel7"), fallback(get(Value(u"gitBranchLabel7")), [&] { return get(Value(u"branchLabelColor")); }));
    set(Value(u"tagLabelColor"), fallback(get(Value(u"tagLabelColor")), [&] { return get(Value(u"primaryTextColor")); }));
    set(Value(u"tagLabelBackground"), fallback(get(Value(u"tagLabelBackground")), [&] { return get(Value(u"primaryColor")); }));
    set(Value(u"tagLabelBorder"), fallback(get(Value(u"tagBorder")), [&] { return get(Value(u"primaryBorderColor")); }));
    set(Value(u"tagLabelFontSize"), fallback(get(Value(u"tagLabelFontSize")), [&] { return Value(u"10px"); }));
    set(Value(u"commitLabelColor"), fallback(get(Value(u"commitLabelColor")), [&] { return get(Value(u"secondaryTextColor")); }));
    set(Value(u"commitLabelBackground"), fallback(get(Value(u"commitLabelBackground")), [&] { return get(Value(u"secondaryColor")); }));
    set(Value(u"commitLabelFontSize"), fallback(get(Value(u"commitLabelFontSize")), [&] { return Value(u"10px"); }));
    set(Value(u"emUiFill"), fallback(get(Value(u"emUiFill")), [&] { return Value(u"white"); }));
    set(Value(u"emUiStroke"), fallback(get(Value(u"emUiStroke")), [&] { return Value(u"#dbdada"); }));
    set(Value(u"emProcessorFill"), fallback(get(Value(u"emProcessorFill")), [&] { return Value(u"#edb3f6"); }));
    set(Value(u"emProcessorStroke"), fallback(get(Value(u"emProcessorStroke")), [&] { return Value(u"#b88cbf"); }));
    set(Value(u"emReadModelFill"), fallback(get(Value(u"emReadModelFill")), [&] { return Value(u"#d3f1a2"); }));
    set(Value(u"emReadModelStroke"), fallback(get(Value(u"emReadModelStroke")), [&] { return Value(u"#a3b732"); }));
    set(Value(u"emCommandFill"), fallback(get(Value(u"emCommandFill")), [&] { return Value(u"#bcd6fe"); }));
    set(Value(u"emCommandStroke"), fallback(get(Value(u"emCommandStroke")), [&] { return Value(u"#679ac3"); }));
    set(Value(u"emEventFill"), fallback(get(Value(u"emEventFill")), [&] { return Value(u"#ffb778"); }));
    set(Value(u"emEventStroke"), fallback(get(Value(u"emEventStroke")), [&] { return Value(u"#c19a0f"); }));
    set(Value(u"emSwimlaneBackgroundOdd"), fallback(get(Value(u"emSwimlaneBackgroundOdd")), [&] { return Value(u"rgb(250,250,250)"); }));
    set(Value(u"emSwimlaneBackgroundStroke"), fallback(get(Value(u"emSwimlaneBackgroundStroke")), [&] { return Value(u"rgb(240,240,240)"); }));
    set(Value(u"emArrowhead"), fallback(get(Value(u"emArrowhead")), [&] { return get(Value(u"lineColor")); }));
    set(Value(u"emRelationStroke"), fallback(get(Value(u"emRelationStroke")), [&] { return get(Value(u"lineColor")); }));
    set(Value(u"attributeBackgroundColorOdd"), fallback(get(Value(u"attributeBackgroundColorOdd")), [&] { return Value(u"#ffffff"); }));
    set(Value(u"attributeBackgroundColorEven"), fallback(get(Value(u"attributeBackgroundColorEven")), [&] { return Value(u"#f2f2f2"); }));
    set(Value(u"gradientStart"), get(Value(u"primaryBorderColor")));
    set(Value(u"gradientStop"), get(Value(u"secondaryBorderColor")));
}
