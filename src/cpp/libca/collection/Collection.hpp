#pragma once
#include <vector>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>

// 包装器类，用于链式调用
template<typename T>
class ChainableVector {
public:
    ChainableVector(const std::vector<T>& vec) : vec_(vec) {}

    // 过滤操作
    ChainableVector<T> filter(std::function<bool(const T&)> predicate) {
        std::vector<T> filtered;
        std::copy_if(vec_.begin(), vec_.end(), std::back_inserter(filtered), predicate);
        return ChainableVector<T>(filtered);
    }

    // 映射操作
    template<typename U>
    ChainableVector<U> map(std::function<U(const T&)> transform) {
        std::vector<U> mapped;
        std::transform(vec_.begin(), vec_.end(), std::back_inserter(mapped), transform);
        return ChainableVector<U>(mapped);
    }

    // 收集操作，返回最终的向量
    std::vector<T> collect() {
        return vec_;
    }

private:
    std::vector<T> vec_;
};

