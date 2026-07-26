#include "../include/core/cache_manager.hpp"
#include "../include/policies/lru.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// A helper to verify base functionality for ANY cache type
void run_basic_test(Cache<int, std::string> &manager, const std::string &type_name)
{
    std::cout << "[TEST] " << type_name << " - Basic Operations..." << std::endl;

    // 1. Test Set & Get
    manager.set(1, "Val1");
    auto res = manager.get(1);
    assert(res.has_value() && res.value() == "Val1");

    // 2. Test Update
    manager.set(1, "Val1_Updated");
    res = manager.get(1);
    assert(res.has_value() && res.value() == "Val1_Updated");

    // 3. Test Remove
    manager.remove(1);
    res = manager.get(1);
    assert(!res.has_value());

    std::cout << "[PASS] " << type_name << " - Basic Operations Passed!" << std::endl;
}

// A helper for high-concurrency testing
void run_concurrent_test(Cache<int, std::string> &manager, const std::string &type_name)
{
    std::cout << "[TEST] " << type_name << " - Heavy Concurrency..." << std::endl;

    const int ops_per_thread = 1000;
    const int num_threads = 8;
    std::vector<std::thread> workers;

    for (int i = 0; i < num_threads; ++i)
    {
        workers.emplace_back(
            [&manager, i]()
            {
                for (int j = 0; j < ops_per_thread; ++j)
                {
                    int key = i * ops_per_thread + j;
                    manager.set(key, "Val" + std::to_string(key));
                }
            });
    }

    for (auto &t : workers)
        t.join();
    std::cout << "[PASS] " << type_name << " - Concurrency Stable!" << std::endl;
}

int main()
{
    auto factory = [](uint32_t cap) { return std::make_unique<LRUCache<int, std::string>>(cap); };

    // 1. Test Serial Cache
    {
        SerialCache<int, std::string> serial(factory, 100);
        run_basic_test(serial, "SerialCache");
    }

    // 2. Test Concurrent Cache
    {
        ConcurrentCache<int, std::string> concurrent(factory, 500);
        run_basic_test(concurrent, "ConcurrentCache");
        run_concurrent_test(concurrent, "ConcurrentCache");
    }

    // 3. Test Sharded Concurrent Cache
    {
        ShardedConcurrentCache<int, std::string> sharded(factory, 1024);
        run_basic_test(sharded, "ShardedConcurrentCache");
        run_concurrent_test(sharded, "ShardedConcurrentCache");
    }

    std::cout << "\nALL CACHE MANAGER TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
