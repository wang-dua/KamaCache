#include "KLruCache.h"
#include <iostream>
#include <string>

int main() {
    using KamaCache::KLruCache;
    using KamaCache::KLruKCache;
    using KamaCache::KHashLruCaches;
    using std::cout;
    using std::endl;

	KHashLruCaches<int, std::string> hashCache(5, 2);
	hashCache.put(1, "wang");
	hashCache.put(2, "xu");
	hashCache.put(3, "Li");
	hashCache.put(4, "wen");
	hashCache.put(5, "mu");
	 //
	std::string name1 = hashCache.get(1);
	cout << "1" << " " << name1 << "\n";
    // if (hashCache.get(2, name1))
    // {
    //     cout << "2" << " " << name1 << "\n";
    // }



    /*KHashLruCaches<std::string, std::string> cache(100, 4);

    // 插入一些键值对
    cache.put("apple", "red");
    cache.put("banana", "yellow");
    cache.put("grape", "purple");

    // 尝试取出数据
    std::string val;

    if (cache.get("apple", val)) {
        std::cout << "apple: " << val << std::endl;
    }
    else {
        std::cout << "apple not found\n";
    }

    if (cache.get("banana", val)) {
        std::cout << "banana: " << val << std::endl;
    }
    else {
        std::cout << "banana not found\n";
    }

    if (cache.get("grape", val)) {
        std::cout << "grape: " << val << std::endl;
    }
    else {
        std::cout << "grape not found\n";
    }

    if (cache.get("orange", val)) {
        std::cout << "orange: " << val << std::endl;
    }
    else {
        std::cout << "orange not found (as expected)\n";
    }

    std::cout << "sliceNum = " << cache.getSliceNum() << std::endl;

    return 0;*/
    return 0;
}
