#pragma once

#include "cache_base.hpp"

/*  `eviction_policy.hpp` : This file implements the base class for the eviction policy, 
    the base class provides virtual interface which needs to be implemented by the derived classes
*/
template <typename K, typename V> class EvictionPolicy {
public:
  virtual void evict(Node<K, V> *ref_node, std::unordered_map<K, std::shared_ptr<Node<K, V>>> &cache_map);
  virtual ~EvictionPolicy() = default;
};

template <typename K, typename V>
void EvictionPolicy<K, V>::evict(Node<K, V> *tail, std::unordered_map<K, std::shared_ptr<Node<K, V>>> &cache_map) 
{
      Node<K, V> *prev_node = tail->prev;
      tail->prev = prev_node->prev;
      prev_node->prev->next = tail;
      cache_map.erase(prev_node->key);
}