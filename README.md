
# KLruKCache
1.  get 和 put 都会增加页面访问次数, 当访问次数达到阈值k才会添加到缓存
2.  在这之前, historyList维护页面访问次数, historyValueMap存储的是访问未达到阈值的key-value
# KLruCache
可以当做k=1的KLruKCache
# KHashLruCaches
1. 上述类中每一次 put get 或是 remove 都由锁保护, 并发性能差

2. 单纯的方法涉及读和写, 比如get, 难以用读写锁优化
- std::shared_lock<std::shared_mutex> readLock(mutex_); // 多线程可读
- std::unique_lock<std::shared_mutex> writeLock(mutex_); // 写线程独占
3. KHashLruCaches 将缓存划分为若干子缓存, 每个子缓存维护自己的锁

4. KHashLruCaches类直接包含(使用)了KLruCache类


# KLfuCache
1. 记录缓存访问次数, 淘汰时从 minFreqList_ 里按 Lru 方式淘汰

2. 对于长期驻留在缓存中的热数据，频率计数可能会无限增长, 过时的旧数据如果访问次数过多, 很难被清除, 

3. 基于第二点, 短期热点数据可能容易被淘汰

4. 冷启动问题：刚加入缓存的项可能因为频率为1而很快被淘汰，即便这些项是近期访问的热门数据
## 优化点
1. 引入访问次数平均值概念，当平均值大于最大平均值限制时将所有结点的访问次数减去最大平均值限制的一半或者一个固定值。

2. 相当于热点数据“老化”了，这样可以避免频次计数溢出，也可以缓解缓存污染

# KHashLfuCache
1. 类似于KHashLruCaches, 分片处理

2. KHashLfuCache直接使用了KLfuCache
