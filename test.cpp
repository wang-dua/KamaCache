#include "KLruCache.h"
#include <iostream>
#include <string>

int main() {
    using namespace KamaCache;

    // 创建一个容量为 3 的 KLruCache 缓存
    KLruCache<int, std::string> cache(3);

    // 插入一些键值对
    cache.put(1, "One");
    cache.put(2, "Two");
    cache.put(3, "Three");

    // 测试是否能访问
    std::string val;
    if (cache.get(2, val)) {
        std::cout << "Key 2 exists, value = " << val << "\n";  // 应该输出 "Two"
    }
    else {
        std::cout << "Key 2 not found." << "\n";
    }

    // 插入第4个元素，应该驱逐最久未使用的 key=1
    cache.put(4, "Four");

    if (cache.get(1, val)) {
        std::cout << "Key 1 exists, value = " << val << "\n";
    }
    else {
        std::cout << "Key 1 has been evicted." << "\n";  // 应该输出这行
    }

    // 测试最近使用更新
    cache.get(2, val);  // 使用 key=2
    cache.put(5, "Five"); // 应该驱逐 key=3

    if (cache.get(3, val)) {
        std::cout << "Key 3 exists, value = " << val << "\n";
    }
    else {
        std::cout << "Key 3 has been evicted." << "\n";  // 应该输出这行
    }

    return 0;
}
