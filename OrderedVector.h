#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <ostream>
#include <vector>

template <typename T, typename Comparator = std::less<T>, uint32_t block_size = 8>
class OrderedVector {
public:
    inline OrderedVector() : items(block_size * 2) {}

    inline void push(const T& t) {
        int i = index_of(t);
        if (items[i]) {
            if (bigger_than(t, items[i].value()))
                shifted_left(i) || (items[++i] && shifted_right(i));
            else
                shifted_right(i) || (items[--i] && shifted_left(i));
        }
        items[i] = t;

        int block_begin = (i / block_size) * block_size;
        int block_end = block_begin + block_size;
        scan(block_begin, block_end, tree_height());
    }

    inline void remove(const T& t) {
        int i = index_of(t);
        if (items[i] && !equal(items[i].value(), t))
            return;

        items[i].reset();
        int block_begin = (i / block_size) * block_size;
        int block_end = block_begin + block_size;
        scan(block_begin, block_end, tree_height());
    }

    inline const T& successor(const T& t) const {
        int i = index_of(t);
        for (;i < items.size() && (!items[i] || less_than_equal(items[i].value(), t)); ++i);
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

            if (less_than(items[mid].value(), t))
                low = mid + 1;
            else if (bigger_than(items[mid].value(), t))
                high = mid - 1;
            else
                return mid;
        }

        return low >= items.size() ? low - 1: low;
    }

    friend inline std::ostream& operator<<(std::ostream& os, const OrderedVector& ov) {
        for (const auto& item : ov.items) {
            if (item)
                os << item.value() << " ";
        }
        return os;
    }

public:
    std::vector<std::optional<T>> items;

private:
    inline void scan(int begin, int end, int depth) {
        float count = count_items(begin, end);
        float lower, upper;
        get_thresholds(&lower, &upper, depth);
        float density = count / (float)(end - begin);

        if (depth == 0) {
            if (density >= upper)
                items.resize(items.size() * 2);

            if (count > 0)
                rearrange_items(begin, end);

            return;
        }

        if (lower <= density && density < upper) {
            if (depth != tree_height())
                rearrange_items(begin, end);

            return;
        }

        int depth_block_size = items.size() / std::pow(2, depth);
        bool left_child = (begin / depth_block_size) % 2 == 0;
        int block_begin = left_child ? begin : begin - depth_block_size;
        int block_end = left_child ? end + depth_block_size : end;
        scan(block_begin, block_end, depth - 1);
    }

    inline void rearrange_items(int begin, int end) {
        std::vector<std::optional<T>> buffer;
        for (int i = begin; i < end; ++i) {
            if (items[i]) {
                buffer.push_back(std::move(items[i]));
                items[i] = std::nullopt;
            }
        }

        int length = end - begin;
        int new_density = (length + buffer.size() - 1) / buffer.size();
        int remainder = buffer.size() - length / new_density;
        for (int i = begin, b_iter = 0; i < end && b_iter < buffer.size(); i += new_density) {
            items[i] = std::move(buffer[b_iter++]);
            if (remainder > 0) {
                items[i + 1] = std::move(buffer[b_iter++]);
                --remainder;
            }
        }
    }

    inline int tree_height() const { return std::log2(items.size() / block_size); }

    inline int count_items(int begin, int end) const {
        return std::count_if(items.begin() + begin, items.begin() + end, [](auto&& item) {
            return item;
        });
    }

    inline bool shifted_right(int index) {
        int i;
        for (i = index; i < items.size() && items[i]; ++i);
        if (i >= items.size())
            return false;

        for (; i > index; --i)
            std::swap(items[i], items[i - 1]);

        return true;
    }

    inline bool shifted_left(int index) {
        int i;
        for (i = index; i >= 0 && items[i]; --i);
        if (i < 0)
            return false;

        for (; i < index; ++i)
            std::swap(items[i], items[i + 1]);

        return true;
    }

    inline void get_thresholds(float* lower, float* upper, int depth) const {
        *lower = 0.5f - 0.25f * ((float)depth / tree_height());
        *upper = 0.75f + 0.25f * ((float)depth / tree_height());
    }

    inline bool less_than(const T& a, const T& b) const {
        return Comparator()(a, b);
    }

    inline bool less_than_equal(const T& a, const T& b) const {
        return less_than(a, b) || equal(a, b);
    }

    inline bool bigger_than(const T& a, const T& b) const {
        return less_than(b, a);
    }

    inline bool equal(const T& a, const T& b) const {
        return !less_than(a, b) && !bigger_than(a, b);
    }
};

