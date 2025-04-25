#pragma once

#include "libca/base/Platform.hpp"
#include <type_traits>

namespace ca {

template<typename K, typename V = void>
struct RbTreeNode
{
    K                                                                 key_;
    typename std::conditional<std::is_same_v<V, void>, char, V>::type value_;
    RbTreeNode<K, V>*                                                 parent_;
    RbTreeNode<K, V>*                                                 left_;
    RbTreeNode<K, V>*                                                 right_;

    enum Color
    {
        Red,
        Black
    } color_;

    RbTreeNode(K key, V value, Color color = Red)
        : key_(key)
        , color_(color)
        , parent_(nullptr)
        , left_(nullptr)
        , right_(nullptr)
    {
        if constexpr (std::is_same_v<V, void>) {
            value_ = value;
        }
    }
};

template<typename K, typename V = void>
class RbTree
{
private:
    RbTreeNode<K, V>* root_;
    usize             size_;

public:
    RbTree()
    {
        root_ = nullptr;
        size_ = 0;
    }

    void insert(K key, V value)
    {
        if (root_ == nullptr) {
            root_ = new RbTreeNode<K, V>(key, value, RbTreeNode<K, V>::Black);
            size_++;
        }
        else {}
    }
};

template<typename K, typename V>
class HashMap
{
private:
    RbTree<K, V>* root_;

public:
    HashMap() {}

    void put(K key, V value)
    {
        if (root_ == nullptr) {
            root_ = new RbTreeNode<K, V>(key, value, RbTreeNode<K, V>::Color::Black);
        }
        else {
            root_->insert(key, value);
        }
    }

    void remove(K key) {}

    void get(K key) {}

    bool contains(K key) { return true; }

    
};

template<typename T>
class HashSet
{
private:
    RbTree<T>* root_;

public:
};



}   // namespace ca
