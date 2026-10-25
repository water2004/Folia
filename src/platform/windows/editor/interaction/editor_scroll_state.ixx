// folia.platform.editor_scroll_state — deterministic editor scroll state.
export module folia.platform.editor_scroll_state;
import std;

export namespace folia::platform::editor
{
    class EditorScrollState
    {
    public:
        struct Snapshot
        {
            double offset = 0.0;
            double target = 0.0;
        };

        float Offset() const noexcept { return static_cast<float>(offset_); }
        float Target() const noexcept { return static_cast<float>(target_); }
        bool Animating() const noexcept { return std::fabs(target_ - offset_) >= 0.1f; }
        Snapshot Save() const noexcept { return {offset_, target_}; }

        void Set(float value, float maximum)
        {
            offset_ = ClampValue(value, maximum);
            target_ = offset_;
        }

        void Restore(Snapshot value, float maximum)
        {
            offset_ = ClampValue(value.offset, maximum);
            target_ = ClampValue(value.target, maximum);
        }

        void Clamp(float maximum)
        {
            offset_ = ClampValue(offset_, maximum);
            target_ = ClampValue(target_, maximum);
        }

        void Shift(float delta, float maximum)
        {
            if (!std::isfinite(delta) || delta == 0.0f) return;
            offset_ = ClampValue(offset_ + delta, maximum);
            target_ = ClampValue(target_ + delta, maximum);
        }

        void Queue(float delta, float maximum)
        {
            if (!std::isfinite(delta) || delta == 0.0f) return;
            auto pendingDistance = target_ - offset_;
            if (pendingDistance * delta < 0.0f) target_ = offset_;
            target_ = ClampValue(target_ + delta, maximum);
        }

        bool Advance(float elapsedSeconds)
        {
            auto distance = target_ - offset_;
            auto elapsed = (std::max)(0.0, static_cast<double>(elapsedSeconds));
            if (std::fabs(distance) < 0.1f)
            {
                offset_ = target_;
                return false;
            }

            // Actual elapsed time and an analytic exponential response give
            // one monotonic deceleration phase independent of frame rate.
            constexpr double responseHalfLifeSeconds = 0.075;
            auto response = 1.0 - std::exp2(-elapsed / responseHalfLifeSeconds);
            offset_ += distance * response;
            return true;
        }

    private:
        static double ClampValue(double value, double maximum)
        {
            if (!std::isfinite(value)) return 0.0f;
            maximum = std::isfinite(maximum) ? (std::max)(0.0, maximum) : 0.0;
            return (std::clamp)(value, 0.0, maximum);
        }

        // Keep subpixel progress until it reaches the float rendering edge.
        // At large document offsets a float increment can round to zero while
        // the remaining distance still says the animation must continue.
        double offset_ = 0.0;
        double target_ = 0.0;
    };
}
