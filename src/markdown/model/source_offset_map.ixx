// Exact scalar boundary maps for derived content. Consecutive source offsets
// and repeated offsets occupy one run; the usual linear projection allocates
// nothing. The authoritative positions remain offsets into the owner's source.
export module folia.markdown.source_offset_map;
import std;

export namespace folia {

class SourceOffsetMap {
    struct Run {
        std::size_t end = 0;
        std::size_t source = 0;
        bool repeated = false;
        bool operator==(Run const&) const = default;
    };

    using Runs = std::vector<Run>;
    using Dense = std::vector<std::size_t>;
    Run first_;
    // Highly discontinuous projections can cost more as runs than as literal
    // offsets. Keep the vector representation only when its measured storage
    // is smaller; ordinary linear maps keep an empty run tail.
    std::variant<Runs, Dense> storage_;

    Dense const* dense() const { return std::get_if<Dense>(&storage_); }
    void bound_run_storage() {
        const auto& following = std::get<Runs>(storage_);
        if (following.capacity() * sizeof(Run) <= size() * sizeof(std::size_t)) return;
        Dense values;
        values.reserve(size());
        for (const auto value : *this) values.push_back(value);
        storage_ = std::move(values);
    }

    Run const& run(std::size_t index) const {
        return index == 0 ? first_ : std::get<Runs>(storage_)[index - 1];
    }
    std::size_t run_start(std::size_t index) const {
        return index == 0 ? 0 : run(index - 1).end;
    }
    std::size_t run_for(std::size_t index) const {
        if (dense()) return 0;
        const auto& following = std::get<Runs>(storage_);
        if (index < first_.end || following.empty()) return 0;
        const auto found = std::ranges::upper_bound(following, index, {}, &Run::end);
        return found == following.end() ? following.size()
            : static_cast<std::size_t>(found - following.begin()) + 1;
    }
    std::size_t value_in_run(std::size_t index, std::size_t run_index) const {
        if (const auto* values = dense()) return (*values)[index];
        const auto& part = run(run_index);
        return part.source + (part.repeated ? 0 : index - run_start(run_index));
    }
    void append_run(std::size_t source, std::size_t count, bool repeated) {
        if (count == 0) return;
        if (count > (std::numeric_limits<std::size_t>::max)() - size()
            || (!repeated && count - 1 > (std::numeric_limits<std::size_t>::max)() - source)) {
            throw std::length_error("source offset map exceeds its offset range");
        }
        if (auto* values = std::get_if<Dense>(&storage_)) {
            if (repeated) values->insert(values->end(), count, source);
            else for (std::size_t index = 0; index < count; ++index) values->push_back(source + index);
            return;
        }
        if (empty()) {
            first_ = {count, source, repeated};
            return;
        }
        auto& following = std::get<Runs>(storage_);
        auto& last = following.empty() ? first_ : following.back();
        const auto begin = run_start(following.size());
        const auto last_value = last.source + (last.repeated ? 0 : last.end - begin - 1);
        const bool contiguous = repeated ? source == last_value
            : last_value != (std::numeric_limits<std::size_t>::max)() && source == last_value + 1;
        if (contiguous && (last.repeated == repeated || last.end - begin == 1)) {
            last.repeated = repeated;
            last.end += count;
            return;
        }
        following.push_back({size() + count, source, repeated});
        bound_run_storage();
    }

public:
    using value_type = std::size_t;
    using size_type = std::size_t;

    // A value iterator keeps the current run so ordinary forward/reverse
    // traversal is linear in the number of values, without a binary lookup
    // for every scalar. Arbitrary seeks locate their run in logarithmic time.
    class const_iterator {
        friend class SourceOffsetMap;
        SourceOffsetMap const* map_ = nullptr;
        std::size_t index_ = 0;
        std::size_t run_ = 0;
        const_iterator(SourceOffsetMap const* map, std::size_t index)
            : map_(map), index_(index), run_(map->run_for(index)) {}
    public:
        using value_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type;
        using pointer = void;
        using iterator_category = std::bidirectional_iterator_tag;
        using iterator_concept = std::bidirectional_iterator_tag;

        const_iterator() = default;
        value_type operator*() const { return map_->value_in_run(index_, run_); }
        value_type operator[](difference_type offset) const { return *(*this + offset); }
        const_iterator& operator++() {
            ++index_;
            if (!map_->dense() && index_ == map_->run(run_).end && index_ < map_->size()) ++run_;
            return *this;
        }
        const_iterator operator++(int) { auto previous = *this; ++*this; return previous; }
        const_iterator& operator--() {
            --index_;
            if (!map_->dense() && index_ < map_->run_start(run_)) --run_;
            return *this;
        }
        const_iterator operator--(int) { auto previous = *this; --*this; return previous; }
        const_iterator& operator+=(difference_type offset) {
            index_ = static_cast<std::size_t>(static_cast<difference_type>(index_) + offset);
            if (!map_->dense() && (index_ < map_->run_start(run_) || index_ >= map_->run(run_).end))
                run_ = map_->run_for(index_);
            return *this;
        }
        const_iterator& operator-=(difference_type offset) { return *this += -offset; }
        friend const_iterator operator+(const_iterator it, difference_type offset) { return it += offset; }
        friend const_iterator operator+(difference_type offset, const_iterator it) { return it += offset; }
        friend const_iterator operator-(const_iterator it, difference_type offset) { return it -= offset; }
        friend difference_type operator-(const_iterator left, const_iterator right) {
            return static_cast<difference_type>(left.index_) - static_cast<difference_type>(right.index_);
        }
        friend bool operator==(const_iterator left, const_iterator right) {
            return left.map_ == right.map_ && left.index_ == right.index_;
        }
        friend auto operator<=>(const_iterator left, const_iterator right) {
            return left.index_ <=> right.index_;
        }
    };

