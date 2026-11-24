export module folia.markdown.owner_index;
import std;
import folia.markdown.ids;

namespace folia::owner_index_detail {

// Snapshots share the directory and each page. A write copies the directory
// once, then detaches only the pages it actually changes.
template<class T>
class Pages {
    static constexpr std::size_t page_size = std::bit_floor(4096 / sizeof(T));
    using Page = std::vector<T>;

    struct Directory {
        std::vector<std::shared_ptr<Page>> pages;
        std::size_t size = 0;
    };
    std::shared_ptr<Directory> directory_;

    Directory& writable_directory() {
        if (!directory_) directory_ = std::make_shared<Directory>();
        else if (directory_.use_count() != 1) directory_ = std::make_shared<Directory>(*directory_);
        return *directory_;
    }

    static Page& writable_page(Directory& directory, std::size_t index) {
        auto& page = directory.pages[index];
        if (page.use_count() != 1) page = std::make_shared<Page>(*page);
        return *page;
    }

public:
    std::size_t size() const { return directory_ ? directory_->size : 0; }
    T const& operator[](std::size_t index) const {
        return (*directory_->pages[index / page_size])[index % page_size];
    }
    T& edit(std::size_t index) {
        return writable_page(writable_directory(), index / page_size)[index % page_size];
    }
    void clear() { directory_.reset(); }

    void resize(std::size_t size) {
        auto previous = this->size();
        if (size == previous) return;
        if (size == 0) { clear(); return; }
        auto& directory = writable_directory();
        auto page_count = (size - 1) / page_size + 1;
        auto last_size = (size - 1) % page_size + 1;
        if (size < previous) {
            directory.pages.resize(page_count);
            writable_page(directory, page_count - 1).resize(last_size);
        } else {
            if (previous != 0 && previous % page_size != 0) {
                auto& page = writable_page(directory, (previous - 1) / page_size);
                auto requested = page_count == directory.pages.size() ? last_size : page_size;
                if (requested > page.capacity())
                    page.reserve((std::min)(page_size, (std::max)(requested, 2 * page.capacity())));
                page.resize(requested);
            }
            while (directory.pages.size() < page_count) {
                auto count = directory.pages.size() + 1 == page_count ? last_size : page_size;
                directory.pages.push_back(std::make_shared<Page>(count));
            }
        }
        directory.size = size;
    }

    bool shares_storage(Pages const& other) const { return directory_ == other.directory_; }
};

} // namespace folia::owner_index_detail

export namespace folia {

class OwnerOrder {
    owner_index_detail::Pages<NodeId> ids_;

public:
    struct Iterator {
        using value_type = NodeId;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using reference = NodeId;
        using pointer = void;

        OwnerOrder const* order = nullptr;
        std::size_t index = 0;
        NodeId operator*() const { return (*order)[index]; }
        Iterator& operator++() { ++index; return *this; }
        Iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        bool operator==(Iterator const&) const = default;
    };

    std::size_t size() const { return ids_.size(); }
    bool empty() const { return size() == 0; }
    NodeId operator[](std::size_t index) const { return ids_[index]; }
    NodeId front() const { return (*this)[0]; }
    NodeId back() const { return (*this)[size() - 1]; }
    Iterator begin() const { return {this, 0}; }
    Iterator end() const { return {this, size()}; }
    void clear() { ids_.clear(); }
    void resize(std::size_t length) { ids_.resize(length); }
    void set(std::size_t index, NodeId owner) { ids_.edit(index) = owner; }
    void push_back(NodeId owner) {
        auto index = size();
        resize(index + 1);
        set(index, owner);
    }

    void insert(std::size_t index, std::span<const NodeId> owners) {
        if (owners.empty()) return;
        auto previous_size = size();
        resize(previous_size + owners.size());
        for (auto i = previous_size; i > index; --i)
            set(i - 1 + owners.size(), (*this)[i - 1]);
        for (auto owner : owners) set(index++, owner);
    }

