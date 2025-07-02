# KLruCache
每次访问 key 时，先查 historyList_，访问次数 +1；

如果访问次数 < k：更新 historyMap_；

如果访问次数 == k：把 value 从 historyMap_ 取出，加入主缓存；

如果访问次数 > k：该 key 已在主缓存中，直接更新 LRU 顺序。
