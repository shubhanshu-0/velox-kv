#pragma once
#include <array>
#include <functional>
#include "cache_base.hpp"

/*  
    `cache_manager.hpp` : This file implements the thread-safe cache manager, 
    it uses a shared_mutex to provide thread safety
*/


// #include <new>
// #ifdef __cpp_lib_hardware_interference_size
//     using std::hardware_destructive_interference_size;
// #else
//     constexpr std::size_t hardware_destructive_interference_size = 64;
// #endif

// Standard cache line size for modern CPUs
constexpr std::size_t CACHE_LINE_SIZE = 64;

constexpr std::size_t SHARD_COUNT = 16;

template<typename K, typename V>
class SerialCache : public Cache<K, V>
{
    // Creating the underlying cache policy (LRU, LFU, etc.)
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

    private:
        std::unique_ptr<Cache<K, V>> cache_policy;
    public:
        // By taking a Factory instead of a physical object, the manager can decide
        // how and when to initialize the underlying policy.
        SerialCache(PolicyFactory factory, uint32_t capacity) 
            : Cache<K, V>(capacity), cache_policy(factory(capacity)) {}
        ~SerialCache() = default;

        // Normal get() without locks for single-threaded use.
        std::optional<V> get(const K& key) override
        {
            return cache_policy->get(key);
        }
        
        bool set(const K& key, const V& value) override
        {
            return cache_policy->set(key, value);
        }

        bool remove(const K& key) override
        {
            return cache_policy->remove(key);
        }
};

template <typename K, typename V> 
class ConcurrentCache : public Cache<K, V>
{
private:
    // std::vector<std::unique_ptr<Cache<K, V>>> shards;
    std::unique_ptr<Cache<K, V>> cache_policy;
    // std::mutex mtx;
    mutable std::shared_mutex rw_mtx;

public:
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

    ConcurrentCache(PolicyFactory factory, uint32_t capacity) 
        : Cache<K, V>(capacity), cache_policy(factory(capacity)) {}
    ~ConcurrentCache() = default;

    std::optional<V> get(const K& key) override
    {
        /* NOTE: In an LRU Cache, get() is NOT a true "read-only" operation.
           Fetching a key moves it to the "Most Recently Used" position in the DLL.
           Therefore, we use a unique_lock (Write Lock) instead of a shared_lock.
        */
        std::unique_lock<std::shared_mutex> read_lock(rw_mtx);
        return cache_policy->get(key);
    }

    bool set(const K& key, const V& value) override
    {
        std::unique_lock<std::shared_mutex> write_lock(rw_mtx);
        return cache_policy->set(key, value);
    }

    bool remove(const K& key) override
    {
        std::unique_lock<std::shared_mutex> write_lock(rw_mtx);
        return cache_policy->remove(key);
    }
};


template <typename K, typename V> 
class ShardedConcurrentCache : public Cache<K, V>
{
private:
    /* alignas(64) prevents "False Sharing":
       It ensures each Shard sits on its own CPU Cache Line. Without this,
       multiple threads locking different shards could still slow each other down
       if those shards were packed too closely together in memory.
    */
    struct alignas(CACHE_LINE_SIZE) Shard {
        std::unique_ptr<Cache<K, V>> cache_policy;
        mutable std::shared_mutex rw_mtx;
    };

    std::array<Shard, SHARD_COUNT> shards;
    uint32_t total_capacity;
    size_t get_shard_index(const K &key){
        std::hash<K> generic_hasher;
        return generic_hasher(key) % SHARD_COUNT;
    }
    
public:
    using PolicyFactory = std::function<std::unique_ptr<Cache<K, V>>(uint32_t)>;

    ShardedConcurrentCache(PolicyFactory factory, uint32_t capacity) 
        : Cache<K,V>(capacity), total_capacity(capacity)
    {
            // Divide the total capacity across all shards.
            size_t capacity_per_shard = total_capacity / SHARD_COUNT;
            if (total_capacity % SHARD_COUNT != 0) capacity_per_shard++;

            /* We initialize each shard in our contiguous array using the factory.
               Since std::array is fixed-size, these Shards never move in memory.
            */
            for(uint8_t i = 0; i < SHARD_COUNT; i++){
                shards[i].cache_policy = factory(capacity_per_shard);
            }
    }

    ~ShardedConcurrentCache() = default;

    std::optional<V> get(const K& key) override
    {
        auto &shard_ = shards[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> read_lock(shard_.rw_mtx);
        return shard_.cache_policy->get(key);
    }

    bool set(const K& key, const V& value) override
    {
        auto &shard_ = shards[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> write_lock(shard_.rw_mtx);
        return shard_.cache_policy->set(key, value);
    }

    bool remove(const K& key) override
    {
        auto &shard_ = shards[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> write_lock(shard_.rw_mtx);
        return shard_.cache_policy->remove(key);
    }
};