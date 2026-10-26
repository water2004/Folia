// Folia's production package composition, shared with registry conformance tests.
export module folia.math.input.tex.default_packages;
export import folia.math.input.tex.parser;
export import folia.math.handlers.html.html_dom_strings;
import std;

export namespace folia::math::tex {
struct DefaultPackages {
    static std::vector<PackageSelection> selections();
    static void initialize(ConfigurationHandler &, MapHandler &,
                           std::shared_ptr<handlers::html::HTMLDomStringsOptions>);
};
} // namespace folia::math::tex
