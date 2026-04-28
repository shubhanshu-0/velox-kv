#pragma once

#include <iostream>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <shared_mutex>

/*  `cache_base.hpp` : This file Implements the base class for the cache and the Node class for the DLL, 
	the base class provides virtual interface which needs to be implemented by the derived classes
 */

template <typename K, typename V>
/* DLL */
class Node {
public:
	K key;
	V value;
	Node *prev;
	Node *next;
	uint32_t expiration_time; // in seconds, 0 means no expiration
	std::chrono::steady_clock::time_point current_timestamp;
	bool is_sentinel; // True if this is a dummy head/tail node, false for actual data nodes

	// Constructor for Sentinel nodes (dummy placeholders)
	Node() : 
		prev(nullptr), next(nullptr), is_sentinel(true), 
		expiration_time(0) {}

	// Constructor for actual data nodes
	Node(K k, V v, uint32_t expiration_time = 0)
		: key(k), value(v), prev(nullptr), next(nullptr), is_sentinel(false),
		  expiration_time(expiration_time), current_timestamp(std::chrono::steady_clock::now()) {}
};

template <typename K, typename V> class Cache {
protected:
	uint32_t capacity;

public:
	Cache(uint32_t cap) : capacity(cap) {}
	virtual ~Cache() = default;
	virtual std::optional<V> get(const K& key) = 0;
	virtual std::optional<K> set(const K& key, const V& value, uint32_t expiration_time = 0) = 0;
	virtual bool remove(const K& key) = 0;
	virtual std::optional<K> evict() = 0;
};
