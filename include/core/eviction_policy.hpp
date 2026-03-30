#pragma once

#include "cache_base.hpp"

template <typename K, typename V>
class Eviction_Policy
{
public:
    void evict(Node<K, V> *ref_node, std::map<K, std::shared_ptr<Node<K, V>>>& cache_map);
};

template <typename K, typename V>
inline void Eviction_Policy<K, V>::evict(Node<K, V> *tail, std::map<K, std::shared_ptr<Node<K, V>>>& cache_map)
{
        Node<K, V>* prev_node = tail->prev;
        tail->prev = prev_node->prev;
        prev_node->prev->next = tail;
        cache_map.erase(prev_node->key);
}