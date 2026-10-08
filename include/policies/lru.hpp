#pragma once
#include "../core/cache_base.hpp"
#include "../core/cache_expiration.hpp"

/**
 * @file lru.hpp
 * @brief High-performance single-layer Least Recently Used (LRU) cache engine.
 *
 * Implements standard LRU cache eviction using a doubly linked list (for tracking recency)
 * and an unordered_map (for O(1) lookups), with integrated TTL tracking and metrics.
 */

template <typename K, typename V> void remove_from_dll(Node<K, V> *node)
{
    Node<K, V> *next_node = node->next;
    Node<K, V> *prev_node = node->prev;
    next_node->prev = prev_node;
    prev_node->next = next_node;
}

template <typename K, typename V> void add_to_dll(Node<K, V> *node, Node<K, V> *head)
{
    Node<K, V> *next_node = head->next;
    head->next = node;
    node->prev = head;
    node->next = next_node;
    next_node->prev = node;
}

template <typename K, typename V> class LRUCache : public Cache<K, V>
{
private:
    std::unordered_map<K, std::shared_ptr<Node<K, V>>> cache_map;
    std::shared_ptr<Node<K, V>> head, tail;
    CacheExpiration<K, V> expiration_manager;

    uint64_t hit_count = 0;
    uint64_t miss_count = 0;
    uint64_t eviction_count = 0;

public:
    LRUCache(uint32_t capacity) : Cache<K, V>(capacity)
    {
        head = std::make_shared<Node<K, V>>();
        tail = std::make_shared<Node<K, V>>();
        head->next = tail.get();
        tail->prev = head.get();
    }
    ~LRUCache() = default;

    std::optional<V> get(const K &key) override
    {
        auto it = cache_map.find(key);
        if (it == cache_map.end())
        {
            miss_count++;
            return std::nullopt;
        }

        Node<K, V> *ref_node = it->second.get();

        /* Lazy TTL eviction check */
        if (ref_node->expiration_time > 0)
        {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - ref_node->current_timestamp).count();
            if (diff >= ref_node->expiration_time)
            {
                remove(key);
                miss_count++;
                return std::nullopt;
            }
        }

        /* Promote node to MRU position */
        remove_from_dll(ref_node);
        add_to_dll(ref_node, head.get());

        hit_count++;
        return ref_node->value;
    }

    std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override
    {
        std::optional<K> evicted_key = std::nullopt;

        auto it = cache_map.find(key);
        if (it != cache_map.end())
        {
            Node<K, V> *ref_node = it->second.get();
            if (head->next != ref_node)
            {
                remove_from_dll(ref_node);
                add_to_dll(ref_node, head.get());
            }
            ref_node->value = value;
            if (expiration_time > 0)
            {
                ref_node->expiration_time = expiration_time;
                ref_node->current_timestamp = std::chrono::steady_clock::now();
                expiration_manager.add_for_expiry(key, ref_node->current_timestamp, expiration_time);
            }
            else
            {
                ref_node->expiration_time = 0;
                expiration_manager.remove_from_expiry(key);
            }
        }
        else
        {
            if (cache_map.size() >= this->capacity)
            {
                evicted_key = evict();
            }
            auto ref_node = std::make_shared<Node<K, V>>(key, value, expiration_time);
            add_to_dll(ref_node.get(), head.get());
            cache_map[key] = ref_node;

            if (expiration_time > 0)
            {
                expiration_manager.add_for_expiry(key, ref_node->current_timestamp, expiration_time);
            }
        }
        return evicted_key;
    }

    bool remove(const K &key) override
    {
        auto it = cache_map.find(key);
        if (it == cache_map.end())
        {
            return false;
        }

        Node<K, V> *ref_node = it->second.get();
        remove_from_dll(ref_node);
        expiration_manager.remove_from_expiry(key);
        cache_map.erase(it);
        return true;
    }

    std::optional<K> evict() override
    {
        if (cache_map.empty())
        {
            return std::nullopt;
        }

        Node<K, V> *end_node = tail->prev;
        K evicted_key = end_node->key;
        remove_from_dll(end_node);
        expiration_manager.remove_from_expiry(evicted_key);
        cache_map.erase(evicted_key);
        eviction_count++;
        return evicted_key;
    }

    uint64_t get_hits() const override { return hit_count; }
    uint64_t get_misses() const override { return miss_count; }
    uint64_t get_evictions() const override { return eviction_count; }
};

