#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

// Nested observations are data records, never generated C++ initializers. The
// field path and type make deletions, reordering and schema drift fail loudly.
// Retained strings/arrays keep the original test-facing string_view/span API.
class ObservationReader {
    std::string file_;
    std::vector<std::vector<std::string>> rows_;
    std::size_t next_{};
    std::deque<std::string> bytes_;
    std::deque<std::u16string> units_;
    std::vector<std::shared_ptr<void>> arrays_;

    std::string_view take(const std::string& path, std::string_view type) {
        if (next_ == rows_.size() || rows_[next_][0] != path || rows_[next_][1] != type)
            throw std::runtime_error(file_ + ": missing or reordered " + path +
                                     " (" + std::string{type} + ")");
        return rows_[next_++][2];
    }

  public:
    explicit ObservationReader(std::string_view file)
        : file_{file}, rows_{folia::test::data::read_tsv(file, 3)} {}

    ObservationReader(const ObservationReader&) = delete;
    ObservationReader& operator=(const ObservationReader&) = delete;
    ObservationReader(ObservationReader&&) = delete;
    ObservationReader& operator=(ObservationReader&&) = delete;

    void finish() const {
        if (next_ != rows_.size())
            throw std::runtime_error(file_ + ": unconsumed observation fields");
    }

    void field(const std::string& path, std::string_view& value) {
        bytes_.push_back(bytes(take(path, "bytes")));
        value = bytes_.back();
    }
    void field(const std::string& path, std::u16string_view& value) {
        units_.push_back(folia::test::data::decode_utf16(take(path, "utf16")));
        value = units_.back();
    }
    void field(const std::string& path, bool& value) {
        value = boolean(take(path, "boolean"));
    }
    void field(const std::string& path, double& value) {
        value = std::bit_cast<double>(binary64(take(path, "binary64")));
    }
    template <std::integral T> requires (!std::same_as<T, bool>)
    void field(const std::string& path, T& value) {
        const auto encoded = take(path, "integer");
        value = encoded == "max" ? std::numeric_limits<T>::max() : integer<T>(encoded);
    }
    template <class T> requires std::is_enum_v<T>
    void field(const std::string& path, T& value) {
        const auto number = integer<std::underlying_type_t<T>>(take(path, "enum"));
        if (number < 0 || number > observationEnumMaximum(T{}))
            throw std::runtime_error(file_ + ": invalid enum at " + path);
        value = static_cast<T>(number);
    }
    template <class T>
    void field(const std::string& path, std::optional<T>& value) {
        bool present{};
        field(path + ".present", present);
        if (present) {
            value.emplace();
            field(path + ".value", *value);
        } else value.reset();
    }
    template <class T, std::size_t N>
    void field(const std::string& path, std::array<T, N>& value) {
        std::size_t count{};
        field(path + ".count", count);
        if (count != N) throw std::runtime_error(file_ + ": fixed array size changed at " + path);
        for (std::size_t i{}; i < N; ++i) field(path + '[' + std::to_string(i) + ']', value[i]);
    }
    template <class T, std::size_t N>
    void field(const std::string& path, T (&value)[N]) {
        std::size_t count{};
        field(path + ".count", count);
        if (count != N) throw std::runtime_error(file_ + ": fixed array size changed at " + path);
        for (std::size_t i{}; i < N; ++i) field(path + '[' + std::to_string(i) + ']', value[i]);
    }
    template <class T>
    void field(const std::string& path, std::span<const T>& value) {
        std::size_t count{};
        field(path + ".count", count);
        if (count > rows_.size() - next_)
            throw std::runtime_error(file_ + ": impossible array size at " + path);
        auto retained = std::make_shared<std::vector<T>>(count);
        for (std::size_t i{}; i < count; ++i)
            field(path + '[' + std::to_string(i) + ']', retained->at(i));
        value = *retained;
        arrays_.push_back(std::move(retained));
    }
    template <class T> requires (!std::is_arithmetic_v<T> && !std::is_enum_v<T>)
    void field(const std::string& path, T& value) {
        observationFields(*this, path, value);
    }
};

} // namespace math_test::fixtures
