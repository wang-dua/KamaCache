
1. get 和 put 都会增加页面访问次数, 当访问次数达到阈值k才会添加到缓存

2. 在这之前, historyList维护页面访问次数, historyValueMap存储的是访问未达到阈值的key-value
# KLruCache
可以当做k=1的KLruKCache
# KHashLruCaches
1. 上述类中每一次 put get 或是 remove 都由锁保护, 并发性能差

2. 单纯的方法涉及读和写, 比如get, 难以用读写锁优化

- std::shared_lock<std::shared_mutex> readLock(mutex_); // 多线程可读
- std::unique_lock<std::shared_mutex> writeLock(mutex_); // 写线程独占

3. KHashLruCaches 将缓存划分为若干子缓存, 每个子缓存维护自己的锁.
