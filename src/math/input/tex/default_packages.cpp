// Folia's eagerly registered TeX package profile. Package implementations stay independent.
module folia.math.input.tex.default_packages;
import folia.math.input.tex.action.configuration;
import folia.math.input.tex.ams.configuration;
import folia.math.input.tex.amscd.configuration;
import folia.math.input.tex.base_configuration;
import folia.math.input.tex.bbm.configuration;
import folia.math.input.tex.bboldx.configuration;
import folia.math.input.tex.bbox.configuration;
import folia.math.input.tex.begingroup.configuration;
import folia.math.input.tex.boldsymbol.configuration;
import folia.math.input.tex.braket.configuration;
import folia.math.input.tex.bussproofs.configuration;
import folia.math.input.tex.cancel.configuration;
import folia.math.input.tex.cases.configuration;
import folia.math.input.tex.centernot.configuration;
import folia.math.input.tex.color.configuration;
import folia.math.input.tex.colortbl.configuration;
import folia.math.input.tex.configmacros.configuration;
import folia.math.input.tex.dsfont.configuration;
import folia.math.input.tex.empheq.configuration;
import folia.math.input.tex.enclose.configuration;
import folia.math.input.tex.extpfeil.configuration;
import folia.math.input.tex.fontsizev3.configuration;
import folia.math.input.tex.gensymb.configuration;
import folia.math.input.tex.html.configuration;
import folia.math.input.tex.mathtools.configuration;
import folia.math.input.tex.mhchem.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.noundefined.configuration;
import folia.math.input.tex.physics.configuration;
import folia.math.input.tex.setoptions.set_options_configuration;
import folia.math.input.tex.tagformat.tag_format_configuration;
import folia.math.input.tex.texhtml.tex_html_configuration;
import folia.math.input.tex.textcomp.configuration;
import folia.math.input.tex.textmacros.configuration;
import folia.math.input.tex.unicode.configuration;
import folia.math.input.tex.units.configuration;
import folia.math.input.tex.upgreek.configuration;
import folia.math.input.tex.verb.configuration;
import folia.math.input.tex.mediawiki_texvc.configuration;
import std;

namespace folia::math::tex {
std::vector<PackageSelection> DefaultPackages::selections() {
    return {{u"base", {}},         {u"action", {}},     {u"ams", {}},        {u"amscd", {}},
        {u"bbm", {}},          {u"bboldx", {}},     {u"bbox", {}},       {u"begingroup", {}},
        {u"boldsymbol", {}},   {u"braket", {}},     {u"bussproofs", {}}, {u"cancel", {}},
        {u"cases", {}},        {u"centernot", {}},  {u"color", {}},      {u"colortbl", {}},
        {u"configmacros", {}}, {u"dsfont", {}},     {u"empheq", {}},     {u"enclose", {}},
        {u"extpfeil", {}},     {u"fontsizev3", {}}, {u"gensymb", {}},    {u"html", {}},
        {u"mathtools", {}},    {u"mhchem", {}},     {u"newcommand", {}}, {u"noundefined", {}},
        {u"physics", {}},      {u"setoptions", {}}, {u"tagformat", {}},  {u"texhtml", {}},
        {u"textcomp", {}},     {u"textmacros", {}}, {u"unicode", {}},    {u"units", {}},
        {u"upgreek", {}},      {u"verb", {}},       {u"mediawiki-texvc", {}}};
}

void DefaultPackages::initialize(ConfigurationHandler &configurations, MapHandler &maps,
                            std::shared_ptr<handlers::html::HTMLDomStringsOptions> htmlOptions) {
    BasePackage::initialize(configurations, maps);
    ActionConfiguration::initializeModule(configurations, maps);
    AmsConfiguration::initializeModule(configurations, maps);
    AmsCdConfiguration::initializeModule(configurations, maps);
    BbmConfiguration::initializeModule(configurations, maps);
    BboldxConfiguration::initializeModule(configurations, maps);
    BboxConfiguration::initializeModule(configurations, maps);
    BegingroupConfiguration::initializeModule(configurations, maps);
    BoldsymbolConfiguration::initializeModule(configurations, maps);
    BraketConfiguration::initializeModule(configurations, maps);
    BussproofsConfiguration::initializeModule(configurations, maps);
    CancelConfiguration::initializeModule(configurations, maps);
    CasesConfiguration::initializeModule(configurations, maps);
    CenternotConfiguration::initializeModule(configurations, maps);
    ColorConfiguration::initializeModule(configurations, maps);
    ColortblConfiguration::initializeModule(configurations, maps);
    ConfigMacrosConfiguration::initializeModule(configurations, maps);
    DsfontConfiguration::initializeModule(configurations, maps);
    EmpheqConfiguration::initializeModule(configurations, maps);
    EncloseConfiguration::initializeModule(configurations, maps);
    ExtpfeilConfiguration::initializeModule(configurations, maps);
    FontSizeV3Configuration::initializeModule(configurations, maps);
    GensymbConfiguration::initializeModule(configurations, maps);
    HtmlConfiguration::initializeModule(configurations, maps);
    MathtoolsConfiguration::initializeModule(configurations, maps);
    MhchemConfiguration::initializeModule(configurations, maps);
    NewcommandConfiguration::initializeModule(configurations, maps);
    NoUndefinedConfiguration::initializeModule(configurations);
    PhysicsConfiguration::initializeModule(configurations, maps);
    SetOptionsConfiguration::initializeModule(configurations);
    TagFormatConfiguration::initializeModule(configurations);
    TexHtmlConfiguration::initializeModule(configurations, maps, htmlOptions);
    TextcompConfiguration::initializeModule(configurations, maps);
    TextMacrosConfiguration::initializeModule(configurations, maps);
    UnicodeConfiguration::initializeModule(configurations, maps);
    UnitsConfiguration::initializeModule(configurations, maps);
    UpgreekConfiguration::initializeModule(configurations, maps);
    VerbConfiguration::initializeModule(configurations, maps);

    MediawikiTexvcConfiguration::initializeModule(configurations, maps);
}
} // namespace folia::math::tex
