#pragma once
#include "../core/cache_base.hpp"

/**
 * @file lru.hpp
 * @brief Least Recently Used (LRU) cache policy implementation.
 *
 * Implements the standard LRU cache strategy using a combination of a doubly linked list (for tracking recency)
 * and a hash map (for O(1) lookups).
 */

template <typename K, typename V> class LRUCache : public Cache<K, V>
{
private:
    std::unordered_map<K, std::shared_ptr<Node<K, V>>> cache_map; // Key to Node pointer mapping.
    std::shared_ptr<Node<K, V>> head, tail;                       // Sentinel nodes of the DLL.

public:
    /**
     * @brief Constructor for the LRU Cache.
     *
     * Sets up the sentinel nodes and links them together to form the initial empty list.
     *
     * @param capacity The maximum capacity of the cache.
     */
    LRUCache(uint32_t capacity) : Cache<K, V>(capacity)
    {
        head = std::make_shared<Node<K, V>>();
        tail = std::make_shared<Node<K, V>>();

        /* Link sentinels: head <-> tail */
        head->next = tail.get();
        tail->prev = head.get();
    }
    ~LRUCache() = default;

    std::optional<V> get(const K &key) override;
    std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override;
    bool remove(const K &key) override;
    std::optional<K> evict() override;

    uint64_t get_hits() const override { return 0; }
    uint64_t get_misses() const override { return 0; }
    uint64_t get_evictions() const override { return 0; }
};

/**
 * @brief Helper to detach a node from the doubly linked list.
 *
 * Rewires the neighboring nodes to bypass the given node.
 */
template <typename K, typename V> void remove_from_dll(Node<K, V> *node)
{
    Node<K, V> *next_node = node->next;
    Node<K, V> *prev_node = node->prev;
    next_node->prev = prev_node;
    prev_node->next = next_node;
}

/**
 * @brief Helper to insert a node at the front of the list (just after the head sentinel).
 *
 * Places the node in the "Most Recently Used" (MRU) position.
 */
template <typename K, typename V> void add_to_dll(Node<K, V> *node, Node<K, V> *head)
{
    Node<K, V> *next_node = head->next;
    head->next = node;
    node->prev = head;
    node->next = next_node;
    next_node->prev = node;
}

template <typename K, typename V> inline std::optional<K> LRUCache<K, V>::evict()
{
    if (cache_map.empty())
    {
        return std::nullopt;
    }

    /* Evict the node immediately preceding the tail sentinel (Least Recently Used). */
    Node<K, V> *end_node = tail->prev;
    K evicted_key = end_node->key;
    remove_from_dll(end_node);
    cache_map.erase(evicted_key);
    return evicted_key;
}

template <typename K, typename V>
inline std::optional<K> LRUCache<K, V>::set(const K &key, const V &value, uint32_t expiration_time)
{
    std::optional<K> evicted_key = std::nullopt;

    /* Check if the key already exists. */
    if (cache_map.find(key) != cache_map.end())
    {
        Node<K, V> *ref_node = cache_map[key].get();

        /* Promote node to the front if it isn't already there. */
        if (head->next != ref_node)
        {
            remove_from_dll(ref_node);
            add_to_dll(ref_node, head.get());
        }
        cache_map[key]->value = value;

        if (expiration_time > 0)
        {
            cache_map[key]->expiration_time = expiration_time;
            cache_map[key]->current_timestamp = std::chrono::steady_clock::now();
        }
        else
        {
            cache_map[key]->expiration_time = 0; // 0 indicates no expiration.
        }
    }
    else
    {
        /* Evict the oldest item if capacity is exceeded. */
        if (cache_map.size() >= this->capacity)
        {
            evicted_key = this->evict();
        }
        std::shared_ptr<Node<K, V>> ref_node = std::make_shared<Node<K, V>>(key, value, expiration_time);
        add_to_dll(ref_node.get(), head.get());
        cache_map[key] = ref_node;
    }
    return evicted_key;
}

template <typename K, typename V> inline std::optional<V> LRUCache<K, V>::get(const K &key)
{
    if (cache_map.find(key) != cache_map.end())
    {
        Node<K, V> *ref_node = cache_map[key].get();

        /* Lazy TTL eviction check. */
        if (ref_node->expiration_time > 0)
        {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - ref_node->current_timestamp).count();
            if (diff >= ref_node->expiration_time)
            {
                remove(key);
                return std::nullopt;
            }
        }

        /* Promote node to MRU position upon access. */
        // redefining the connections, not altering the memory address
        remove_from_dll(ref_node); // remove connection, not delete
        add_to_dll(ref_node, head.get());

        return ref_node->value;
    }
    else
    {
        return std::nullopt;
    }
}

template <typename K, typename V> inline bool LRUCache<K, V>::remove(const K &key)
{
    try
    {
        if (cache_map.find(key) == cache_map.end())
        {
            throw std::runtime_error("Key is not present in the LRUCache!");
        }

        Node<K, V> *ref_node = cache_map[key].get();
        remove_from_dll(ref_node);
        cache_map.erase(key); // shared_ptr auto-deletes when erased from map, no manual delete needed

        return true;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error removing key: " << ex.what() << std::endl;
        return false;
    }
}
