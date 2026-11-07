// folia.platform.editor_display_mapping — source/display coordinate bridge.
export module folia.platform.editor_display_mapping;
import std;
export import folia.markdown.ids;
export import folia.markdown.selection;
export import folia.markdown.text_edit;

export namespace folia::platform::editor
{
    enum class EditorDisplayPositionKind
    {
        Source,
        Generated,
        GeneratedPreview,
        BoundaryDecoration,
    };

    enum class EditorFootnoteControlKind
    {
        Reference,
        DefinitionLabel,
    };

    struct EditorDisplayPosition : folia::TextPosition
    {
        EditorDisplayPositionKind kind = EditorDisplayPositionKind::Source;

        EditorDisplayPosition() = default;
        EditorDisplayPosition(
            folia::TextPosition position,
            EditorDisplayPositionKind displayKind = EditorDisplayPositionKind::Source)
            : folia::TextPosition(position), kind(displayKind)
        {
        }
        EditorDisplayPosition(
            folia::NodeId containerId,
            std::size_t sourceOffset,
            folia::TextAffinity affinity,
            EditorDisplayPositionKind displayKind = EditorDisplayPositionKind::Source)
            : folia::TextPosition{containerId, sourceOffset, affinity}, kind(displayKind)
        {
        }
    };

    // Display offsets remain UTF-16 units; source offsets remain block-local
    // code points. Store constant or linear runs instead of one 32-byte object
    // per displayed unit. Copies are independent, but proportional to runs.
    class EditorDisplayMapping
    {
        struct Run
        {
            std::size_t begin;
            std::size_t length;
            EditorDisplayPosition first;
            bool linear;
        };
        std::vector<Run> runs_;
        std::size_t size_ = 0;

