#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <ostream>
#include <vector>

template <typename T, typename Comparator = std::less<T>, uint32_t chunk_size = 8>
class packed_memory_array {
public:
    inline packed_memory_array() : items(chunk_size * 2) {}

    inline void push(const T& t) {
        int i = index_of(t);
        int block_begin = (i / chunk_size) * chunk_size;
        int block_end = block_begin + chunk_size;
        int count = count_items(block_begin, block_end) + 1;
        float lower, upper;
        get_thresholds(&lower, &upper, tree_height());
        float density = (float)count / (float)(block_end - block_begin);
        if (density > upper) {
            scan(block_begin, block_end, count, tree_height() - 1);
            i = index_of(t);
        }

        insert(t, i);
    }

    inline void remove(const T& t) {
        int i = index_of(t);
        if (!items[i] || !equal(items[i].value(), t))
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

    inline const T& successor(const T& t) const {
        int i = index_of(t);
        for (; i < items.size() && (!items[i] || less_equal(items[i].value(), t)); ++i);
        if (i >= items.size())
            return t;

        return items[i].value();
    }

    inline int index_of(const T& t) const {
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

            if (less(items[mid].value(), t))
                low = mid + 1;
            else if (greater(items[mid].value(), t))
                high = mid - 1;
            else
                return mid;
        }

        return low == items.size() ? low - 1: low;
    }

    using const_iterator = typename std::vector<std::optional<T>>::const_iterator;
    inline const_iterator begin() const { return items.begin(); }
    inline const_iterator end() const { return items.end(); }

private:
    std::vector<std::optional<T>> items;

private:
    inline void scan(int begin, int end, int accum_count, int depth) {
        int depth_block_size = end - begin;
        bool is_left = (begin / depth_block_size) % 2 == 0;
        int sibling_begin = is_left ? end : begin - depth_block_size;
        int sibling_end = sibling_begin + depth_block_size;
        int sibling_count = count_items(sibling_begin, sibling_end);
        float lower, upper;
        get_thresholds(&lower, &upper, depth);
        float density = (float)(accum_count + sibling_count) / (float)(depth_block_size * 2);

        if (lower <= density && density <= upper) {
            int parent_begin = is_left ? begin : sibling_begin;
            int parent_end = is_left ? sibling_end : end;
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

        int parent_begin = is_left ? begin : sibling_begin;
        int parent_end = is_left ? sibling_end : end;
        scan(parent_begin, parent_end, accum_count + sibling_count, depth - 1);
    }

    inline void rearrange_items(int begin, int end, std::vector<T>& buffer) {
        int k = end - begin;
        int n = buffer.size();
        float step = (float)k / (float)n;

        float pos = 0.0f;
        for (auto& item : buffer) {
            items[begin + (int)std::round(pos)] = std::move(item);
            pos += step;
        }
    }

    inline std::vector<T> get_items(int begin, int end) {
        std::vector<T> buffer;
        buffer.reserve(end - begin);
        for (int i = begin; i < end; ++i) {
            if (items[i]) {
                buffer.push_back(std::move(items[i].value()));
                items[i].reset();
            }
        }

        return buffer;
    }

    inline int tree_height() const { return std::log2(items.size() / chunk_size); }

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

    inline void insert(const T& t, int i) {
        if (items[i]) {
            if (greater(t, items[i].value()) && i + 1 < items.size() && !items[i + 1]) {
                i++;
            } else if (less(t, items[i].value()) && i - 1 >= 0 && !items[i - 1]) {
                i--;
            } else {
                bool on_right;
                int closest_gap_index;
                closest_gap(&closest_gap_index, &on_right, i);
                if ((on_right && greater(t, items[i].value())) ||
                    (!on_right && less(t, items[i].value())))
                {
                    i += on_right ? 1 : -1;
                }

                if (on_right)
                    shift_right(i, closest_gap_index);
                else
                    shift_left(i, closest_gap_index);
            }
        }
        items[i] = t;
    }

    inline void closest_gap(int* closest_gap, bool* on_right, const int index) const {
        int closest_right = index + 1;
        for (; closest_right < items.size() && items[closest_right]; ++closest_right);
        int closest_left = index - 1;
        for (; closest_left >= 0 && items[closest_left]; --closest_left);

        if (closest_left < 0) {
            *closest_gap = closest_right;
            *on_right = true;
        } else if (closest_right >= items.size()) {
            *closest_gap = closest_left;
            *on_right = false;
        } else {
            *on_right = closest_right - index <= index - closest_left;
            *closest_gap = *on_right ? closest_right : closest_left;
        }
    }

    inline void get_thresholds(float* lower, float* upper, int depth) const {
        *lower = 0.5f - 0.25f * ((float)depth / tree_height());
        *upper = 0.75f + 0.25f * ((float)depth / tree_height());
    }

    inline bool less(const T& a, const T& b) const {
        return Comparator()(a, b);
    }
    inline bool greater(const T& a, const T& b) const {
        return less(b, a);
    }
    inline bool equal(const T& a, const T& b) const {
        return !less(a, b) && !greater(a, b);
    }
    inline bool less_equal(const T& a, const T& b) const {
        return less(a, b) || !greater(a, b);
    }
};
