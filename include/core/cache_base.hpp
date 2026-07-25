#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

/*  `cache_base.hpp` : This file Implements the base class for the cache and the Node class for the DLL,
    the base class provides virtual interface which needs to be implemented by the derived classes
 */

template <typename K, typename V>
/* DLL */
class Node
{
public:
    K key;
    V value;
    Node *prev;
    Node *next;
    uint32_t expiration_time; // in seconds, 0 means no expiration
    std::chrono::steady_clock::time_point current_timestamp;
    bool is_sentinel; // True if this is a dummy head/tail node, false for actual data nodes

    // Constructor for Sentinel nodes (dummy placeholders)
    Node()
        : prev(nullptr), next(nullptr), expiration_time(0), current_timestamp(std::chrono::steady_clock::now()),
          is_sentinel(true)
    {
    }

    // Constructor for actual data nodes
    Node(K k, V v, uint32_t expiration_time = 0)
        : key(k), value(v), prev(nullptr), next(nullptr), expiration_time(expiration_time),
          current_timestamp(std::chrono::steady_clock::now()), is_sentinel(false)
    {
    }
};

template <typename K, typename V> class Cache
{
protected:
    uint32_t capacity;

public:
    Cache(uint32_t cap) : capacity(cap) {}
    virtual ~Cache() = default;
    virtual std::optional<V> get(const K &key) = 0;
    virtual std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) = 0;
    virtual bool remove(const K &key) = 0;
    virtual std::optional<K> evict() = 0;
};
