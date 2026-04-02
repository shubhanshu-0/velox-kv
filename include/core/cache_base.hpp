#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

template <typename K, typename V>
/* DLL */
class Node {
public:
  K key;
  V value;
  Node *prev;
  Node *next;
  bool is_sentinel;

  Node() : prev(nullptr), next(nullptr), is_sentinel(true) {}

  Node(K k, V v)
      : key(k), value(v), prev(nullptr), next(nullptr), is_sentinel(false) {}
};

template <typename K, typename V> class Cache {
public:
  uint32_t capacity;
  Cache(uint32_t cap) : capacity(cap) {}
  virtual ~Cache() = default;
  virtual std::optional<V> get(K key) = 0;
  virtual bool set(K key, V value) = 0;
  virtual bool remove(K key) = 0;
};