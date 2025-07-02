#pragma once
#include "KICachePolicy.h"
#include <cstring>
#include <list>
#include <memory>
#include <mutex> //互斥锁
#include <unordered_map>
#include <vector>

namespace KamaCache
{
	//namespace include LruCache and KLruCache

	template <typename Key, typename Value>
	class KLruCache;

	//LruNode维护key value 访问计数 前后指针
	template <typename Key, typename Value>
	class LruNode
	{
	private:
		Key key_;
		Value value_;
		size_t accessCount_;
		std::weak_ptr<LruNode<Key, Value>> prev_;
		std::shared_ptr<LruNode<Key, Value>> next_;

	public:
		LruNode(Key key, Value value):
			key_(key),
			value_(value),
			accessCount_(1)
		{
		}

		Key getKey() const { return key_; }
		Value getValue() const { return value_; }
		void setValue(const Value& value) { value_ = value; }
		size_t getAccessCount() const { return accessCount_; }
		void incrementAccessCount() { ++accessCount_; }

		friend class KLruCache<Key, Value>;
	};


	//LruCache
	template <typename Key, typename Value>
	class KLruCache : public KICachePolicy<Key, Value>
	{
	public:
		using LruNodeType = LruNode<Key, Value>; //节点n
		using NodePtr = std::shared_ptr<LruNodeType>; //节点指针, 使用 shared_ptr 管理
		using NodeMap = std::unordered_map<Key, NodePtr>; //维护一个 hashmap, 装 Key和 node
	private:
		int capacity_;
		NodeMap nodeMap_; //nodeMap_ 存放key和LruNode节点(key value 计数 前后指针)
		std::mutex mutex_; //互斥变量
		NodePtr dummyHead_; //头结点
		NodePtr dummyTail_; //尾节点, 双向链表采用尾插法 
	public:
		KLruCache(int capacity): capacity_(capacity)
		{
			initializeList();
		}

		~KLruCache() override = default;

		void put(Key key, Value value) override; //添加节点或更新value
		bool get(Key key, Value& value) override; //访问key
		Value get(Key key) override;
		void remove(Key key); //去除key对应缓存节点
	private:
		void initializeList(); //初始化头尾节点 
		void updateExistingNode(NodePtr node, const Value& value); //更新节点
		void addNewNode(const Key& key, const Value& value); //添加新节点
		void moveToMostRecent(NodePtr node); //先移除节点, 更新节点插到表尾
		void removeNode(NodePtr node); //移除当前节点, 仅移除不删除
		void insertNode(NodePtr node); //用于节点移动到表尾
		void evictLeastRecent(); //删除节点, nodeMap_中会一并删除
	};

	//public
	template <typename Key, typename Value>
	void KLruCache<Key, Value>::put(Key key, Value value)
	{
		if (capacity_ <= 0)
			return;
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = nodeMap_.find(key);
		if (it != nodeMap_.end())
		{
			updateExistingNode(it->second, value);
		}
		else
		{
			addNewNode(key, value);
		}
	}

	template <typename Key, typename Value>
	bool KLruCache<Key, Value>::get(Key key, Value& value)
	{
		std::lock_guard<std::mutex> lock(mutex_); //mutex_是私有成员, lock构造会自动上锁, 析构解锁
		auto it = nodeMap_.find(key);
		if (it != nodeMap_.end())
		{
			moveToMostRecent(it->second);
			value = it->second->getValue();
			return true;
		}
		return false;
	}

	template <typename Key, typename Value>
	Value KLruCache<Key, Value>::get(Key key)
	{
		Value value{};
		//memset(&value, 0, sizeof(value)); 当Value为基本类型时可用
		get(key, value);
		return value;
	}

