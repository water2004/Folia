import std;
import folia.platform.mermaid_measurer;

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    std::ifstream input(argv[1]);
    std::ofstream output(argv[2]);
    if (!input || !output) return 2;
    folia::platform::MermaidMeasurer measurer;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string family, size, hex, browser, browser_advance;
        if (!std::getline(row, family, '\t') || !std::getline(row, size, '\t') ||
            !std::getline(row, hex, '\t') || !std::getline(row, browser, '\t') ||
            !std::getline(row, browser_advance)) return 2;
        std::u16string text;
        if (hex != "-") for (std::size_t i = 0; i < hex.size(); i += 4)
            text += static_cast<char16_t>(std::stoul(hex.substr(i, 4), nullptr, 16));
        auto result = measurer.measure(text, {{family.begin(), family.end()}, std::stof(size)});
        if (!result) { std::cerr << result.error() << '\n'; return 1; }
        output << line << '\t' << std::setprecision(17) << result->advance << '\t'
            << result->ink_width << '\t' << result->bounds_width << '\n';
        std::cout << family << '\t' << size << '\t' << hex << '\t' << browser << '\t'
            << std::setprecision(17) << result->advance << '\t' << result->bounds_width << '\n';
    }
    if (measurer.measure(u"text", {u"Folia deliberately missing font 792743", 17}) ||
        measurer.measure(std::u16string(1, 0xd800), {u"Arial", 17}) ||
        measurer.measure(u"text", {u"Arial", 0})) return 1;
    return output.good() ? 0 : 2;
}