        static bool same_context(EditorDisplayPosition a, EditorDisplayPosition b)
        {
            return a.container_id == b.container_id && a.affinity == b.affinity && a.kind == b.kind;
        }
        static EditorDisplayPosition at(Run const& run, std::size_t offset)
        {
            auto point = run.first;
            if (run.linear) point.source_offset += offset;
            return point;
        }
        std::size_t locate(std::size_t offset) const
        {
            return static_cast<std::size_t>(std::upper_bound(runs_.begin(), runs_.end(), offset,
                [](auto value, Run const& run) { return value < run.begin; }) - runs_.begin() - 1);
        }
        void append_run(EditorDisplayPosition first, std::size_t count, bool linear)
        {
            if (count == 0) return;
            if (!runs_.empty())
            {
                auto& last = runs_.back();
                if (same_context(last.first, first))
                {
                    if (last.length == 1 && first.source_offset == last.first.source_offset + (linear ? 1 : 0))
                        last.linear = linear;
                    if (last.linear == linear && at(last, last.length).source_offset == first.source_offset)
                    {
                        last.length += count;
                        size_ += count;
                        return;
                    }
                }
            }
            runs_.push_back({size_, count, first, linear});
            size_ += count;
        }
        void append_slice(EditorDisplayMapping const& source, std::size_t begin, std::size_t end)
        {
            if (begin == end) return;
            for (auto index = source.locate(begin); index < source.runs_.size(); ++index)
            {
                auto const& run = source.runs_[index];
                if (run.begin >= end) break;
                auto first = (std::max)(begin, run.begin);
                auto last = (std::min)(end, run.begin + run.length);
                append_run(at(run, first - run.begin), last - first, run.linear);
            }
        }
    public:
        // Read-only random access: a position is a value derived from its run.
        // Mutation goes through insert/erase, never through a second mutable
        // character array. Iteration caches its run for sequential traversal.
        class const_iterator
        {
            friend class EditorDisplayMapping;
            EditorDisplayMapping const* owner_ = nullptr;
            std::size_t offset_ = 0;
            mutable std::size_t run_ = 0;
            mutable EditorDisplayPosition value_;
            const_iterator(EditorDisplayMapping const* owner, std::size_t offset) : owner_(owner), offset_(offset) {}
        public:
            using value_type = EditorDisplayPosition;
            using difference_type = std::ptrdiff_t;
            using reference = value_type;
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            const_iterator() = default;
            value_type operator*() const
            {
                if (run_ >= owner_->runs_.size() || offset_ < owner_->runs_[run_].begin
                    || offset_ >= owner_->runs_[run_].begin + owner_->runs_[run_].length)
                    run_ = owner_->locate(offset_);
                return at(owner_->runs_[run_], offset_ - owner_->runs_[run_].begin);
            }
            value_type const* operator->() const { value_ = **this; return &value_; }
            value_type operator[](difference_type n) const { return *(*this + n); }
            const_iterator& operator++() { ++offset_; return *this; }
            const_iterator operator++(int) { auto old = *this; ++*this; return old; }
            const_iterator& operator--() { --offset_; return *this; }
            const_iterator operator--(int) { auto old = *this; --*this; return old; }
            const_iterator& operator+=(difference_type n) { offset_ = static_cast<std::size_t>(static_cast<difference_type>(offset_) + n); return *this; }
            const_iterator& operator-=(difference_type n) { return *this += -n; }
            friend const_iterator operator+(const_iterator i, difference_type n) { return i += n; }
            friend const_iterator operator+(difference_type n, const_iterator i) { return i += n; }
            friend const_iterator operator-(const_iterator i, difference_type n) { return i -= n; }
            friend difference_type operator-(const_iterator a, const_iterator b) { return static_cast<difference_type>(a.offset_) - static_cast<difference_type>(b.offset_); }
            friend bool operator==(const_iterator a, const_iterator b) { return a.owner_ == b.owner_ && a.offset_ == b.offset_; }
            friend auto operator<=>(const_iterator a, const_iterator b) { return a.offset_ <=> b.offset_; }
        };
        using iterator = const_iterator;
        EditorDisplayMapping() = default;
        EditorDisplayMapping(EditorDisplayMapping const&) = default;
        EditorDisplayMapping& operator=(EditorDisplayMapping const&) = default;
        EditorDisplayMapping(EditorDisplayMapping&& other) noexcept
            : runs_(std::move(other.runs_)), size_(std::exchange(other.size_, 0)) {}
        EditorDisplayMapping& operator=(EditorDisplayMapping&& other) noexcept
        {
            if (this != &other) {
                runs_ = std::move(other.runs_);
                size_ = std::exchange(other.size_, 0);
            }
            return *this;
        }
        EditorDisplayMapping(std::initializer_list<EditorDisplayPosition> points) { for (auto point : points) push_back(point); }
        EditorDisplayMapping(std::size_t count, EditorDisplayPosition point) { append_run(point, count, false); }
        std::size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }
        std::size_t storage_bytes() const { return runs_.capacity() * sizeof(Run); }
        bool contains_kind(EditorDisplayPositionKind kind) const
        {
            return std::ranges::any_of(runs_, [kind](Run const& run) { return run.first.kind == kind; });
        }
        // Visit clipped, nonempty runs in display order. All positions in a
        // run share owner/kind/affinity and their source offsets are constant
        // or increase by one. Return false from the visitor to stop early.
        template<class Visitor>
        void for_each_run(std::size_t begin, std::size_t end, Visitor&& visitor) const
        {
            begin = (std::min)(begin, size_);
            end = (std::clamp)(end, begin, size_);
            if (begin == end) return;
            for (auto index = locate(begin); index < runs_.size(); ++index) {
                auto const& run = runs_[index];
                if (run.begin >= end) break;
                auto first = (std::max)(begin, run.begin);
                auto last = (std::min)(end, run.begin + run.length);
                if (!visitor(first, last, at(run, first - run.begin), at(run, last - run.begin - 1))) break;
            }
        }
        void clear() { runs_.clear(); size_ = 0; }
        const_iterator begin() const { return {this, 0}; }
        const_iterator end() const { return {this, size_}; }
        EditorDisplayPosition operator[](std::size_t offset) const { auto const& run = runs_[locate(offset)]; return at(run, offset - run.begin); }
        EditorDisplayPosition front() const { return (*this)[0]; }
        EditorDisplayPosition back() const { return (*this)[size_ - 1]; }
        void pop_back() { resize(size_ - 1); }
        void push_back(EditorDisplayPosition point)
        {
            if (!runs_.empty())
            {
                auto const& last = runs_.back();
                if (same_context(last.first, point))
                {
                    auto previous = at(last, last.length - 1).source_offset;
                    if (point.source_offset == previous || point.source_offset == previous + 1)
                    {
                        append_run(point, 1, point.source_offset != previous);
                        return;
                    }
                }
            }
            append_run(point, 1, false);
        }
        const_iterator insert(const_iterator position, std::size_t count, EditorDisplayPosition point)
        {
            auto offset = position.offset_;
            if (offset == size_) append_run(point, count, false);
            else
            {
                EditorDisplayMapping replacement;
                replacement.append_slice(*this, 0, offset);
                replacement.append_run(point, count, false);
                replacement.append_slice(*this, offset, size_);
                *this = std::move(replacement);
            }
            return {this, offset};
        }
        const_iterator insert(const_iterator position, EditorDisplayPosition point) { return insert(position, 1, point); }
        const_iterator insert(const_iterator position, const_iterator first, const_iterator last)
        {
            auto offset = position.offset_;
            if (offset == size_ && first.owner_ != this)
                append_slice(*first.owner_, first.offset_, last.offset_);
            else
            {
                EditorDisplayMapping replacement;
                replacement.append_slice(*this, 0, offset);
                replacement.append_slice(*first.owner_, first.offset_, last.offset_);
                replacement.append_slice(*this, offset, size_);
                *this = std::move(replacement);
            }
            return {this, offset};
        }
        const_iterator erase(const_iterator first, const_iterator last)
        {
            auto offset = first.offset_;
            EditorDisplayMapping replacement;
            replacement.append_slice(*this, 0, offset);
            replacement.append_slice(*this, last.offset_, size_);
            *this = std::move(replacement);
            return {this, offset};
        }
        void resize(std::size_t count)
        {
            if (count < size_)
            {
                while (!runs_.empty() && runs_.back().begin >= count) runs_.pop_back();
                if (!runs_.empty()) runs_.back().length = count - runs_.back().begin;
                size_ = count;
            }
            else append_run({}, count - size_, false);
        }
    };

    // Map an ordered source selection to its displayed intervals. Source order
    // across owners is supplied by the document, not inferred from NodeIds.
    template<class Less>
    std::vector<std::pair<std::size_t, std::size_t>> DisplayRangesForSourceSelection(
        EditorDisplayMapping const& mapping, folia::TextPosition start, folia::TextPosition end, Less&& less)
    {
        std::vector<std::pair<std::size_t, std::size_t>> ranges;
        auto count = mapping.empty() ? 0 : mapping.size() - 1;
        mapping.for_each_run(0, count, [&](auto begin, auto limit, auto first, auto last) {
            if (less(last, start) || !less(first, end)) return true;
            if (first.source_offset != last.source_offset) {
                auto runBegin = begin;
                if (first.container_id == start.container_id && start.source_offset > first.source_offset)
                    begin += start.source_offset - first.source_offset;
                if (first.container_id == end.container_id && end.source_offset <= last.source_offset)
                    limit = runBegin + end.source_offset - first.source_offset;
            }
            if (begin < limit) {
                if (!ranges.empty() && ranges.back().second == begin) ranges.back().second = limit;
                else ranges.emplace_back(begin, limit);
            }
            return true;
        });
        return ranges;
    }

    inline std::optional<std::pair<std::size_t, std::size_t>>
    SourceDisplayRangeForContainer(
        EditorDisplayMapping const& mapping,
        folia::NodeId containerId,
        std::size_t displayCharacterCount)
    {
        auto const limit = (std::min)(mapping.size(), displayCharacterCount);
        std::optional<std::size_t> first;
        std::size_t last = 0;
        mapping.for_each_run(0, limit, [&](std::size_t begin, std::size_t end, auto position, auto) {
            if (position.container_id != containerId
                || position.kind != EditorDisplayPositionKind::Source)
                return true;
            if (!first) first = begin;
            last = end;
            return true;
        });
        return first
            ? std::optional{std::pair{*first, last}}
            : std::nullopt;
    }

    inline std::size_t DisplayPositionForSource(
        EditorDisplayMapping const& mapping,
        folia::TextPosition sourcePosition)
    {
        if (mapping.empty()) return 0;

        std::optional<std::size_t> lastInContainer;
        std::optional<std::size_t> exactSource;
        std::optional<std::size_t> exactGeneratedAffinity;
        std::optional<std::size_t> firstGreater;
        std::optional<std::size_t> resolved;
        mapping.for_each_run(0, mapping.size(), [&](std::size_t begin, std::size_t end, auto point, auto last) {
            if (point.container_id != sourcePosition.container_id) return true;
            lastInContainer = end - 1;
            auto target = sourcePosition.source_offset;
            if (last.source_offset < target) return true;
            if (point.source_offset > target) {
                if (!firstGreater) firstGreater = begin;
                return true;
            }
            auto linear = point.source_offset != last.source_offset;
            auto index = begin + (linear ? target - point.source_offset : 0);
            if (last.source_offset > target && !firstGreater) firstGreater = index + 1;

            // An unaccepted insertion occupies display space at the caret but
            // owns no source characters. Keep the caret before that run.
            if (point.kind == EditorDisplayPositionKind::GeneratedPreview
                && point.affinity == sourcePosition.affinity) { resolved = index; return false; }

            if (point.kind == EditorDisplayPositionKind::Source)
            {
                if (point.affinity == sourcePosition.affinity) { resolved = index; return false; }
                if (!exactSource) exactSource = index;
            }
            else if (point.affinity == sourcePosition.affinity)
            {
                // A downstream boundary decoration is a generated prefix
                // (quote/list/callout indentation, bullet, task control, ...).
                // The source position begins after the complete prefix, not
                // before its first glyph. Empty content nodes have no Source
                // mapping to supersede this candidate, which is why forward
                // cross-block movement previously drew their caret at the
                // far-left container edge. Upstream decorations are suffixes;
                // they intentionally keep the first boundary before the run.
                if (point.kind == EditorDisplayPositionKind::BoundaryDecoration
                    && sourcePosition.affinity == folia::TextAffinity::Downstream)
                {
                    auto lastExact = linear ? index : end - 1;
                    exactGeneratedAffinity = (std::min)(lastExact + 1, mapping.size() - 1);
                }
                else if (!exactGeneratedAffinity)
                {
                    exactGeneratedAffinity = index;
                }
            }
            return true;
        });
        return resolved.value_or(exactGeneratedAffinity.value_or(exactSource.value_or(firstGreater.value_or(lastInContainer.value_or(0)))));
    }
}
