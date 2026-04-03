#include "../include/policies/lru.hpp"
#include "./cache_manager.hpp"

int main()
 {
  auto cache = std::make_unique<LRUCache<int, std::string>>(15);

  ConcurrentCache<int, std::string> manager(std::move(cache));

  std::vector<std::thread> threads;

  std::thread t1([&](){
    manager.set(1, "hello1");
	manager.set(12, "hello12");
  });

  std::thread t2([&](int key, std::string value){
    manager.set(key, value);
  }, 2, "okay");

  std::thread t3([&](int key){
    auto val = manager.get(key);
    if (val) std::cout << "Value of key 1 is " << *val << std::endl;
  }, 1);

  std::thread t4([&](){
    auto val2 = manager.get(2);
    if (val2) std::cout << "Value of key 2 is " << *val2 << std::endl;
    auto val12 = manager.get(12);
    if (val12) std::cout << "Value of key 12 is " << *val12 << std::endl;
  });

  threads.push_back(std::move(t1));
  threads.push_back(std::move(t2));
  threads.push_back(std::move(t3));
  threads.push_back(std::move(t4));

  for (auto &t : threads)
    t.join();

  return 0;
}