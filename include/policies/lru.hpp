#pragma once
#include "../core/cache_base.hpp"
/*  `lru.hpp` : This file implements the LRU cache policy,
		offers set(), get() and remove() function
*/

template <typename K, typename V> 
class LRUCache : public Cache<K, V> 
{
private:
	std::unordered_map<K, std::shared_ptr<Node<K, V>>> cache_map; // [Key] -> Pointer to MemoryAddress of Node
	
	/* MEMORY MANAGEMENT STRATEGY:
	   - head and tail are shared_ptr sentinels that live for the entire cache lifetime
	   - They are linked via raw pointers (head->next = tail, tail->prev = head)
	   - Data nodes are stored as shared_ptr in cache_map, keeping them alive while cached
	   - DLL operations use raw pointers for next/prev links (standard DLL pattern)
	   - When a data node is evicted: remove from DLL first, then erase from map to auto-delete
	   - This design is safe because head/tail are member variables and never reassigned
	*/
	std::shared_ptr<Node<K, V>> head, tail; // Head and Tail sentinel nodes of DLL

public:
	LRUCache(uint32_t capacity) : Cache<K, V>(capacity) {
		head = std::make_shared<Node<K, V>>();
		tail = std::make_shared<Node<K, V>>();
		// Link sentinels: head <-> tail (no data nodes yet)
		head->next = tail.get();
		tail->prev = head.get();
	}
	~LRUCache() = default;

	std::optional<V> get(const K& key) override;
	std::optional<K> set(const K &key, const V &value, uint32_t expiration_time = 0) override; 
	bool remove(const K& key) override;
	std::optional<K> evict() override;
};

template <typename K, typename V> 
void remove_from_dll(Node<K, V> *node) 
{
	Node<K, V> *next_node = node->next;
	Node<K, V> *prev_node = node->prev;
	next_node->prev = prev_node;
	prev_node->next = next_node;
}

template <typename K, typename V> 
void add_to_dll(Node<K, V> *node, Node<K, V> *head) 
{
	Node<K, V> *next_node = head->next;
	head->next = node;
	node->prev = head;
	node->next = next_node;
	next_node->prev = node;
}

template <typename K, typename V>
inline std::optional<K> LRUCache<K, V>::evict()
{
	if (cache_map.empty()) return std::nullopt;

	Node<K, V> *end_node = tail->prev;
	K evicted_key = end_node->key;
	remove_from_dll(end_node);
	cache_map.erase(evicted_key);
	return evicted_key;
}

template <typename K, typename V>
inline std::optional<K> LRUCache<K, V>::set(const K& key, const V& value, uint32_t expiration_time) {
	std::optional<K> evicted_key = std::nullopt;

	// if k-v pair exist
	if (cache_map.find(key) != cache_map.end()) {
		Node<K, V> *ref_node = cache_map[key].get();
		// redefining the connections, not altering the memory address
		if (head->next != ref_node) {
			remove_from_dll(ref_node); // remove connection, not delete
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
			cache_map[key]->expiration_time = 0; // 0 means no expiration
		}
	}
	// if k-v pair does not exist
	else {
		// remove a node if capacity exceeds
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

template <typename K, typename V>
inline std::optional<V> LRUCache<K, V>::get(const K& key) 
{
	if (cache_map.find(key) != cache_map.end()) 
	{
		Node<K, V> *ref_node = cache_map[key].get();

		// Check for TTL expiration (Lazy Eviction)
		if (ref_node->expiration_time > 0) {
			auto now = std::chrono::steady_clock::now();
			auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - ref_node->current_timestamp).count();
			if (diff >= ref_node->expiration_time) {
				remove(key);
				return std::nullopt;
			}
		}

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
inline bool LRUCache<K, V>::remove(const K& key) {
	try {
		// If key does not exists
		if (cache_map.find(key) == cache_map.end()) {
			throw std::runtime_error("Key is not present in the LRUCache !");
		}
		// If key exists
		Node<K, V> *ref_node = cache_map[key].get();
		remove_from_dll(ref_node);
		cache_map.erase(key); // shared_ptr auto-deletes when erased from map, no manual delete needed
		return true;
	} catch (const std::exception &ex) {
		std::cerr << "Error removing the Key! " << ex.what() << std::endl;
		return false;
	}
}
