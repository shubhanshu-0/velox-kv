#pragma once
#include "cache_base.hpp"
#include "cache_expiration.hpp"
#include <array>
#include <functional>

/*
    `cache_manager.hpp` : This file implements the thread-safe cache manager,
    it uses a shared_mutex to provide thread safety
    Integrated with CacheExpiration for background TTL eviction.
*/

// #include <new>
// #ifdef __cpp_lib_hardware_interference_size
//     using std::hardware_destructive_interference_size;
// #else
//     constexpr std::size_t hardware_destructive_interference_size = 64;
// #endif

template <typename K, typename V> class SerialCache : public Cache<K, V>
{
    // Creating the underlying cache policy (LRU, LFU, etc.)
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

private:
    struct SerialManager
    {
        std::unique_ptr<Cache<K, V>> cache_policy;
    };
    SerialManager serial_manager;

public:
    // By taking a Factory instead of a physical object, the manager can decide
    // how and when to initialize the underlying policy.
    SerialCache(PolicyFactory factory, uint32_t capacity) : Cache<K, V>(capacity)
    {
        serial_manager.cache_policy = factory(capacity);
    }
    ~SerialCache() = default;

    // Normal get() without locks for single-threaded use.
    std::optional<V> get(const K &key) override { return serial_manager.cache_policy->get(key); }

    std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override
    {
        return serial_manager.cache_policy->set(key, value, expiration_time);
    }

    bool remove(const K &key) override { return serial_manager.cache_policy->remove(key); }

    // Not usually used directly, used when cache storage is full and we need to evict something to make room for a new
    // key.
    std::optional<K> evict() override { return serial_manager.cache_policy->evict(); }
};

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

public:
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

    ConcurrentCache(PolicyFactory factory, uint32_t capacity) : Cache<K, V>(capacity)
    {
        threads_manager.cache_policy = factory(capacity);
        expiration_manager.start(this);
    }

    ~ConcurrentCache() { expiration_manager.stop(); }

    std::optional<V> get(const K &key) override
    {
        /* NOTE: In an LRU Cache, get() is NOT a true "read-only" operation.
           Fetching a key moves it to the "Most Recently Used" position in the DLL.
           Therefore, we use a unique_lock (Write Lock) instead of a shared_lock.
        */
        std::unique_lock<std::shared_mutex> lock(threads_manager.rw_mtx);
        return threads_manager.cache_policy->get(key);
    }

    std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override
    {
        std::unique_lock<std::shared_mutex> lock(threads_manager.rw_mtx);
        auto evicted_key = threads_manager.cache_policy->set(key, value, expiration_time);

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

    // Not usually used directly, used when cache storage is full and we need to evict something to make room for a new
    // key.
    std::optional<K> evict() override
    {
        std::unique_lock<std::shared_mutex> lock(threads_manager.rw_mtx);
        return threads_manager.cache_policy->evict();
    }
};

template <typename K, typename V> class ShardedConcurrentCache : public Cache<K, V>
{
private:
    // Standard cache line size for modern CPUs
    constexpr static std::size_t CACHE_LINE_SIZE = 64;
    constexpr static std::size_t SHARD_COUNT = 16;

    /* alignas(64) prevents "False Sharing":
       It ensures each Shard sits on its own CPU Cache Line. Without this,
       multiple threads locking different shards could still slow each other down
       if those shards were packed too closely together in memory.
    */
    struct alignas(CACHE_LINE_SIZE) ShardManager
    {
        std::unique_ptr<Cache<K, V>> cache_policy;
        mutable std::shared_mutex rw_mtx;
    };

    std::array<ShardManager, SHARD_COUNT> shards_manager;
    CacheExpiration<K, V> expiration_manager;

    size_t get_shard_index(const K &key)
    {
        std::hash<K> generic_hasher;
        return generic_hasher(key) % SHARD_COUNT;
    }

public:
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

    ShardedConcurrentCache(PolicyFactory factory, uint32_t capacity) : Cache<K, V>(capacity)
    {
        // Divide the total capacity across all shards.
        size_t capacity_per_shard = capacity / SHARD_COUNT;
        if (capacity % SHARD_COUNT != 0)
            capacity_per_shard++;

        /* We initialize each shard in our contiguous array using the factory.
           Since std::array is fixed-size, these Shards never move in memory.
        */
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
        return shard.cache_policy->get(key);
    }

    std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override
    {
        auto &shard = shards_manager[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> lock(shard.rw_mtx);
        auto evicted_key = shard.cache_policy->set(key, value, expiration_time);

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

    // Not usually used directly, used when cache storage is full and we need to evict something to make room for a new
    // key.

    std::optional<K> evict() override
    {
        // Simple eviction strategy: evict from shard 0 for now.
        auto &shard = shards_manager[0];
        std::unique_lock<std::shared_mutex> lock(shard.rw_mtx);
        return shard.cache_policy->evict();
    }
};