#pragma once
#include "cache_base.hpp"
#include <chrono>
#include <mutex>
#include <set>

/**
 * @file cache_expiration.hpp
 * @brief Threadless TTL-based key expiration tracking.
 *
 * Keeps track of key expiration times using a sorted std::set.
 * Expiration is triggered passively on GET operations, and actively via
 * non-blocking purge_expired() calls from the server event loop.
 */

using cache_clock = std::chrono::steady_clock;

template <typename K, typename V> class CacheExpiration
{
private:
    std::set<std::pair<cache_clock::time_point, K>> expiring_elements;
    std::mutex rw_mutex;

public:
    CacheExpiration() = default;
    ~CacheExpiration() = default;

    CacheExpiration(const CacheExpiration &) = delete;
    CacheExpiration &operator=(const CacheExpiration &) = delete;

    /**
     * @brief Compatibility methods (no background threads needed).
     */
    void start(Cache<K, V> * /*cache*/) {}
    void stop() {}

    /**
     * @brief Schedules a key for deletion after a given time-to-live.
     */
    void add_for_expiry(const K &key, cache_clock::time_point timestamp_created, uint32_t expiration_time_seconds)
    {
        auto expiry_point = timestamp_created + std::chrono::seconds(expiration_time_seconds);
        std::lock_guard<std::mutex> lock(rw_mutex);

        /* Remove any existing expiration for this key first to avoid stale TTL eviction. */
        for (auto it = expiring_elements.begin(); it != expiring_elements.end();)
        {
            if (it->second == key)
            {
                it = expiring_elements.erase(it);
            }
            else
            {
                ++it;
            }
        }
        expiring_elements.insert({expiry_point, key});
    }

    /**
     * @brief Cancels the scheduled expiration tracking for a key.
     */
    void remove_from_expiry(const K &key)
    {
        std::lock_guard<std::mutex> lock(rw_mutex);
        for (auto it = expiring_elements.begin(); it != expiring_elements.end();)
        {
            if (it->second == key)
            {
                it = expiring_elements.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    /**
     * @brief Purges up to max_purge expired items. Non-blocking.
     */
    size_t purge_expired(Cache<K, V> *cache, size_t max_purge = 100)
    {
        std::lock_guard<std::mutex> lock(rw_mutex);
        auto now = cache_clock::now();
        size_t purged = 0;

        while (!expiring_elements.empty() && purged < max_purge)
        {
            auto it = expiring_elements.begin();
            if (now >= it->first)
            {
                K key_to_remove = it->second;
                expiring_elements.erase(it);
                cache->remove(key_to_remove);
                purged++;
            }
            else
            {
                break; // Earliest item has not expired yet.
            }
        }
        return purged;
    }
};