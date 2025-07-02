#include "KLruCache.h"
#include <iostream>
#include <string>

int main() {
    using namespace KamaCache;

    // ����һ������Ϊ 3 �� KLruCache ����
    KLruCache<int, std::string> cache(3);

    // ����һЩ��ֵ��
    cache.put(1, "One");
    cache.put(2, "Two");
    cache.put(3, "Three");

    // �����Ƿ��ܷ���
    std::string val;
    if (cache.get(2, val)) {
        std::cout << "Key 2 exists, value = " << val << "\n";  // Ӧ����� "Two"
    }
    else {
        std::cout << "Key 2 not found." << "\n";
    }

    // �����4��Ԫ�أ�Ӧ���������δʹ�õ� key=1
    cache.put(4, "Four");

    if (cache.get(1, val)) {
        std::cout << "Key 1 exists, value = " << val << "\n";
    }
    else {
        std::cout << "Key 1 has been evicted." << "\n";  // Ӧ���������
    }

    // �������ʹ�ø���
    cache.get(2, val);  // ʹ�� key=2
    cache.put(5, "Five"); // Ӧ������ key=3

    if (cache.get(3, val)) {
        std::cout << "Key 3 exists, value = " << val << "\n";
    }
    else {
        std::cout << "Key 3 has been evicted." << "\n";  // Ӧ���������
    }

    return 0;
}
