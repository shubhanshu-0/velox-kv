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

/**
 * @file cache_base.hpp
 * @brief Base interfaces and data structures for the caching system.
 *
 * Implements the base class for the cache and the Node class for the doubly linked list.
 * The Cache class provides a virtual interface that is implemented by concrete policy classes.
 */

template <typename K, typename V> class Node
{
public:
    K key;
    V value;
    Node *prev;
    Node *next;
    uint32_t expiration_time; // Time-to-live in seconds. 0 indicates no expiration.
    std::chrono::steady_clock::time_point current_timestamp;
    bool is_sentinel; // Indicates whether this is a dummy sentinel node.

    /**
     * @brief Constructor for sentinel (dummy) nodes.
     *
     * Initializes a node that acts as a placeholder at the boundary of a doubly linked list,
     * ensuring no null pointer checks are needed in list traversal operations.
     */
    Node()
        : prev(nullptr), next(nullptr), expiration_time(0), current_timestamp(std::chrono::steady_clock::now()),
          is_sentinel(true)
    {
    }

    /**
     * @brief Constructor for actual data-carrying nodes.
     *
     * @param k The key identifier.
     * @param v The associated value.
     * @param expiration_time The duration (in seconds) before this node is considered expired.
     */
    Node(K k, V v, uint32_t expiration_time = 0)
        : key(k), value(v), prev(nullptr), next(nullptr), expiration_time(expiration_time),
          current_timestamp(std::chrono::steady_clock::now()), is_sentinel(false)
    {
    }
};

/**
 * @class Cache
 * @brief Abstract base class defining the standard interface for all cache implementations.
 */
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

    virtual uint64_t get_hits() const = 0;
    virtual uint64_t get_misses() const = 0;
    virtual uint64_t get_evictions() const = 0;
};
