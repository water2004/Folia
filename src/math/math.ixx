// Application-facing TeX renderer contract composed by the original math owner.
export module folia.math;
export import folia.math.output.scene;
import std;

export namespace folia::math {
enum class SvgMode { math_container, standalone_display };

struct RenderOptions {
    bool display = false;
    double em = 16;
    double containerWidth = 1280;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    // A standalone display owns its pixel viewport and does not participate
    // in line breaking by a surrounding text document.
    SvgMode svgMode = SvgMode::math_container;
    std::string svgIdPrefix;
};

struct RenderResult {
    std::string svg;
    output::scene::Scene scene;
};
struct RenderInput {
    std::u16string_view tex;
    std::stop_token stop;
    RenderOptions options;
    std::chrono::milliseconds timeout{};
};
struct RenderOutcome {
    std::optional<RenderResult> result;
    std::exception_ptr error;
};

class RenderError : public std::runtime_error {
  public:
    std::string name;
    RenderError(std::string kind, std::string message)
        : std::runtime_error(std::move(message)), name(std::move(kind)) {}
};

// One renderer owns one configured TeX session. Calls must use its creating
// thread. Results own their strings and immutable Scene values independently.
class Renderer final {
    struct State;
    std::unique_ptr<State> state_;

  public:
    Renderer();
    ~Renderer();
    Renderer(Renderer const &) = delete;
    Renderer &operator=(Renderer const &) = delete;
    std::vector<RenderOutcome> renderBatch(std::span<RenderInput const> requests);
    RenderResult render(std::u16string_view, RenderOptions = {});
    RenderResult render(std::string_view, RenderOptions = {});
    RenderResult render(std::u16string_view, std::stop_token, RenderOptions);
    RenderResult render(std::string_view, std::stop_token, RenderOptions);
    output::scene::Scene renderScene(std::u16string_view, RenderOptions = {});
    output::scene::Scene renderScene(std::string_view, RenderOptions = {});
    output::scene::Scene renderScene(std::u16string_view, std::stop_token, RenderOptions);
    output::scene::Scene renderScene(std::string_view, std::stop_token, RenderOptions);
    std::string toSvg(std::u16string_view, RenderOptions = {});
    std::string toSvg(std::string_view, RenderOptions = {});
    std::string toSvg(std::u16string_view, std::stop_token, RenderOptions);
    std::string toSvg(std::string_view, std::stop_token, RenderOptions);
};
} // namespace folia::math
