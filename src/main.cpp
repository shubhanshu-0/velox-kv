#include "cache_manager.hpp"

int main()
 {
  LRUCache<int, std::string> cache(2);

  cache.set(1, "hello");
  cache.set(2, "okay");
  auto val1 = cache.get(1);
  if (val1.has_value())
    std::cout << val1.value() << std::endl;
  auto val2 = cache.get(2);
  if (val2.has_value())
    std::cout << val2.value() << std::endl;

  for(auto val : cache.cache_map)
  {
    std::cout << val.first << " " << val.second->value << std::endl;
    std::cout << "size : " << cache.cache_map.size() << std::endl;
  }

  cache.set(3, "hmm");
  auto val3 = cache.get(3);
  if (val3.has_value())
    std::cout << val3.value() << std::endl;
  else
    std::cout << "Value does not exist" << std::endl;

  cache.set(3, "no");
  auto valo = cache.get(3);
  if (valo.has_value())
    std::cout << valo.value() << std::endl;
  else
    std::cout << "Value does not exist" << std::endl;
  
  
  cache.set(4, "done");

  auto val4 = cache.get(4);
  if (val4.has_value())
    std::cout << val4.value() << std::endl;

  auto val5 = cache.get(2);
  if (val5.has_value())
    std::cout << val5.value() << std::endl;
  else
    std::cout << "Value does not exist" << std::endl;

  auto val6 = cache.get(3);
  if (val6.has_value())
    std::cout << val6.value() << std::endl;
  else
    std::cout << "Value does not exist" << std::endl;

  auto val7 = cache.get(5);
  if (val7.has_value())
    std::cout << val7.value() << std::endl;
  else
    std::cout << "Value does not exist" << std::endl;

  return 0;
}