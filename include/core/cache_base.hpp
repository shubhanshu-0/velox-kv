#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <string>
#include <optional>

template <typename K, typename V>
/* DLL */
class Node
{
public:
    K key;
    V value;
    Node *prev;
    Node *next;
    bool is_sentinel;

    Node() : prev(nullptr), next(nullptr), is_sentinel(true) {}
    
    Node(K k, V v) : key(k), value(v), prev(nullptr), next(nullptr), is_sentinel(false) {}
};

template <typename K, typename V>
class Cache_Operations
{
    public:
        uint32_t capacity;
        Cache_Operations(uint32_t cap) : capacity(cap) {}
        virtual ~Cache_Operations() = default;
        virtual std::optional<V> get(K key) = 0;
        virtual bool set(K key, V value) = 0;
        virtual bool remove(K key) = 0;
};