#pragma once

#include "cache_base.hpp"

template <typename K, typename V> class EvictionPolicy {
public:
  virtual void evict(Node<K, V> *ref_node,
                     std::map<K, std::shared_ptr<Node<K, V>>> &cache_map);
  virtual ~EvictionPolicy() = default;
};

template <typename K, typename V>
void EvictionPolicy<K, V>::evict(
    Node<K, V> *tail, std::map<K, std::shared_ptr<Node<K, V>>> &cache_map) {
  Node<K, V> *prev_node = tail->prev;
  K k = prev_node->key;
  V v = prev_node->value;
  tail->prev = prev_node->prev;
  prev_node->prev->next = tail;
  cache_map.erase(prev_node->key);
  std::cout << "Cache Eviction Successfull ! " << k << " " << v << std::endl;
}