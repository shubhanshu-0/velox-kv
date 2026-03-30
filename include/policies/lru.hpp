#pragma once
#include "../core/eviction_policy.hpp"

template <typename K, typename V>
class Cache : public Cache_Operations<K, V>
{
private:
    
    std::map<K, std::shared_ptr<Node<K, V>>> cache_map;
    std::shared_ptr<Node<K, V>> head, tail;
    Eviction_Policy<K, V> lru_eviction;

public:
    Cache(uint32_t capacity) : Cache_Operations<K, V>(capacity) {
        head = std::make_shared<Node<K, V>>();
        tail = std::make_shared<Node<K, V>>();
        head->next = tail.get();
        tail->prev = head.get();
    }
    ~Cache() = default;

    std::optional<V> get(K key) override;
    bool set(K key, V value) override;
    bool remove(K key) override;
};

template <typename K, typename V>
void remove_from_dll(Node<K,V> *node)
{
    Node<K,V> *next_node = node->next; 
    Node<K,V> *prev_node = node->prev;
    next_node->prev = prev_node;
    prev_node->next = next_node;
}

template <typename K, typename V>
void add_to_dll(Node<K, V> *node, Node<K,V> *head)
{
    Node<K,V> *next_node = head->next;
    head->next = node;
    node->prev = head;
    node->next = next_node;
    next_node->prev = node;
}

template <typename K, typename V>
inline bool Cache<K, V>::set(K key, V value)
{
    // if k-v pair exist
    if (cache_map.find(key) != cache_map.end())
    {
        if (cache_map[key]->value == value)
        {
            if (head->next != cache_map[key].get())
            {
                Node<K, V>* ref_node = cache_map[key].get();
                // redefining the connections, not altering the memory address
                remove_from_dll(ref_node); // remove connection, not delete
                add_to_dll(ref_node, head.get());
            }
        }
    }
    // if k-v pair does not exist
    else
    {
        std::shared_ptr<Node<K, V>> ref_node = std::make_shared<Node<K, V>>(key, value);
        add_to_dll(ref_node.get(), head.get());
        cache_map[key] = ref_node;
    }

    
    // remove a node if capacity exceeds
    if (cache_map.size() > this->capacity)
    {
        lru_eviction.evict(tail.get(), cache_map);
    }

    return true;
}

template <typename K, typename V>
inline std::optional<V> Cache<K,V>::get(K key)
{
    if (cache_map.find(key) != cache_map.end())
    {
        Node<K, V>* ref_node = cache_map[key].get();
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

template <typename K, typename V>
inline bool Cache<K,V>::remove(K key)
{
    try
    {
        // If key does not exists
        if(cache_map.find(key) == cache_map.end())
        {
            throw std::runtime_error("Key is not present in the Cache !");
        }
        // If key exists
        Node<K, V>* ref_node = cache_map[key].get();
        remove_from_dll(ref_node);
        cache_map.erase(key);
        // shared_ptr auto-deletes when erased from map, no manual delete needed
        
        return true;
    }
    catch(const std::exception &ex)
    {
        std::cerr << "Error removing the Key! " << ex.what() << std::endl;
        return false;
    }
}
