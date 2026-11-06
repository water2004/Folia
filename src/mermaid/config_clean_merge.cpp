module folia.mermaid.config_clean_merge;
import std;
import folia.mermaid.config_support;

namespace folia::mermaid {
namespace {
// Private JS property state can represent undefined, holes and custom array
// properties during merging. Public ConfigValue remains an owned dense tree.
struct Value {
    ConfigValue scalar;
    bool undefined = true;
    std::map<std::u16string, Value> properties;
    std::size_t length = 0;
    bool array() const { return !undefined && scalar.kind == ConfigKind::sequence; }
    bool object() const { return !undefined && (scalar.kind == ConfigKind::mapping || array()); }
    bool nullish() const { return undefined || scalar.kind == ConfigKind::null; }
};
std::u16string index_key(std::size_t index) {
    const auto ascii = std::to_string(index);
    return {ascii.begin(), ascii.end()};
}
std::optional<std::size_t> array_index(std::u16string_view key) {
    if (key.empty() || (key.size() > 1 && key.front() == u'0') || key.size() > 10) return {};
    std::uint64_t value = 0;
    for (auto c : key) {
        if (c < u'0' || c > u'9') return {};
        value = value * 10 + c - u'0';
    }
    if (value >= 4294967295ULL) return {};
    return static_cast<std::size_t>(value);
}
bool inherited_function(std::u16string_view key, bool array) {
    for (const auto name : {u"constructor", u"toString", u"toLocaleString", u"valueOf",
         u"hasOwnProperty", u"isPrototypeOf", u"propertyIsEnumerable", u"__defineGetter__",
         u"__defineSetter__", u"__lookupGetter__", u"__lookupSetter__"}) if (key == name) return true;
    if (array) for (const auto name : {u"at", u"concat", u"copyWithin", u"entries", u"every", u"fill",
         u"filter", u"find", u"findIndex", u"findLast", u"findLastIndex", u"flat", u"flatMap", u"forEach",
         u"includes", u"indexOf", u"join", u"keys", u"lastIndexOf", u"map", u"pop", u"push", u"reduce",
         u"reduceRight", u"reverse", u"shift", u"slice", u"some", u"sort", u"splice", u"toReversed",
         u"toSorted", u"toSpliced", u"unshift", u"values", u"with"}) if (key == name) return true;
    return false;
}
struct Merge {
    config_detail::Budget budget;
    std::size_t nodes = 0, units = 0;
    void allocate(std::size_t text = 0) {
        budget.step();
        if (nodes >= budget.limits.values) throw std::runtime_error("Clean merge intermediate value budget exceeded");
        ++nodes;
        if (text > budget.limits.string_units - units) throw std::runtime_error("Clean merge intermediate string budget exceeded");
        units += text;
    }
    Value copy(const Value& source) {
        allocate(source.scalar.string.size());
        Value result; result.scalar = source.scalar; result.undefined = source.undefined; result.length = source.length;
        for (const auto& [key, child] : source.properties) {
            allocate(key.size()); result.properties.emplace(key, copy(child));
        }
        return result;
    }
    Value from(const ConfigValue& source) {
        allocate(source.string.size());
        Value result; result.undefined = false; result.scalar.kind = source.kind;
        result.scalar.boolean = source.boolean; result.scalar.number = source.number; result.scalar.string = source.string;
        if (source.kind == ConfigKind::sequence) {
            result.length = source.sequence.size();
            for (std::size_t i = 0; i < source.sequence.size(); ++i) {
                const auto key = index_key(i); allocate(key.size());
                result.properties.emplace(key, from(source.sequence[i]));
            }
        } else for (const auto& [key, child] : source.mapping) {
            allocate(key.size()); result.properties.emplace(key, from(child));
        }
        return result;
    }
    Value container(bool array) {
        allocate(); Value result; result.undefined = false;
        result.scalar.kind = array ? ConfigKind::sequence : ConfigKind::mapping; return result;
    }
    void bounded_length(std::size_t length) {
        if (length > budget.limits.values) throw std::runtime_error("Clean merge array length budget exceeded");
    }
    Value property(const Value& target, const std::u16string& key) {
        if (target.array() && key == u"length") {
            allocate();
            Value value; value.undefined = false; value.scalar.kind = ConfigKind::number;
            value.scalar.number = static_cast<double>(target.length); return value;
        }
        if (const auto found = target.properties.find(key); found != target.properties.end()) return copy(found->second);
        if (inherited_function(key, target.array())) {
            // A function is non-null and non-object-like for merge's branches.
            // It is never emitted: an undefined source leaves it inherited.
            allocate(); Value value; value.undefined = false; value.scalar.kind = ConfigKind::boolean; value.scalar.boolean = true; return value;
        }
        allocate(); return {};
    }
    std::u16string primitive_text(const Value& value) {
        budget.step();
        if (value.properties.contains(u"toString")) throw std::runtime_error("Non-callable array length toString");
        std::u16string result;
        if (value.array()) {
            for (std::size_t i = 0; i < value.length; ++i) {
                budget.step();
                if (i) result += u',';
                const auto found = value.properties.find(index_key(i));
                if (found != value.properties.end() && !found->second.nullish()) result += primitive_text(found->second);
            }
        } else {
            const auto text = config_to_string(value.scalar);
            if (!text) throw std::runtime_error(text.error());
            result = *text;
        }
        budget.text(result.size()); return result;
    }
    double length_number(const Value& value) {
        if (value.undefined) return std::numeric_limits<double>::quiet_NaN();
        if (value.scalar.kind == ConfigKind::number) return value.scalar.number;
        if (value.scalar.kind == ConfigKind::null) return 0;
        if (value.scalar.kind == ConfigKind::boolean) return value.scalar.boolean ? 1 : 0;
        const auto text = primitive_text(value);
        auto raw = std::u16string_view{text};
        const auto whitespace = [](char16_t c) {
            return (c >= 9 && c <= 13) || c == 0x20 || c == 0xa0 || c == 0x1680 ||
                (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
        };
        while (!raw.empty() && whitespace(raw.front())) raw.remove_prefix(1);
        while (!raw.empty() && whitespace(raw.back())) raw.remove_suffix(1);
        if (raw.empty()) return 0;
        const auto invalid = std::numeric_limits<double>::quiet_NaN();
        if (raw.size() > 2 && raw[0] == u'0' && (raw[1] == u'x' || raw[1] == u'X' || raw[1] == u'o' || raw[1] == u'O' || raw[1] == u'b' || raw[1] == u'B')) {
            const unsigned base = raw[1] == u'x' || raw[1] == u'X' ? 16 : raw[1] == u'o' || raw[1] == u'O' ? 8 : 2;
            double number = 0;
            for (auto c : raw.substr(2)) {
                budget.step();
                const unsigned digit = c >= u'0' && c <= u'9' ? c - u'0' : c >= u'a' && c <= u'f' ? c - u'a' + 10 : c >= u'A' && c <= u'F' ? c - u'A' + 10 : 99;
                if (digit >= base) return invalid;
                number = number * base + digit;
            }
            return number;
        }
        if (raw.front() == u'+') {
            raw.remove_prefix(1);
            if (!raw.empty() && (raw.front() == u'+' || raw.front() == u'-')) return invalid;
        }
        if (raw.empty() || std::ranges::any_of(raw, [](auto c) { return c > 127; })) return invalid;
        const std::string ascii(raw.begin(), raw.end());
        double number{};
        const auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), number);
        if (parsed.ptr != ascii.data() + ascii.size()) return invalid;
        if (parsed.ec == std::errc{}) return number;
        if (parsed.ec != std::errc::result_out_of_range) return invalid;
        // Decimal underflow is zero. Saturate exponent arithmetic beyond the
        // input's maximum mantissa displacement; overflow cannot be a length.
        const auto e = ascii.find_first_of("eE");
        auto mantissa = std::string_view{ascii}.substr(0, e);
        if (mantissa.starts_with('-')) mantissa.remove_prefix(1);
        const auto dot = mantissa.find('.');
        const auto integral = dot == mantissa.npos ? mantissa.size() : dot;
        std::size_t zeros = 0; bool nonzero = false;
        for (auto c : mantissa) {
            if (c == '.') continue;
            if (c != '0') { nonzero = true; break; }
            ++zeros;
        }
        if (!nonzero) return 0;
        long long exponent = 0;
        if (e != ascii.npos) {
            auto i = e + 1; const bool minus = ascii[i] == '-';
            if (minus || ascii[i] == '+') ++i;
            const auto cap = static_cast<long long>(ascii.size()) + 1000;
            for (; i < ascii.size(); ++i) exponent = std::min(cap, exponent * 10 + ascii[i] - '0');
            if (minus) exponent = -exponent;
        }
        return exponent + static_cast<long long>(integral) - static_cast<long long>(zeros) - 1 < 0 ? 0 : invalid;
    }
    void put(Value& target, const std::u16string& key, Value value) {
        budget.step();
        if (target.array()) {
            if (key == u"length") {
                const auto number = length_number(value);
                if (!std::isfinite(number) || number < 0 || number > 4294967295.0 || std::floor(number) != number)
                    throw std::runtime_error("Invalid array length assignment");
                const auto length = static_cast<std::size_t>(number); bounded_length(length);
                std::erase_if(target.properties, [&](const auto& entry) {
                    budget.step(); const auto index = array_index(entry.first); return index && *index >= length;
                });
                target.length = length; return;
            }
            if (const auto index = array_index(key)) {
                bounded_length(*index + 1); target.length = std::max(target.length, *index + 1);
            }
        }
        allocate(key.size()); target.properties.insert_or_assign(key, std::move(value));
    }
    void into(Value& target, const Value& source, std::size_t depth = 0) {
        budget.step();
        if (depth > std::min(budget.limits.depth, std::size_t{64})) throw std::runtime_error("Clean merge depth budget exceeded");
        if (!source.object()) return;
        // Integer keys precede other properties in JS. Only array length writes
        // are order-sensitive in this plain-data domain; they follow all indices.
        std::vector<const std::pair<const std::u16string, Value>*> entries;
        for (const auto& entry : source.properties) entries.push_back(&entry);
        std::stable_sort(entries.begin(), entries.end(), [&](const auto* a, const auto* b) {
            budget.step();
            const auto ai = array_index(a->first), bi = array_index(b->first);
            if (ai && bi) return *ai < *bi;
            return ai.has_value() && !bi.has_value();
        });
        for (const auto* entry : entries) {
            budget.step(); const auto& [key, original] = *entry;
            if (key == u"__proto__") continue;
            Value undefined;
            const auto& item = source.array() && original.nullish() ? undefined : original;
            auto value = property(target, key);
            if (item.array()) {
                if (!value.array()) {
                    auto converted = container(true);
                    const auto length = value.properties.find(u"length");
                    if (value.object() && length != value.properties.end() && !length->second.undefined &&
                        length->second.scalar.kind == ConfigKind::number) {
                        const double n = length->second.scalar.number;
                        if (std::isfinite(n) && n >= 0 && n <= 9007199254740991.0 && std::floor(n) == n) {
                            if (n > static_cast<double>(budget.limits.values)) throw std::runtime_error("Clean merge array-like length budget exceeded");
                            converted.length = static_cast<std::size_t>(n);
                            for (std::size_t i = 0; i < converted.length; ++i) {
                                const auto k = index_key(i); allocate(k.size());
                                converted.properties.emplace(k, property(value, k));
                            }
                        }
                    }
                    value = std::move(converted);
                }
                into(value, item, depth + 1); put(target, key, std::move(value));
            } else if (item.object() && (value.object() || value.nullish())) {
                if (value.nullish()) value = container(false);
                into(value, item, depth + 1); put(target, key, std::move(value));
            } else if (value.undefined || !item.undefined) put(target, key, copy(item));
        }
    }
    ConfigValue emit(const Value& value) {
        budget.step();
        if (value.undefined) throw std::runtime_error("Clean merge result contains undefined values");
        auto result = value.scalar;
        if (value.array()) {
            if (value.properties.size() != value.length) throw std::runtime_error("Clean merge result is not a dense array");
            for (std::size_t i = 0; i < value.length; ++i) {
                const auto found = value.properties.find(index_key(i));
                if (found == value.properties.end()) throw std::runtime_error("Clean merge result has holes or custom array properties");
                result.sequence.push_back(emit(found->second));
            }
        } else for (const auto& [key, child] : value.properties) result.mapping.emplace(key, emit(child));
        return result;
    }
};
}
std::expected<ConfigValue, std::string> clean_and_merge_config(
    const ConfigValue& defaults, const ConfigValue* data, ConfigLimits limits) {
    try {
        Merge merge{{limits}};
        merge.budget.inspect(defaults); if (data) merge.budget.inspect(*data);
        auto result = merge.container(false);
        const auto first = merge.from(defaults); merge.into(result, first);
        if (data) { const auto second = merge.from(*data); merge.into(result, second); }
        auto output = merge.emit(result); merge.budget.output(output); return output;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
