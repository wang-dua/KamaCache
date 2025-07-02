#include "KLruCache.h"
#include <iostream>
#include <string>

int main() {
    using KamaCache::KLruCache;
    using KamaCache::KLruKCache;
    using std::cout;
    using std::endl;

	KLruKCache<int, std::string> cache(5, 4, 2);

    // 插入一些键值对
    cache.put(1, "One");
    cache.put(2, "Two");
    cache.put(3, "Three");

    std::string val;
    cout << "1 is not in cache: " << val << endl;
    val = cache.get(1);
    cout << "Now 1 is in cache: " << val << endl;
    return 0;
}
