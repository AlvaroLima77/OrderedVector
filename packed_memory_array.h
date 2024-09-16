#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

template <typename ItemType, typename Comparator = std::less<ItemType>, uint32_t chunk_size = 8>
class packed_memory_array {
public:
    static_assert(chunk_size > 0, "Chunk size must be greater than 0");
    inline packed_memory_array() : items(chunk_size * 2) {}

    inline void push(const ItemType& item) {
        int i = index_of(item);
        int block_begin = (i / chunk_size) * chunk_size;
        int block_end = block_begin + chunk_size;
        int count = count_items(block_begin, block_end) + 1;
        float lower, upper;
        get_thresholds(&lower, &upper, tree_height());
        float density = (float)count / (float)(block_end - block_begin);
        if (density > upper) {
            scan(block_begin, block_end, count, tree_height() - 1);
            i = index_of(item);
        }

        if (items[i]) {
            int closest_gap = get_closest_gap(i);
            bool is_on_right = closest_gap > i;
            if (is_on_right && item > items[i])
                i++;
            else if (!is_on_right && item < items[i])
                i--;

            is_on_right ? shift_right(i, closest_gap) : shift_left(i, closest_gap);
        }
        items[i] = item;
    }

    inline void remove(const ItemType& target) {
        int i = index_of(target);
        if (!items[i] || items[i] != target)
            return;

        items[i].reset();
        int block_begin = (i / chunk_size) * chunk_size;
        int block_end = block_begin + chunk_size;
        int count = count_items(block_begin, block_end);
        float lower, upper;
        get_thresholds(&lower, &upper, tree_height());
        float density = (float)count / (float)(block_end - block_begin);
        if (density < lower)
            scan(block_begin, block_end, count, tree_height() - 1);
    }

    inline ItemType successor(const ItemType& target) const {
        int i = index_of(target);
        for (; i < items.size() && (!items[i] || items[i] <= target); ++i);
        if (i >= items.size())
            return target;

        return items[i].value();
    }

    inline int index_of(const ItemType& target) const {
        int low = 0, high = items.size() - 1;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            for (; mid <= high && !items[mid]; ++mid);
            if (mid > high) {
                mid = low + (high - low) / 2;
                for (; mid >= low && !items[mid]; --mid);
                if (mid < low)
                    return low;
            }

            if (items[mid] < target)
                low = mid + 1;
            else if (items[mid] > target)
                high = mid - 1;
            else
                return mid;
        }

        return low == items.size() ? low - 1: low;
    }

    using const_iterator = typename std::vector<std::optional<ItemType>>::const_iterator;
    inline const_iterator begin() const { return items.begin(); }
    inline const_iterator end() const { return items.end(); }

private:
    std::vector<std::optional<ItemType>> items;

private:
    inline void scan(int begin, int end, int accum_count, int depth) {
        int curr_block_size = end - begin;
        bool is_left_child = (begin / curr_block_size) % 2 == 0;
        int sibling_begin = is_left_child ? end : begin - curr_block_size;
        int sibling_end = sibling_begin + curr_block_size;
        int sibling_count = count_items(sibling_begin, sibling_end);
        float lower, upper;
        get_thresholds(&lower, &upper, depth);
        float density = (float)(accum_count + sibling_count) / (float)(curr_block_size * 2);

        if (lower <= density && density <= upper) {
            int parent_begin = is_left_child ? begin : sibling_begin;
            int parent_end = is_left_child ? sibling_end : end;
            auto buffer = get_items(parent_begin, parent_end);
            rearrange_items(parent_begin, parent_end, buffer);
            return;
        }

        if (depth == 0) {
            auto buffer = get_items(0, items.size());
            if (density > upper)
                items.resize(items.size() * 2);
            else if (density < lower && items.size() > chunk_size * 2)
                items.resize(items.size() / 2);

            if (!buffer.empty())
                rearrange_items(0, items.size(), buffer);

            return;
        }

        int parent_begin = is_left_child ? begin : sibling_begin;
        int parent_end = is_left_child ? sibling_end : end;
        scan(parent_begin, parent_end, accum_count + sibling_count, depth - 1);
    }

    inline void rearrange_items(int begin, int end, std::vector<ItemType>& buffer) {
        int length = end - begin;
        float step = (float)length / (float)buffer.size();
        float pos = 0.0f;
        for (auto& item : buffer) {
            items[begin + (int)std::round(pos)] = std::move(item);
            pos += step;
        }
    }

    inline void get_thresholds(float* lower, float* upper, int depth) const {
        *lower = 0.5f - 0.25f * ((float)depth / tree_height());
        *upper = 0.75f + 0.25f * ((float)depth / tree_height());
    }
    inline int tree_height() const { return std::log2(items.size() / chunk_size); }

    inline std::vector<ItemType> get_items(int begin, int end) {
        std::vector<ItemType> buffer;
        for (int i = begin; i < end; ++i) {
            if (items[i]) {
                buffer.push_back(std::move(items[i].value()));
                items[i].reset();
            }
        }

        return buffer;
    }

    inline int count_items(int begin, int end) const {
        return std::count_if(items.begin() + begin, items.begin() + end, [](auto&& item) {
            return item;
        });
    }

    inline void shift_right(const int from, int to) {
        for (; to > from; --to)
            std::swap(items[to], items[to - 1]);
    }
    inline void shift_left(const int from, int till) {
        for (; till < from; ++till)
            std::swap(items[till], items[till + 1]);
    }

    inline int get_closest_gap(const int index) const {
        for (int offset = 1; ; offset++) {
            if (index + offset < items.size() && !items[index + offset])
                return index + offset;
            if (index - offset >= 0 && !items[index - offset])
                return index - offset;
        }
    }

    friend inline bool operator<(const std::optional<ItemType>& left, const std::optional<ItemType>& right) {
        return Comparator()(left.value(), right.value());
    }
    friend inline bool operator>(const std::optional<ItemType>& left, const std::optional<ItemType>& right) {
        return right < left;
    }
    friend inline bool operator==(const std::optional<ItemType>& left, const std::optional<ItemType>& right) {
        return !(right < left) && !(left < right);
    }
    friend inline bool operator!=(const std::optional<ItemType>& left, const std::optional<ItemType>& right) {
        return !(left == right);
    }
    friend inline bool operator<=(const std::optional<ItemType>& left, const std::optional<ItemType>& right) {
        return left < right || left == right;
    }
};