    SourceOffsetMap() = default;
    template<std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    SourceOffsetMap(Iterator first, Sentinel last) {
        for (; first != last; ++first) push_back(*first);
    }
    SourceOffsetMap(std::initializer_list<std::size_t> values)
        : SourceOffsetMap(values.begin(), values.end()) {}

    bool empty() const { return size() == 0; }
    std::size_t size() const {
        if (const auto* values = dense()) return values->size();
        const auto& following = std::get<Runs>(storage_);
        return following.empty() ? first_.end : following.back().end;
    }
    std::size_t run_count() const {
        if (const auto* values = dense()) return values->size();
        return empty() ? 0 : std::get<Runs>(storage_).size() + 1;
    }
    bool uses_dense_storage() const { return dense() != nullptr; }
    std::size_t allocated_bytes() const {
        if (const auto* values = dense()) return values->capacity() * sizeof(std::size_t);
        return std::get<Runs>(storage_).capacity() * sizeof(Run);
    }
    std::size_t operator[](std::size_t index) const { return value_in_run(index, run_for(index)); }
    std::size_t at(std::size_t index) const {
        if (index >= size()) throw std::out_of_range("source offset map index");
        return (*this)[index];
    }
    std::size_t front() const { return dense() ? dense()->front() : first_.source; }
    std::size_t back() const {
        if (const auto* values = dense()) return values->back();
        return value_in_run(size() - 1, std::get<Runs>(storage_).size());
    }
    const_iterator begin() const { return {this, 0}; }
    const_iterator end() const { return {this, size()}; }

    void clear() { first_ = {}; storage_.emplace<Runs>(); }
    void assign(std::size_t count, std::size_t source) {
        clear();
        append_run(source, count, true);
    }
    void assign_linear(std::size_t source, std::size_t count) {
        clear();
        append_run(source, count, false);
    }
    void append_linear(std::size_t source, std::size_t count) { append_run(source, count, false); }
    void push_back(std::size_t source) {
        if (!empty() && source == back()) append_run(source, 1, true);
        else append_run(source, 1, false);
    }

    SourceOffsetMap slice(std::size_t begin, std::size_t end) const {
        if (begin > end || end > size()) throw std::out_of_range("source offset map slice");
        SourceOffsetMap result;
        if (const auto* values = dense())
            return SourceOffsetMap(values->begin() + begin, values->begin() + end);
        auto index = run_for(begin);
        while (begin < end) {
            const auto& part = run(index);
            const auto stop = (std::min)(part.end, end);
            result.append_run(value_in_run(begin, index), stop - begin, part.repeated);
            begin = stop;
            ++index;
        }
        return result;
    }

    // As with std::lower_bound, values must be nondecreasing. All parser-owned
    // content projections satisfy this; gaps and repeated entity offsets are
    // retained exactly. Search runs directly instead of binary-searching an
    // iterator whose arbitrary seeks also search the run sequence.
    const_iterator lower_bound(std::size_t source) const {
        if (empty()) return end();
        if (const auto* values = dense()) {
            const auto found = std::ranges::lower_bound(*values, source);
            return {this, static_cast<std::size_t>(found - values->begin())};
        }
        auto first = std::size_t{0};
        auto last = run_count();
        while (first < last) {
            const auto middle = first + (last - first) / 2;
            const auto& part = run(middle);
            const auto final = value_in_run(part.end - 1, middle);
            if (final < source) first = middle + 1;
            else last = middle;
        }
        if (first == run_count()) return end();
        const auto& part = run(first);
        const auto offset = part.repeated || source <= part.source ? 0 : source - part.source;
        return {this, run_start(first) + offset};
    }

    friend bool operator==(SourceOffsetMap const& left, SourceOffsetMap const& right) {
        if (left.size() != right.size()) return false;
        if (left.first_ == right.first_ && left.storage_ == right.storage_) return true;
        return std::ranges::equal(left, right);
    }
};

static_assert(std::bidirectional_iterator<SourceOffsetMap::const_iterator>);
static_assert(!std::random_access_iterator<SourceOffsetMap::const_iterator>);
static_assert(std::ranges::sized_range<SourceOffsetMap>);

} // namespace folia
