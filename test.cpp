#include "KLruCache.h"
#include <iostream>
#include "KLfuCache.h"
#include <string>
using KamaCache::KLruCache;
using KamaCache::KLruKCache;
using KamaCache::KHashLruCaches;
using std::cout;
using std::endl;
using KamaCache::KLfuCache;
using std::string;

void printCacheStats(KLfuCache<int, std::string>& cache) {
    std::cout << "---- Info ----\n";
    std::cout << "maxAverage: " << 100 << "\n"; // 本测试设定 maxAverageNum=100
    std::cout << "curTotalFreq " << cache.getTotalNum() << "\n";
    std::cout << "curAverageFreq: " << cache.getAverageFreq() << "\n";
    std::cout << "--------------------\n";
}
int main() {
    std::cout << "\n========\n";
    KLfuCache<int, std::string> cache(3, 100);  // 设置最大平均访问频次为 100

    cache.put(1, "A");
    cache.put(2, "B");
    cache.put(3, "C");

    int n = cache.getTotalNum();
    std::string val;
    std::cout << "\n==== improve key=1 freq ====\n";
    for (int i = 1; i <= 600; ++i) {
        cache.get(1, val);
        if (i % 100 == 0) {
            std::cout << "access key=1 " << i << " times\n";
            printCacheStats(cache);
            std::cout << "A freq: " << cache.nodeFreq(1) << "\n\n";
        }
    }

    std::cout << "\n==== Insert new cache, observe status====\n";
    cache.put(4, "D"); 

    // 验证缓存状态
    std::cout << "key exists or not: \n";
    for (int k = 1; k <= 4; ++k) {
        bool hit = cache.get(k, val);
    	std::cout << "Key " << k << ": " << (hit ? "exist" : "not exist") << "\n";
        if (hit)
            std::cout << "freq: " << cache.nodeFreq(k) << "\n";
    }

    std::cout << "\nDone\n";
    return 0;
}