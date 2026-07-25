#pragma once
#include "cache_base.hpp"
#include "cache_expiration.hpp"
#include <array>
#include <atomic>
#include <functional>

/**
 * @file cache_manager.hpp
 * @brief Thread-safe wrappers and concurrency managers for the cache system.
 *
 * Implements SerialCache for single-threaded usage, ConcurrentCache for thread-safe multi-threaded usage,
 * and ShardedConcurrentCache which partitions keys across multiple shards to minimize lock contention.
 */

// #include <new>
// #ifdef __cpp_lib_hardware_interference_size
//     using std::hardware_destructive_interference_size;
// #else
//     constexpr std::size_t hardware_destructive_interference_size = 64;
// #endif

/**
 * @class SerialCache
 * @brief Wrapper for single-threaded execution without locking overhead.
 */
template <typename K, typename V> class SerialCache : public Cache<K, V>
{
public:
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

private:
    struct SerialManager
    {
        std::unique_ptr<Cache<K, V>> cache_policy;
    };
    SerialManager serial_manager;

    uint64_t hit_count = 0;
    uint64_t miss_count = 0;
    uint64_t eviction_count = 0;

public:
    /**
     * @brief Constructor for SerialCache.
     *
     * @param factory Callable that constructs the underlying eviction policy.
     * @param capacity The maximum capacity of the cache.
     */
    SerialCache(PolicyFactory factory, uint32_t capacity) : Cache<K, V>(capacity)
    {
        serial_manager.cache_policy = factory(capacity);
    }
    ~SerialCache() = default;

    std::optional<V> get(const K &key) override
    {
        auto res = serial_manager.cache_policy->get(key);
        if (res.has_value())
        {
            hit_count++;
        }
        else
        {
            miss_count++;
        }
        return res;
    }

    std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override
    {
        auto evicted_key = serial_manager.cache_policy->set(key, value, expiration_time);
        if (evicted_key.has_value())
        {
            eviction_count++;
        }
        return evicted_key;
    }

    bool remove(const K &key) override { return serial_manager.cache_policy->remove(key); }

    std::optional<K> evict() override
    {
        auto evicted_key = serial_manager.cache_policy->evict();
        if (evicted_key.has_value())
        {
            eviction_count++;
        }
        return evicted_key;
    }

    uint64_t get_hits() const override { return hit_count; }
    uint64_t get_misses() const override { return miss_count; }
    uint64_t get_evictions() const override { return eviction_count; }
};

/**
 * @class ConcurrentCache
 * @brief Thread-safe cache using a single reader-writer mutex (`std::shared_mutex`).
 */
template <typename K, typename V> class ConcurrentCache : public Cache<K, V>
{
private:
    struct ThreadManager
    {
        std::unique_ptr<Cache<K, V>> cache_policy;
        mutable std::shared_mutex rw_mtx;
    };
    ThreadManager threads_manager;
    CacheExpiration<K, V> expiration_manager;

    std::atomic<uint64_t> hit_count{0};
    std::atomic<uint64_t> miss_count{0};
    std::atomic<uint64_t> eviction_count{0};

public:
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

    /**
     * @brief Constructor for ConcurrentCache.
     */
    ConcurrentCache(PolicyFactory factory, uint32_t capacity) : Cache<K, V>(capacity)
    {
        threads_manager.cache_policy = factory(capacity);
        expiration_manager.start(this);
    }

    ~ConcurrentCache() { expiration_manager.stop(); }

    std::optional<V> get(const K &key) override
    {
        /* In an LRU cache, get() mutates the DLL list (moving nodes to head),
           so it requires a unique_lock rather than a shared_lock. */
        std::unique_lock<std::shared_mutex> lock(threads_manager.rw_mtx);
        auto res = threads_manager.cache_policy->get(key);
        if (res.has_value())
        {
            hit_count.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            miss_count.fetch_add(1, std::memory_order_relaxed);
        }
        return res;
    }

    std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override
    {
        std::unique_lock<std::shared_mutex> lock(threads_manager.rw_mtx);
        auto evicted_key = threads_manager.cache_policy->set(key, value, expiration_time);

        if (evicted_key.has_value())
        {
            eviction_count.fetch_add(1, std::memory_order_relaxed);
        }

        if (expiration_time > 0)
        {
            expiration_manager.add_for_expiry(key, cache_clock::now(), expiration_time);
        }
        return evicted_key;
    }

    bool remove(const K &key) override
    {
        std::unique_lock<std::shared_mutex> lock(threads_manager.rw_mtx);
        return threads_manager.cache_policy->remove(key);
    }

    std::optional<K> evict() override
    {
        std::unique_lock<std::shared_mutex> lock(threads_manager.rw_mtx);
        auto evicted_key = threads_manager.cache_policy->evict();
        if (evicted_key.has_value())
        {
            eviction_count.fetch_add(1, std::memory_order_relaxed);
        }
        return evicted_key;
    }

    uint64_t get_hits() const override { return hit_count.load(std::memory_order_relaxed); }
    uint64_t get_misses() const override { return miss_count.load(std::memory_order_relaxed); }
    uint64_t get_evictions() const override { return eviction_count.load(std::memory_order_relaxed); }
};

