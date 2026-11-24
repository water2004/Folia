#include "support/folia_test.hpp"
import folia.math.input.tex.color.util;
using namespace boost::ut;
using folia::math::tex::ColorModel;
using folia::math::tex::ColorConstants;

namespace {
template<class F>
std::pair<std::string,std::u16string> errorOf(F&& action) {
    try { action(); }
    catch (folia::math::tex::TexError const& error) {
        return {std::string(error.id()),std::u16string(error.message())};
    }
    throw std::logic_error("expected TexError");
}

std::u16string ascii(std::string_view value) {
    std::u16string result;
    for (char c:value) result.push_back(static_cast<char16_t>(c));
    return result;
}
}

suite native_tex_color = [] {
    "tex_color_source_model_cases"_test = [] {
        ColorModel colors;
        std::u16string huge(400,u'9');
        expect(colors.getColor(u"",u"Red") == u"#ED1B23");
        expect(colors.getColor(u"named",u"not-a-known-color") == u"not-a-known-color");
        expect(colors.getColor(u"rgb",u"0.5,0,1") == u"#7f00ff");
        expect(colors.getColor(u"rgb",u"\uFEFF0.5\u3000,\u00A00.25,1\u202F") == u"#7f3fff");
        expect(colors.getColor(u"RGB",u"128,0,255") == u"#8000ff");
        expect(colors.getColor(u"RGB",u"0001,002,03") == u"#010203");
        expect(colors.getColor(u"gray",u" 0.5\u00A0") == u"#7f7f7f");
        expect(colors.getColor(u"gray",u"\uFEFF.25\u3000") == u"#3f3f3f");
        std::u16string tiny=u"0.";
        tiny.append(400,u'0');
        tiny.push_back(u'1');
        expect(colors.getColor(u"gray",tiny) == u"#000000");

        expect(errorOf([&] { (void)colors.getColor(u"rgb",u"0,1"); }) ==
               std::pair<std::string,std::u16string>{"ModelArg1",u"Color values for the rgb model require 3 numbers"});
        expect(errorOf([&] { (void)colors.getColor(u"rgb",u"1e-1,0,0"); }) ==
               std::pair<std::string,std::u16string>{"InvalidDecimalNumber",u"Invalid decimal number"});
        expect(errorOf([&] { (void)colors.getColor(u"rgb",u"1.001,0,0"); }) ==
               std::pair<std::string,std::u16string>{"ModelArg2",u"Color values for the rgb model must be between 0 and 1"});
        expect(errorOf([&] { (void)colors.getColor(u"rgb",u"999999999999999999999999999999999999,0,0"); }) ==
               std::pair<std::string,std::u16string>{"ModelArg2",u"Color values for the rgb model must be between 0 and 1"});
        expect(errorOf([&] { (void)colors.getColor(u"rgb",huge+u",0,0"); }) ==
               std::pair<std::string,std::u16string>{"ModelArg2",u"Color values for the rgb model must be between 0 and 1"});
        expect(errorOf([&] { (void)colors.getColor(u"RGB",u"256,0,0"); }) ==
               std::pair<std::string,std::u16string>{"ModelArg2",u"Color values for the RGB model must be between 0 and 255"});
        expect(errorOf([&] { (void)colors.getColor(u"RGB",u"999999999999999999999999999999999999,0,0"); }) ==
               std::pair<std::string,std::u16string>{"ModelArg2",u"Color values for the RGB model must be between 0 and 255"});
        expect(errorOf([&] { (void)colors.getColor(u"RGB",huge+u",0,0"); }) ==
               std::pair<std::string,std::u16string>{"ModelArg2",u"Color values for the RGB model must be between 0 and 255"});
        expect(errorOf([&] { (void)colors.getColor(u"gray",u"."); }) ==
               std::pair<std::string,std::u16string>{"InvalidDecimalNumber",u"Invalid decimal number"});
        expect(errorOf([&] { (void)colors.getColor(u"CMYK",u"0,0,0,1"); }) ==
               std::pair<std::string,std::u16string>{"UndefinedColorModel",u"Color model 'CMYK' not defined"});
        expect(errorOf([&] { (void)colors.getColor(u"named",u"bad;color"); }) ==
               std::pair<std::string,std::u16string>{"BadColorValue",u"Invalid color value"});
    };

    "tex_color_user_colors_are_owned_and_override_constants"_test = [] {
        ColorModel first;
        ColorModel second;
        first.defineColor(u"rgb",u"Custom",u"0,0.5,1");
        expect(first.getColor(u"named",u"Custom") == u"#007fff");
        expect(second.getColor(u"named",u"Custom") == u"Custom");

        first.defineColor(u"named",u"Red",u"rebeccapurple");
        expect(first.getColor(u"",u"Red") == u"rebeccapurple");
        expect(second.getColor(u"",u"Red") == u"#ED1B23");
        first.defineColor(u"RGB",u"Custom",u"1,2,3");
        expect(first.getColor(u"named",u"Custom") == u"#010203");

        first.defineColor(u"RGB",u"kept-on-failure",u"4,5,6");
        try { first.defineColor(u"rgb",u"kept-on-failure",u"0,2,0"); }
        catch (folia::math::tex::TexError const&) {}
        expect(first.getColor(u"named",u"kept-on-failure") == u"#040506");
        first.defineColor(u"named",u"semi;name",u"#abc");
        expect(first.getColor(u"named",u"semi;name") == u"#abc");

        const auto copy=first;
        first.defineColor(u"named",u"Custom",u"copy-isolation");
        expect(copy.getColor(u"named",u"Custom") == u"#010203");
        expect(first.getColor(u"named",u"Custom") == u"copy-isolation");
    };

    "tex_color_constants_find_is_exact_for_all_source_entries"_test = [] {
        expect(folia::math::tex::ColorConstantValues.size() == 68u);
        for (auto const& [name,value] : folia::math::tex::ColorConstantValues) {
            expect(ColorConstants::find(ascii(name)) == value);
        }
        expect(!ColorConstants::find(u"red"));
        expect(!ColorConstants::find(u"Blue "));
    };
};