	template <typename Key, typename Value>
	void KLruCache<Key, Value>::remove(Key key)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = nodeMap_.find(key);
		if (it != nodeMap_.end())
		{
			removeNode(it->second);
			nodeMap_.erase(it);
		}
	}

	//private
	template <typename Key, typename Value>
	void KLruCache<Key, Value>::initializeList()
	{
		dummyHead_ = std::make_shared<LruNodeType>(Key(), Value()); //(Key(), Value())是类型的默认构造, 
		//对LruNodeType传入两个参数构造Node对象
		dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
		dummyHead_->next_ = dummyTail_;
		dummyTail_->prev_ = dummyHead_;
	}

	template <typename Key, typename Value>
	void KLruCache<Key, Value>::updateExistingNode(NodePtr node, const Value& value)
	{
		node->setValue(value);
		moveToMostRecent(node);
	}

	template <typename Key, typename Value>
	void KLruCache<Key, Value>::addNewNode(const Key& key, const Value& value)
	{
		if (nodeMap_.size() >= capacity_)
		{
			evictLeastRecent();
		}
		NodePtr newNode = std::make_shared<LruNodeType>(key, value);
		insertNode(newNode);
		nodeMap_[key] = newNode;
	}

	template <typename Key, typename Value>
	void KLruCache<Key, Value>::moveToMostRecent(NodePtr node)
	{
		removeNode(node);
		insertNode(node);
	}

	template <typename Key, typename Value>
	void KLruCache<Key, Value>::removeNode(NodePtr node)
	{
		//expired检查weak_ptr指针指向对象是否存在, weak_ptr不能直接像普通指针一样判断
		//lock方法会返回一个shared_ptr, 当使用weak_ptr对象的成员时(比如访问他的next_), 需要lock方法
		if (!node->prev_.expired() && node->next_)
		{
			auto prev = node->prev_.lock();
			prev->next_ = node->next_;
			node->next_->prev_ = prev;
			node->next_ = nullptr;
			node->prev_.reset();
		}
	}

	template <typename Key, typename Value>
	void KLruCache<Key, Value>::insertNode(NodePtr node)
	{
		node->next_ = dummyTail_; //node是shared_ptr
		node->prev_ = dummyTail_->prev_;
		dummyTail_->prev_.lock()->next_ = node;
		dummyTail_->prev_ = node;
	}

	template <typename Key, typename Value>
	void KLruCache<Key, Value>::evictLeastRecent()
	{
		NodePtr leastRecent = dummyHead_->next_;
		removeNode(leastRecent);
		nodeMap_.erase(leastRecent->getKey());
	}


	//KLruKCache-----put和get 不立即添加进缓存, get会增加访问次数
	template <typename Key, typename Value>
	class KLruKCache: public KLruCache<Key, Value>
	{
	private:
		int k_;  //访问次数阈值, 达到k可添加进缓存
		std::unique_ptr<KLruCache<Key, size_t>> historyList_;	//记录key与 count
		std::unordered_map<Key, Value> historyValueMap_; //未达到k次访问的key-value
	public:
		KLruKCache(int capacity, int historyCapacity, int k):
		KLruCache<Key, Value>(capacity),						//调用KLru的构造, 包含了容器的初始化
		historyList_(std::make_unique<KLruCache<Key, size_t>>(historyCapacity)), //historyCapacity 作为KLruCache初始化的参数, 给capacity_
		k_(k)
		{}

		Value get(Key key);
		void put(Key key, Value value);
	};

	template<typename Key, typename Value>
	Value KLruKCache<Key, Value>::get(Key key)
	{
		//get key 即访问次数增加, 修改historyList_
		Value value;
		bool inMainCache = KLruCache<Key, Value>::get(key, value);
		size_t historyCount = historyList_->get(key);  //key is the count
		historyCount++;
		historyList_->put(key, historyCount); //historyList use LruCache method 

		//在缓存中
		if (inMainCache)
		{
			return value;
		}

		//不在缓存, 次数大于k就取值, 
		if (historyCount >= k_)
		{
			auto it = historyValueMap_.find(key);
			//在historyValueMap中找到后取值
			if (it != historyValueMap_.end())
			{
				Value storedValue = it->second; //取值
				//从history_List删除key - count, 添加key - value到缓存(LruCache的容器)
				historyList_->remove(key); //delete key
				historyValueMap_.erase(key); //delete key
				KLruCache<Key, Value>::put(key, storedValue); //将key添加进了缓存
				return storedValue;
			}
		}
		//不在缓存且访问次数小于k_
		return value;
	}

	template<typename Key, typename Value>
	void KLruKCache<Key, Value>::put(Key key, Value value)
	{
		Value existingValue{};
		bool inMainCache = KLruCache<Key, Value>::get(key, existingValue);
		if (inMainCache)
		{
			KLruCache<Key, Value>::put(key, value);
			return;
		}

		size_t historyCount = historyList_->get(key); // get 方法在没有相应key的时候返回的是默认值(size_t 0)
		historyCount++;
		historyList_->put(key, historyCount); //更新historyCount或插入新值
		historyValueMap_[key] = value;

		//判断是否可以加入缓存
		if (historyCount >= k_)
		{
			historyList_->remove(key);
			historyValueMap_.erase(key);
			KLruCache<Key, Value>::put(key, value);
		}
	}



	//KHashLruCaches----------对缓存分片, 该类直接包含(使用)KLruCache类
	template <typename Key, typename Value>
	class KHashLruCaches
	{
	private:
		size_t capacity_;	//总容量
		int sliceNum_;	//分片数
		std::vector<std::unique_ptr<KLruCache<Key, Value>>> lruSliceCaches_; //KLruCache 对象指针矢量
	private:
		size_t Hash(Key key);
	public:
		KHashLruCaches(size_t capacity, int sliceNum):
			capacity_(capacity),
			sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
		{
			size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum));
			for (int i = 0; i < sliceNum_; i++)
			{
				//初始化vector, 每个对象的cache容量都是sliceSize
				lruSliceCaches_.emplace_back(std::make_unique<KLruCache<Key, Value>>(sliceSize));
			}
		}
		void put(Key key, Value value);
		bool get(Key key, Value& value);
		Value get(Key key);
		int getSliceNum() const
		{
			return sliceNum_;
		}
	};

	template <typename Key, typename Value>
	size_t KHashLruCaches<Key, Value>::Hash(Key key)
	{
		std::hash<Key> hashFunc;
		return hashFunc(key); //对key计算返回哈希值
	}

	template <typename Key, typename Value>
	void KHashLruCaches<Key, Value>::put(Key key, Value value)
	{
		size_t sliceIndex = Hash(key) % sliceNum_; //使用hash映射到vector的索引分片
		lruSliceCaches_[sliceIndex]->put(key, value);
	}

	template <typename Key, typename Value>
	bool KHashLruCaches<Key, Value>::get(Key key, Value& value)
	{
		size_t sliceIndex = Hash(key) % sliceNum_; //
		bool inCache = lruSliceCaches_[sliceIndex]->get(key, value);
		return inCache;
	}

	template <typename Key, typename Value>
	Value KHashLruCaches<Key, Value>::get(Key key)
	{
		Value value;
		get(key, value);
		return value;
	}
};
