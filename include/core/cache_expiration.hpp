#pragma once
#include "cache_base.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>

/**
 * @file cache_expiration.hpp
 * @brief Manages TTL-based key eviction using a background thread.
 *
 * Keeps track of when keys are scheduled to expire using a sorted std::set of time points.
 * A background thread sleeps until the next expiration occurs, avoiding a busy loop and CPU usage.
 */

using cache_clock = std::chrono::steady_clock;

template <typename K, typename V> class CacheExpiration
{
private:
    std::set<std::pair<cache_clock::time_point, K>> expiring_elements;
    std::mutex rw_mutex;
    std::condition_variable expire_cv;

    std::thread worker;
    bool stop_flag = false; // Flag to signal the background worker thread to stop.

    /**
     * @brief The main loop executed by the background thread.
     *
     * @param cache Pointer to the cache to perform evictions on.
     */
    void thread_loop(Cache<K, V> *cache)
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(rw_mutex);

            /* Wait until there is a key to track, or we are instructed to stop. */
            expire_cv.wait(lock, [this] { return stop_flag || !expiring_elements.empty(); });

            if (stop_flag && expiring_elements.empty())
                return; // Exit thread loop for clean shutdown.

            /* Retrieve the item that will expire the soonest. */
            auto earliest = *expiring_elements.begin();

            if (cache_clock::now() >= earliest.first)
            {
                K key_to_remove = earliest.second;
                expiring_elements.erase(expiring_elements.begin());

                /* Unlock the mutex before deleting from the cache to prevent deadlocks.
                   This prevents conflicts if another thread holds the cache lock and
                   attempts to add a new expiration tracker (which locks rw_mutex). */
                lock.unlock();
                cache->remove(key_to_remove);
            }
            else
            {
                /* Sleep until the next key is scheduled to expire. We will wake up early
                   if a new key with an even sooner expiration is added. */
                expire_cv.wait_until(lock, earliest.first);
            }
        }
    }

public:
    /**
     * @brief Constructor for the Expiration Manager.
     */
    CacheExpiration() = default;

    /**
     * @brief Destructor that ensures the background thread exits cleanly.
     */
    ~CacheExpiration() { stop(); }

    CacheExpiration(const CacheExpiration &) = delete;
    CacheExpiration &operator=(const CacheExpiration &) = delete;

    /**
     * @brief Spawns the background helper thread.
     *
     * @param cache The cache structure to manage.
     */
    void start(Cache<K, V> *cache) { worker = std::thread(&CacheExpiration::thread_loop, this, cache); }

    /**
     * @brief Gracefully terminates the background thread.
     */
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(rw_mutex);
            stop_flag = true;
        }
        expire_cv.notify_all(); // Wake the thread so it sees the stop flag.

        if (worker.joinable())
            worker.join();
    }

    /**
     * @brief Schedules a key for deletion after a given time-to-live.
     *
     * @param key The key to monitor.
     * @param timestamp_created The creation baseline time.
     * @param expiration_time_seconds Lifetime of the key in seconds.
     */
    void add_for_expiry(const K &key, cache_clock::time_point timestamp_created, uint32_t expiration_time_seconds)
    {
        auto expiry_point = timestamp_created + std::chrono::seconds(expiration_time_seconds);

        std::unique_lock<std::mutex> lock(rw_mutex);
        auto [it, inserted] = expiring_elements.insert({expiry_point, key});

        /* If this key is now the next one slated to expire, wake the thread to update its sleep time. */
        if (inserted && it == expiring_elements.begin())
            expire_cv.notify_one();
    }

    /**
     * @brief Cancels the scheduled expiration tracking for a key.
     *
     * @param key The key to untrack.
     * @param expiry_point The scheduled expiration time point.
     */
    void remove_from_expiry(const K &key, cache_clock::time_point expiry_point)
    {
        std::lock_guard<std::mutex> lock(rw_mutex);
        expiring_elements.erase({expiry_point, key});
    }
};