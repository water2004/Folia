// MathJax 2.7.9 mediawiki-texvc aliases adapted to the typed v4 package API.
// Copyright (c) 2015-2020 The MathJax Consortium. Apache-2.0.
export module folia.math.input.tex.mediawiki_texvc.configuration;
export import folia.math.input.tex.parser;
import std;

export namespace folia::math::tex {
struct MediawikiTexvcConfiguration {
    static ConfigurationRef initializeModule(ConfigurationHandler &, MapHandler &);
};
} // namespace folia::math::tex
