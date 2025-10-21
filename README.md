## 概述

本项目实现了一个多级缓存系统，包含 **K-LRU**、**K-LFU** 及其分片版本，针对不同场景提供优化的缓存策略。

---

## 1. KLruKCache - 访问频率阈值缓存

### 核心特性
```cpp
/**
 * 基于访问频率的 LRU 缓存 - 只有达到访问阈值 K 的条目才会进入缓存
 * 
 * 设计思想：避免短期临时数据占用缓存空间
 */
```

### 数据结构
```cpp
class KLruKCache {
private:
    // 历史访问记录（未达到阈值 K）
    std::list<Key> historyList_;
    std::unordered_map<Key, HistoryValue> historyValueMap_;
    
    // 正式缓存（达到阈值 K）
    std::list<Key> cacheList_; 
    std::unordered_map<Key, CacheValue> cacheMap_;
    
    size_t capacity_;
    int thresholdK_;  // 访问阈值
}
```

### 工作流程
```
访问流程：
1. get(key) / put(key, value)
   ↓
2. 在 historyValueMap_ 中增加访问计数
   ↓
3. 如果访问次数 < K: 只更新历史记录
   ↓  
4. 如果访问次数 ≥ K: 从历史记录迁移到正式缓存
   ↓
5. 执行标准 LRU 调整
```

### 适用场景
- **热点数据筛选**：自动识别并缓存真正的高频数据
- **内存优化**：避免缓存短期访问的数据
- **访问模式分析**：通过阈值 K 控制缓存准入标准

---

## 2. KLruCache - 标准 LRU 缓存

```cpp
/**
 * 标准 LRU 缓存实现 - 可视为 KLruKCache 在 K=1 时的特例
 * 
 * 最近最少使用淘汰策略：
 * - 新访问的条目移动到头部
 * - 淘汰时从尾部移除
 */
```

### 简化特性
- 所有访问直接进入缓存（无阈值限制）
- 简单的链表 + 哈希表实现
- 基础缓存功能，性能最佳

---

## 3. KHashLruCaches - 分片 LRU 缓存

### 并发性能问题分析
```cpp
/**
 * 解决单一锁的性能瓶颈：
 * 
 * 问题：
 * - 每个操作都需要全局锁保护
 * - get() 操作包含读写（LRU 位置调整）
 * - 难以使用读写锁优化
 * 
 * 解决方案：缓存分片
 */
```

### 分片设计
```cpp
class KHashLruCaches {
private:
    std::vector<KLruCache> shards_;     // 缓存分片
    std::vector<std::mutex> mutexes_;   // 分片级别锁
    size_t shardCount_;                 // 分片数量
    
    // 哈希函数确定数据分片
    size_t getShardIndex(const Key& key) {
        return std::hash<Key>{}(key) % shardCount_;
    }
}
```

### 锁优化效果
```
优化前：全局锁
Thread1: get(key1) ──────────────┐
Thread2: get(key2) ────────┐     │ 串行执行
Thread3: put(key3) ────┐   │     │
                      │   │     │
时间 ──────────────────────────────────→

优化后：分片锁  
Thread1: get(key1) [Shard1] ──────┐
Thread2: get(key2) [Shard2] ────┐ │ 并行执行
Thread3: put(key3) [Shard3] ──┐ │ │
                             │ │ │
时间 ──────────────────────────────────→
```

### 性能提升
- **锁竞争减少**：不同分片的操作完全并行
- **缓存局部性**：每个分片数据更紧凑
- **可扩展性**：分片数随 CPU 核心数调整

---

## 4. KLfuCache - 频率计数 LFU 缓存

### 核心架构
```cpp
class KLfuCache {
private:
    // 频率层级结构
    std::unordered_map<Key, FreqNode> keyToFreq_;
    std::unordered_map<int, std::list<Key>> freqLists_;  // 各频率的 LRU 列表
    std::unordered_map<Key, Value> cache_;
    
    int minFreq_;      // 当前最小频率
    size_t capacity_;
}
```

### 算法流程
```
访问流程：
1. get(key) → 频率 +1，移动到更高频率 LRU 头部
2. put(key, value) → 新条目频率=1，插入 freqLists_[1]
3. 淘汰策略：从 minFreqList_ 的尾部移除（最低频率中最久未使用）
```

### 现存问题

#### 1. **频率计数无限增长**
```cpp
// 长期热数据频率持续累积
hot_data → freq: 10000+
new_data → freq: 1

// 问题：旧的高频数据难以被淘汰
```

#### 2. **短期热点容易被淘汰**
```cpp
// 新加入的热点数据
new_hot_data → 频率增长需要时间
// 可能在被识别为热点前就被淘汰
```

#### 3. **缓存污染**
```cpp
// 过时但历史上高频的数据长期占用空间
old_popular_data → 频率高但实际已不再访问
```

### 优化方案：频率老化机制

```cpp
class OptimizedKLfuCache {
    void aging() {
        if (currentAverageFreq() > maxAvgThreshold_) {
            // 所有节点频率衰减
            for (auto& node : keyToFreq_) {
                node.freq = std::max(1, node.freq - decayValue_);
            }
            rebuildFreqLists();  // 重建频率层级
        }
    }
    
    // 触发条件：平均频率 > 阈值时执行老化
}
```

#### 老化策略对比：
| 策略 | 优点 | 缺点 |
|------|------|------|
| **减半衰减** | 快速降低频率 | 可能过度衰减 |
| **固定值衰减** | 控制更精确 | 需要调优参数 |
| **按比例衰减** | 保持相对频率 | 计算开销大 |

---

## 5. KHashLfuCache - 分片 LFU 缓存

### 架构设计
```cpp
/**
 * 分片 LFU 缓存 - 结合分片并发与 LFU 淘汰策略
 * 
 * 继承 KHashLruCaches 的分片思想，应用于 LFU 场景
 */
class KHashLfuCache {
private:
    std::vector<KLfuCache> shards_;    // LFU 分片
    std::vector<std::mutex> mutexes_;  // 分片锁
    size_t shardCount_;
}
```

### 性能优势
- **并发访问**：不同分片的 LFU 操作并行执行
- **锁粒度细化**：每个分片独立维护频率统计
- **负载均衡**：哈希分片均匀分布访问压力

---

## 缓存策略对比总结

| 缓存类型 | 淘汰策略 | 并发支持 | 适用场景 |
|----------|----------|----------|----------|
| **KLruCache** | 最近最少使用 | 单锁 | 简单缓存、访问模式均匀 |
| **KLruKCache** | 频率阈值+LRU | 单锁 | 热点识别、内存敏感 |
| **KHashLruCaches** | LRU + 分片 | 分片锁 | 高并发读写的 LRU |
| **KLfuCache** | 最不经常使用 | 单锁 | 长期热点数据 |
| **KHashLfuCache** | LFU + 分片 | 分片锁 | 高并发频率敏感场景 |

## 设计模式应用

### 1. **策略模式**
```cpp
// 不同的淘汰策略实现
interface EvictionPolicy {
    void onGet(Key key);
    void onPut(Key key, Value value);
    Key evict();
}

class LruPolicy : public EvictionPolicy {...}
class LfuPolicy : public EvictionPolicy {...}
class LruKPolicy : public EvictionPolicy {...}
```

### 2. **分片模式**
```cpp
// 通过哈希分片提升并发性能
template<typename CacheImpl>
class ShardedCache {
    std::vector<CacheImpl> shards_;
    // 分片路由和锁管理
}
```

这个缓存框架通过分层设计和策略组合，为不同业务场景提供了灵活的缓存解决方案。
