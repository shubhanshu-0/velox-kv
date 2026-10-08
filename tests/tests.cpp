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

// A helper for high-volume operation testing
void run_throughput_test(Cache<int, std::string> &manager, const std::string &type_name)
{
    std::cout << "[TEST] " << type_name << " - High Volume Operations..." << std::endl;

    const int total_ops = 8000;
    for (int j = 0; j < total_ops; ++j)
    {
        manager.set(j, "Val" + std::to_string(j));
    }

    for (int j = 0; j < total_ops; ++j)
    {
        auto res = manager.get(j);
        assert(res.has_value() && res.value() == "Val" + std::to_string(j));
    }

    std::cout << "[PASS] " << type_name << " - High Volume Stable!" << std::endl;
}

int main()
{
    // Test LRUCache Engine Directly
    {
        LRUCache<int, std::string> engine(10000);
        run_basic_test(engine, "LRUCache");
        run_throughput_test(engine, "LRUCache");
    }

    std::cout << "\nALL CACHE ENGINE TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