    bool operator==(OwnerOrder const& other) const {
        return ids_.shares_storage(other.ids_) || std::ranges::equal(*this, other);
    }
};

// Open addressing keeps a whole group of keys in one allocation. Zero is the
// unassigned NodeId; a separate value on a zero key denotes a deleted slot.
class OwnerIndex {
public:
    struct Entry {
        std::uint64_t first = 0;
        std::size_t second = 0;
    };

private:
    owner_index_detail::Pages<Entry> slots_;
    std::size_t count_ = 0;
    std::size_t deleted_ = 0;
    static constexpr auto missing = (std::numeric_limits<std::size_t>::max)();

    static std::uint64_t hash(std::uint64_t key) {
        key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ull;
        key = (key ^ (key >> 27)) * 0x94d049bb133111ebull;
        return key ^ (key >> 31);
    }

    std::size_t lookup(std::uint64_t key) const {
        if (key == 0 || count_ == 0) return missing;
        auto mask = slots_.size() - 1;
        for (auto index = hash(key) & mask;; index = (index + 1) & mask) {
            const auto& entry = slots_[index];
            if (entry.first == key) return index;
            if (entry.first == 0 && entry.second != missing) return missing;
        }
    }

    void insert_new(std::uint64_t key, std::size_t value) {
        auto mask = slots_.size() - 1;
        auto index = hash(key) & mask;
        while (slots_[index].first != 0) index = (index + 1) & mask;
        if (slots_[index].second == missing) --deleted_;
        slots_.edit(index) = {key, value};
        ++count_;
    }

    void rehash(std::size_t capacity) {
        OwnerIndex next;
        next.slots_.resize(capacity);
        for (std::size_t index = 0; index < slots_.size(); ++index) {
            const auto& entry = slots_[index];
            if (entry.first != 0) next.insert_new(entry.first, entry.second);
        }
        *this = std::move(next);
    }

public:
    OwnerIndex() = default;
    OwnerIndex(OwnerIndex const&) = default;
    OwnerIndex& operator=(OwnerIndex const&) = default;
    OwnerIndex(OwnerIndex&& other) noexcept
        : slots_(std::move(other.slots_)),
          count_(std::exchange(other.count_, 0)),
          deleted_(std::exchange(other.deleted_, 0)) {}
    OwnerIndex& operator=(OwnerIndex&& other) noexcept {
        if (this != &other) {
            slots_ = std::move(other.slots_);
            count_ = std::exchange(other.count_, 0);
            deleted_ = std::exchange(other.deleted_, 0);
        }
        return *this;
    }

    std::size_t size() const { return count_; }
    void clear() { *this = {}; }
    Entry const* end() const { return nullptr; }
    Entry const* find(std::uint64_t key) const {
        auto index = lookup(key);
        return index == missing ? nullptr : &slots_[index];
    }
    bool contains(std::uint64_t key) const { return find(key) != nullptr; }
    std::size_t at(std::uint64_t key) const {
        if (auto entry = find(key)) return entry->second;
        throw std::out_of_range("unknown document owner");
    }

    void set(std::uint64_t key, std::size_t value) {
        if (key == 0) throw std::invalid_argument("unassigned document owner");
        if (auto index = lookup(key); index != missing) {
            if (slots_[index].second != value) slots_.edit(index).second = value;
            return;
        }
        auto capacity = slots_.size();
        if (capacity == 0) rehash(8);
        else if (count_ + deleted_ >= capacity - capacity / 4) {
            if (count_ >= capacity - capacity / 4) {
                if (capacity > missing / 2) throw std::length_error("document owner index");
                capacity *= 2;
            }
            rehash(capacity);
        }
        insert_new(key, value);
    }

    void erase(std::uint64_t key) {
        if (auto index = lookup(key); index != missing) {
            slots_.edit(index) = {0, missing};
            --count_;
            ++deleted_;
        }
    }

    bool operator==(OwnerIndex const& other) const {
        if (slots_.shares_storage(other.slots_)) return true;
        if (count_ != other.count_) return false;
        for (std::size_t index = 0; index < slots_.size(); ++index) {
            const auto& entry = slots_[index];
            if (entry.first != 0) {
                auto right = other.find(entry.first);
                if (!right || entry.second != right->second) return false;
            }
        }
        return true;
    }
};

} // namespace folia
