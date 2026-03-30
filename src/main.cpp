#include "../include/core/cache_base.hpp"
#include "../include/policies/lru.hpp"

int main()
{
    Cache<int, std::string> cache(10);

    cache.set(1, "hello");
    cache.set(2, "okay");
    auto val1 = cache.get(1);
    if (val1.has_value()) std::cout << val1.value() << std::endl;
    
    cache.set(3, "hmm");
    auto val2 = cache.get(2);
    if (val2.has_value()) std::cout << val2.value() << std::endl;
    
    cache.set(4, "done");
    auto val3 = cache.get(1);
    if (val3.has_value()) std::cout << val3.value() << std::endl;
    
    auto val4 = cache.get(3);
    if (val4.has_value()) std::cout << val4.value() << std::endl;
    
    auto val5 = cache.get(4);
    if (val5.has_value()) std::cout << val5.value() << std::endl;

    auto val6 = cache.get(5);
    if (val6.has_value())
        std::cout << val6.value() << std::endl;
    else std::cout<<"Value does not exist"<<std::endl;

    return 0;
}