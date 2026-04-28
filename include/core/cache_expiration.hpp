#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>
#include "cache_base.hpp"

/*  `cache_expiration.hpp` : Manages TTL-based eviction for any Cache<K,V>.
    - add_for_expiry()  : registers a key with an absolute expiration time_point.
    - start()           : spawns the background eviction thread (call after cache is ready).
    - stop()            : signals and joins the thread gracefully (also called by destructor).

    Design notes:
    - std::set<pair<time_point, K>> keeps elements sorted by expiry time, so
      the "next to expire" element is always at begin() in O(1).
    - The background thread sleeps precisely until the next expiry rather than
      polling in a hot loop, so it uses zero CPU while idle.
*/

using cache_clock = std::chrono::steady_clock;

template <typename K, typename V>
class CacheExpiration
{
private:
    std::set<std::pair<cache_clock::time_point, K>> expiring_elements;
    std::mutex rw_mutex;
    std::condition_variable expire_cv;

    std::thread worker;
    bool stop_flag = false;   // signals the background thread to exit

    /**
     * @brief thread_loop() — The actual loop run by the background thread. 
     * Receives the cache by pointer so we don't have to store it as a member
     * before start() is called.
     * @param cache : The cache to evict from.
     */
    void thread_loop(Cache<K, V> *cache)
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(rw_mutex);

            // Sleep indefinitely if nothing is registered yet (or we've already
            // processed everything). Also exits cleanly when stop_flag is set.
            expire_cv.wait(lock, [this]{
                return stop_flag || !expiring_elements.empty();
            });

            if (stop_flag && expiring_elements.empty())
                return;   // clean shutdown

            // Peek at the element that expires soonest.
            auto earliest = *expiring_elements.begin();

            if (cache_clock::now() >= earliest.first)
            {
                // It has expired — evict it.
                K key_to_remove = earliest.second;
                expiring_elements.erase(expiring_elements.begin());

                // Unlock BEFORE calling cache->remove() to avoid a deadlock:
                // cache->remove() will try to acquire the cache's own mutex,
                // and another thread inside set() holds the cache mutex while
                // trying to call add_for_expiry() which needs rw_mutex.
                lock.unlock();
                cache->remove(key_to_remove);
            }
            else
            {
                // Not yet expired — sleep exactly until its expiry time.
                // wait_until will also wake up early if notify_one() is called
                // (e.g., a new earlier-expiring element was just added, or stop()).
                expire_cv.wait_until(lock, earliest.first);
            }
        }
    }

public:
    /**
     * @brief Constructor — initialises data structures but does NOT start the thread.
     * Call start(cache) once your cache object is fully constructed.
     */
    CacheExpiration() = default;

    /**
     * @brief Destructor — always stops the background thread first.
     */
    ~CacheExpiration()
    {
        stop();
    }

    // Non-copyable — owning a thread means this makes no sense to copy.
    CacheExpiration(const CacheExpiration &)            = delete;
    CacheExpiration &operator=(const CacheExpiration &) = delete;

    /**
     * @brief start() — spawns the background eviction thread.
     * @param cache : Pass the cache by pointer (not reference) so the thread can be stored.
     * Call this exactly once after the cache is fully initialized.
     */
    void start(Cache<K, V> *cache)
    {
        worker = std::thread(&CacheExpiration::thread_loop, this, cache);
    }

    /**
     * @brief stop() — signals the thread to exit and waits for it to finish.
     * Safe to call multiple times.
     */
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(rw_mutex);
            stop_flag = true;
        }
        expire_cv.notify_all();    // wake thread so it can see stop_flag

        if (worker.joinable())
            worker.join();
    }

    /**
     * @brief add_for_expiry() — register a key with a TTL in seconds.
     * @param key : The key to register.
     * @param timestamp_created : The time the key was inserted.
     * @param expiration_time_seconds : The time to live in seconds.
     */
    void add_for_expiry(const K &key,
                        cache_clock::time_point timestamp_created,
                        uint32_t expiration_time_seconds)
    {
        auto expiry_point = timestamp_created
                          + std::chrono::seconds(expiration_time_seconds);

        std::unique_lock<std::mutex> lock(rw_mutex);
        auto [it, inserted] = // iterator, bool
            expiring_elements.insert({expiry_point, key});

        // If this new element is now the earliest to expire, the background
        // thread may be sleeping too long. Wake it up to recalculate.
        if (inserted && it == expiring_elements.begin())
            expire_cv.notify_one();
    }

    /**
     * @brief remove_from_expiry() — de-register a key on manual cache eviction.
     * @param key : The key to remove.
     * @param expiry_point : The expiry point of the key.
     */
    void remove_from_expiry(const K &key,
                            cache_clock::time_point expiry_point)
    {
        std::lock_guard<std::mutex> lock(rw_mutex);
        expiring_elements.erase({expiry_point, key});
    }
};