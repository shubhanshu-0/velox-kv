#include<iostream>
#include<map>

class Node
{
    public:
        int key, value;
        Node* prev;
        Node* next;

        Node(int k, int v)
        {
            key = k;
            value = v;
            prev = nullptr;
            next = nullptr;
        }
};

void remove_from_dll(Node *node)
{
    Node *next_node = node->next;
    Node *prev_node = node->prev;
    next_node->prev = prev_node;
    prev_node->next = next_node;
}

void add_to_dll(Node *node, Node* head)
{
    Node* next_node = head->next;
    head->next = node;
    node->prev = head;
    node->next = next_node;
    next_node->prev = node;
}

class Eviction_Policy
{
    // if capacity is full, then perform evition policy to accomodate new values
};

class Cache
{
    public: 
        int capacity;
        std::map<int,Node *> cache_map;
        Node *head, *tail;

        Cache(int capacity)
        {
            this->capacity = capacity;
            head = new Node(-1, -1);
            tail = new Node(-1, -1);
            head->next = tail;
            tail->prev = head;
        }

        int get(int key)
        {
            if(cache_map.find(key) != cache_map.end())
            {
                Node* ref_node = cache_map[key];
                // redefining the connections, not altering the memory address
                remove_from_dll(ref_node); // remove connection, not delete
                add_to_dll(ref_node, head);

                return ref_node->value;
            }
            else
            {
                return -1;
            }
        }

        bool put(int key, int value)
        {
            // if k-v pair exist
            if(cache_map.find(key) != cache_map.end())
            {
                if(cache_map[key]->value == value) 
                {
                    if (head->next != cache_map[key])
                    {
                        Node *ref_node = cache_map[key];
                        // redefining the connections, not altering the memory address
                        remove_from_dll(ref_node); // remove connection, not delete
                        add_to_dll(ref_node, head);
                        delete ref_node;
                    }
                }
            }
            // if k-v pair does not exist
            else
            {
                Node *ref_node = new Node(key, value);
                add_to_dll(ref_node, head);
                cache_map[key] = ref_node;
            }

            // remove a node if capacity exceeds
            if(cache_map.size() > capacity)
            {
                Node* ref_node = tail->prev;
                ref_node->prev->next = tail;
                tail->prev = ref_node->prev;
                cache_map.erase(ref_node->key);
                delete ref_node;
            }
            
            return true;
        }
};

int main()
{
    Cache cache(2);

    cache.put(1, 1);
    cache.put(2, 2);
    std::cout << cache.get(1) << std::endl;
    cache.put(3, 3);
    std::cout << cache.get(2) << std::endl;
    cache.put(4, 4);
    std::cout << cache.get(1) << std::endl;
    std::cout << cache.get(3) << std::endl;
    std::cout << cache.get(4) << std::endl;
}