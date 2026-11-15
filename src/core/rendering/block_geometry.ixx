// folia.core.block_geometry — persistent, platform-independent placement index.
// Only compact geometry is retained; text layouts have a separate lifetime.
export module folia.core.block_geometry;
import std;

export namespace folia
{
    class BlockGeometryIndex
    {
    public:
        struct Entry
        {
            float marginTop = 0.0f;
            float height = 0.0f;
            float trailing = 0.0f;
        };

        struct Placement
        {
            float top = 0.0f;
            float bottom = 0.0f;
        };

        void Reset(std::vector<Entry> values, float documentPadding)
        {
            entries = std::move(values);
            padding = documentPadding;
            fenwick.assign(entries.size() + 1, 0.0);
            for (std::size_t index = 0; index < entries.size(); ++index)
            {
                auto treeIndex = index + 1;
                fenwick[treeIndex] += Extent(entries[index]);
                auto parent = treeIndex + LowBit(treeIndex);
                if (parent < fenwick.size()) fenwick[parent] += fenwick[treeIndex];
            }
            initialized = true;
        }

        bool Initialized() const noexcept { return initialized; }
        std::size_t Size() const noexcept { return entries.size(); }

        Placement At(std::size_t index) const
        {
            if (index >= entries.size()) return {};
            auto const& entry = entries[index];
            auto top = padding + Prefix(index) + entry.marginTop;
            return {static_cast<float>(top), static_cast<float>(top + entry.height)};
        }

        void UpdateHeight(std::size_t index, float height)
        {
            if (index >= entries.size()) return;
            auto updated = entries[index];
            updated.height = height;
            Update(index, updated);
        }

        void Update(std::size_t index, Entry updated)
        {
            if (index >= entries.size()) return;
            auto delta = Extent(updated) - Extent(entries[index]);
            entries[index] = updated;
            if (delta == 0.0) return;
            for (auto treeIndex = index + 1; treeIndex < fenwick.size(); treeIndex += LowBit(treeIndex))
                fenwick[treeIndex] += delta;
        }

        std::size_t FirstIntersecting(float documentTop) const
        {
            // Fenwick lower_bound: locate a document position in O(log N),
            // including the original inclusive-bottom hit policy and gaps.
            // Binary searching At() would do O(log^2 N) prefix-sum work.
            std::size_t consumed = 0;
            double prefix = 0.0;
            for (auto step = std::bit_floor(entries.size()); step != 0; step >>= 1)
            {
                auto next = consumed + step;
                if (next > entries.size()) continue;
                auto nextPrefix = prefix + fenwick[next];
                auto bottom = padding + nextPrefix - entries[next - 1].trailing;
                if (bottom < static_cast<double>(documentTop))
                {
                    consumed = next;
                    prefix = nextPrefix;
                }
            }
            return consumed;
        }

        float TotalHeight() const
        {
            return static_cast<float>(PreciseTotalHeight());
        }

        double PreciseTotalHeight() const
        {
            return padding + Prefix(entries.size()) + padding;
        }

    private:
        static std::size_t LowBit(std::size_t value)
        {
            return value & (~value + 1);
        }

        static double Extent(Entry const& entry)
        {
            return static_cast<double>(entry.marginTop) + entry.height + entry.trailing;
        }

        double Prefix(std::size_t count) const
        {
            double result = 0.0;
            for (auto treeIndex = (std::min)(count, entries.size()); treeIndex > 0; treeIndex -= LowBit(treeIndex))
                result += fenwick[treeIndex];
            return result;
        }

        std::vector<Entry> entries;
        // Do not accumulate subpixel deltas in float: at multi-million-DIP
        // document heights an individual edit can otherwise disappear.
        std::vector<double> fenwick;
        float padding = 0.0f;
        bool initialized = false;
    };
}