/**
 * @class ShardedConcurrentCache
 * @brief High-concurrency cache that shards keys across 16 sub-caches to reduce mutex contention.
 */
template <typename K, typename V> class ShardedConcurrentCache : public Cache<K, V>
{
private:
    constexpr static std::size_t CACHE_LINE_SIZE = 64;
    constexpr static std::size_t SHARD_COUNT = 16;

    /* alignas(64) ensures each Shard lies on a separate cache line, preventing false sharing
       where threads locking distinct shards compete for the same L1 cache line. */
    struct alignas(CACHE_LINE_SIZE) ShardManager
    {
        std::unique_ptr<Cache<K, V>> cache_policy;
        mutable std::shared_mutex rw_mtx;
    };

    std::array<ShardManager, SHARD_COUNT> shards_manager;
    CacheExpiration<K, V> expiration_manager;

    std::atomic<uint64_t> hit_count{0};
    std::atomic<uint64_t> miss_count{0};
    std::atomic<uint64_t> eviction_count{0};

    /**
     * @brief Computes which shard handles a given key.
     */
    size_t get_shard_index(const K &key)
    {
        std::hash<K> generic_hasher;
        return generic_hasher(key) % SHARD_COUNT;
    }

public:
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

    /**
     * @brief Constructor for ShardedConcurrentCache.
     */
    ShardedConcurrentCache(PolicyFactory factory, uint32_t capacity) : Cache<K, V>(capacity)
    {
        size_t capacity_per_shard = capacity / SHARD_COUNT;
        if (capacity % SHARD_COUNT != 0)
        {
            capacity_per_shard++;
        }

        for (uint8_t i = 0; i < SHARD_COUNT; i++)
        {
            shards_manager[i].cache_policy = factory(capacity_per_shard);
        }
        expiration_manager.start(this);
    }

    ~ShardedConcurrentCache() { expiration_manager.stop(); }

    std::optional<V> get(const K &key) override
    {
        auto &shard = shards_manager[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> lock(shard.rw_mtx);
        auto res = shard.cache_policy->get(key);
        if (res.has_value())
        {
            hit_count.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            miss_count.fetch_add(1, std::memory_order_relaxed);
        }
        return res;
    }

    std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override
    {
        auto &shard = shards_manager[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> lock(shard.rw_mtx);
        auto evicted_key = shard.cache_policy->set(key, value, expiration_time);

        if (evicted_key.has_value())
        {
            eviction_count.fetch_add(1, std::memory_order_relaxed);
        }

        if (expiration_time > 0)
        {
            expiration_manager.add_for_expiry(key, cache_clock::now(), expiration_time);
        }
        return evicted_key;
    }

    bool remove(const K &key) override
    {
        auto &shard = shards_manager[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> lock(shard.rw_mtx);
        return shard.cache_policy->remove(key);
    }

    std::optional<K> evict() override
    {
        /* Simple fallback eviction: eject an item from shard 0. */
        auto &shard = shards_manager[0];
        std::unique_lock<std::shared_mutex> lock(shard.rw_mtx);
        auto evicted_key = shard.cache_policy->evict();
        if (evicted_key.has_value())
        {
            eviction_count.fetch_add(1, std::memory_order_relaxed);
        }
        return evicted_key;
    }

    uint64_t get_hits() const override { return hit_count.load(std::memory_order_relaxed); }
    uint64_t get_misses() const override { return miss_count.load(std::memory_order_relaxed); }
    uint64_t get_evictions() const override { return eviction_count.load(std::memory_order_relaxed); }
};