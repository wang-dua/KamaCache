# KLruKCache
get 和 put 都会增加页面访问次数, 当访问次数达到阈值k才会添加到缓存, 在这之前, historyList维护页面访问次数, historyValueMap存储的是访问未达到阈值的key-value
# KLruCache
可以当做k=1的KLruKCache
# KHashLruCaches
上述类中每一次的 put get 或是 
