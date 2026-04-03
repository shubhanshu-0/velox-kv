#pragma once
#include "../include/core/cache_base.hpp"
#include "../include/policies/lru.hpp"

/*  `cache_manager.hpp` : This file implements the thread-safe cache manager, 
    it uses a shared_mutex to provide thread safety
*/

constexpr uint32_t SHARD_COUNT = 16;

template<typename K, typename V>
class SerialCache : public Cache<K, V>
{
    private:
        std::unique_ptr<Cache<K, V>> cache_policy;
    public:
        SerialCache(std::unique_ptr<Cache<K, V>> policy) 
            : Cache<K, V>(policy->capacity), cache_policy(std::move(policy)) {}
        ~SerialCache() = default;
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
    ConcurrentCache(std::unique_ptr<Cache<K, V>> policy) 
        : Cache<K, V>(policy->capacity), cache_policy(std::move(policy)) {}
    ~ConcurrentCache() = default;
    std::optional<V> get(const K& key) override
    {
        // unique lock since get() also modifies the cache (DLL pointers)
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
    struct Shard {
        std::unique_ptr<Cache<K, V>> cache_policy;
        mutable std::shared_mutex rw_mtx;
    };

    std::vector<std::unique_ptr<Shard>> shards;
    uint32_t total_capacity;
    size_t get_shard_index(const K &key){
        std::hash<K> generic_hasher;
        return generic_hasher(key) % SHARD_COUNT;
    }
    
public:
    ShardedConcurrentCache(std::unique_ptr<Cache<K,V>> cache_policy, uint32_t capacity) 
        : Cache<K,V>(capacity), total_capacity(capacity)
    {
            size_t capacity_per_shard = total_capacity / SHARD_COUNT 
                                            + (SHARD_COUNT - total_capacity % SHARD_COUNT);

            /* Creating `SHARD_COUNT` shards with the same policy */
            for(uint8_t i = 0; i < SHARD_COUNT; i++){
                auto shard_ = std::make_unique<Shard>();
                cache_policy->capacity = capacity_per_shard;
                shard_->cache_policy = std::move(cache_policy);
                shards.push_back(std::move(shard_));
            }
    }

    ~ShardedConcurrentCache() = default;

    std::optional<V> get(const K& key) override
    {
        auto &shard_ = shards[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> read_lock(shard_->rw_mtx);
        return shard_->cache_policy->get(key);
    }

    bool set(const K& key, const V& value) override
    {
        auto &shard_ = shards[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> write_lock(shard_->rw_mtx);
        return shard_->cache_policy->set(key, value);
    }

    bool remove(const K& key) override
    {
        auto &shard_ = shards[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> write_lock(shard_->rw_mtx);
        return shard_->cache_policy->remove(key);
    }
};