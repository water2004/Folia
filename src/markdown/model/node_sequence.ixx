export module folia.markdown.node_sequence;
import std;

export namespace folia {

// Stores the size and capacity with the allocation. Empty child sequences
// require only a null pointer. The elements retain contiguous value storage.
template<class T>
class NodeSequence {
    struct Header {
        std::size_t size = 0;
        std::size_t capacity = 0;
    };
    std::byte* storage_ = nullptr;

    static constexpr std::size_t alignment() { return (std::max)(alignof(T), alignof(Header)); }
    static constexpr std::size_t offset() {
        return (sizeof(Header) + alignof(T) - 1) & ~(alignof(T) - 1);
    }
    Header& header() {
        return *std::launder(reinterpret_cast<Header*>(storage_));
    }
    Header const& header() const {
        return *std::launder(reinterpret_cast<Header const*>(storage_));
    }
    void allocate(std::size_t capacity) {
        if (capacity > ((std::numeric_limits<std::size_t>::max)() - offset()) / sizeof(T))
            throw std::length_error("node sequence");
        storage_ = static_cast<std::byte*>(::operator new(
            offset() + capacity * sizeof(T), std::align_val_t{alignment()}));
        std::construct_at(reinterpret_cast<Header*>(storage_), Header{0, capacity});
    }
    void release() noexcept {
        if (!storage_) return;
        clear();
        std::destroy_at(&header());
        ::operator delete(std::exchange(storage_, nullptr), std::align_val_t{alignment()});
    }
    void grow(std::size_t needed) {
        auto current = capacity();
        auto slack = (std::min)(current / 2 + 1, (std::numeric_limits<std::size_t>::max)() - current);
        reserve((std::max)(needed, current + slack));
    }

public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = T const*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    NodeSequence() = default;
    NodeSequence(std::initializer_list<T> entries) {
        NodeSequence next;
        next.reserve(entries.size());
        for (auto const& entry : entries) next.emplace_back(entry);
        swap(next);
    }
    NodeSequence(NodeSequence const& other) {
        NodeSequence next;
        next.reserve(other.size());
        for (auto const& entry : other) next.emplace_back(entry);
        swap(next);
    }
    NodeSequence(NodeSequence&& other) noexcept : storage_(std::exchange(other.storage_, nullptr)) {}
    ~NodeSequence() { release(); }
    NodeSequence& operator=(NodeSequence const& other) {
        if (this != &other) { NodeSequence next(other); swap(next); }
        return *this;
    }
    NodeSequence& operator=(NodeSequence&& other) noexcept {
        if (this != &other) {
            release();
            storage_ = std::exchange(other.storage_, nullptr);
        }
        return *this;
    }
    void swap(NodeSequence& other) noexcept { std::swap(storage_, other.storage_); }

    std::size_t size() const { return storage_ ? header().size : 0; }
    std::size_t capacity() const { return storage_ ? header().capacity : 0; }
    bool empty() const { return size() == 0; }
    T* data() { return storage_ ? reinterpret_cast<T*>(storage_ + offset()) : nullptr; }
    T const* data() const { return storage_ ? reinterpret_cast<T const*>(storage_ + offset()) : nullptr; }
    T* begin() { return data(); }
    T* end() { return data() + size(); }
    T const* begin() const { return data(); }
    T const* end() const { return data() + size(); }
    auto rbegin() { return std::reverse_iterator(end()); }
    auto rend() { return std::reverse_iterator(begin()); }
    auto rbegin() const { return std::reverse_iterator(end()); }
    auto rend() const { return std::reverse_iterator(begin()); }
    T& operator[](std::size_t index) { return data()[index]; }
    T const& operator[](std::size_t index) const { return data()[index]; }
    T& front() { return (*this)[0]; }
    T const& front() const { return (*this)[0]; }
    T& back() { return (*this)[size() - 1]; }
    T const& back() const { return (*this)[size() - 1]; }

    void reserve(std::size_t capacity) {
        if (capacity <= this->capacity()) return;
        NodeSequence next;
        next.allocate(capacity);
        for (auto& entry : *this) {
            std::construct_at(next.end(), std::move_if_noexcept(entry));
            ++next.header().size;
        }
        swap(next);
    }
    template<class... Args>
    T& emplace_back(Args&&... args) {
        if (size() == capacity()) {
            // Materialize before growth, since a caller can insert an element
            // already in the sequence.
            T entry(std::forward<Args>(args)...);
            grow(size() + 1);
            std::construct_at(end(), std::move(entry));
        } else {
            std::construct_at(end(), std::forward<Args>(args)...);
        }
        ++header().size;
        return back();
    }
    void push_back(T entry) {
        if (size() == capacity()) grow(size() + 1);
        std::construct_at(end(), std::move(entry));
        ++header().size;
    }
    void pop_back() {
        std::destroy_at(&back());
        --header().size;
    }
    void clear() noexcept {
        while (!empty()) pop_back();
    }
    T* erase(T const* first, T const* last) {
        auto start = static_cast<std::size_t>(first - begin());
        auto count = static_cast<std::size_t>(last - first);
        if (count == 0) return begin() + start;
        auto close = std::move(begin() + start + count, end(), begin() + start);
        std::destroy(close, end());
        header().size -= count;
        return begin() + start;
    }
    T* erase(T const* entry) { return erase(entry, entry + 1); }
    T* insert(T const* position, T entry) {
        auto index = static_cast<std::size_t>(position - begin());
        push_back(std::move(entry));
        std::rotate(begin() + index, end() - 1, end());
        return begin() + index;
    }
    template<class Iterator>
    T* insert(T const* position, Iterator first, Iterator last) {
        auto index = static_cast<std::size_t>(position - begin());
        auto previous = size();
        if constexpr (std::is_base_of_v<std::forward_iterator_tag,
            typename std::iterator_traits<Iterator>::iterator_category>) {
            auto count = static_cast<std::size_t>(std::distance(first, last));
            if (count > capacity() - previous) grow(previous + count);
        }
        for (; first != last; ++first) emplace_back(*first);
        std::rotate(begin() + index, begin() + previous, end());
        return begin() + index;
    }
};

} // namespace folia
