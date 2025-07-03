#pragma once
#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "KICachePolicy.h"

namespace KamaCache
{
	template <typename Key, typename Value>
	class KLfuCache;

	template <typename Key, typename Value>
	class FreqList
	{
	private:
		//嵌套类
		struct Node
		{
			int freq; //当前节点的访问次数, 与freq无关
			Key key;
			Value value;
			std::weak_ptr<Node> pre;
			std::shared_ptr<Node> next;
			Node(): freq(1), next(nullptr){}
			Node(Key key, Value value): key(key), value(value), freq(1), next(nullptr){}
		};
		using NodePtr = std::shared_ptr<Node>; //use NodePtr instead of Node*
		int freq_;  //这个是存放节点的链表的频次, 创建后是不变的, 用于存放具有相同访问次数的节点, Lfu要用到
		NodePtr head_;
		NodePtr tail_;
	public:
		//初始化链表, 这个时候头尾节点应该生成
		explicit FreqList(int n): freq_(n)
		{
			head_ = std::make_shared<Node>();
			tail_ = std::make_shared<Node>();
			head_->next = tail_; 
			tail_->pre = head_;// Node构造将next指针已初始化为nullptr
		}
		bool isEmpty() const; 
		void addNode(NodePtr node); //尾插法, 相同freq的key按Lru淘汰
		void removeNode(NodePtr node); 
		NodePtr getFirstNode() const; //淘汰第一个有效节点
		friend class KLfuCache<Key, Value>;
	};

	template <typename Key, typename Value>
	class KLfuCache: public KICachePolicy<Key, Value>
	{
		using Node = typename FreqList<Key, Value>::Node; 
		using NodePtr = std::shared_ptr<Node>;
		using NodeMap = std::unordered_map<Key, NodePtr>;

	private:
		size_t capacity_;
		int minFreq_;
		int maxAverageNum_;
		int curAverageNum_;
		int curTotalNum_;
		std::mutex mutex_; 
		NodeMap nodeMap_; //key -- node(pointer)
		//std::unordered_map<int, FreqList<Key, Value>*> freqToFreqList_; //freq -- FreqList(class)
		std::unordered_map<int, std::shared_ptr<FreqList<Key, Value>>> freqToFreqList_;
	public:
		KLfuCache(int capacity, int maxAverageNum = 1000000):
			capacity_(capacity),
			minFreq_(INT8_MAX), //设置minFreq_的最大值, 避免访问次数无限增大
			maxAverageNum_(maxAverageNum),
			curAverageNum_(0),
			curTotalNum_(0){}

		void put(Key key, Value value) override;
		bool get(Key key, Value& value) override;
		Value get(Key key) override;
		void purge();
		int getTotalNum() const
		{ return curTotalNum_; }
		int getAverageFreq() const
		{ return curAverageNum_; }
		int nodeFreq(Key key) { return nodeMap_[key]->freq; }
	private:
		// void putInternal(Key key, Value value);
		// void getInternal(NodePtr node, Value& value);
		// void kickOut();
		void removeFromFreqList(NodePtr node);
		void addToFreqList(NodePtr node);
		void addFreqNum();
		void decreaseFreqNum(int num);
		void handleOverMaxAverageNum();
		void updateMinFreq();
	};

	template<typename Key, typename Value>
	class KHashLfuCache
	{
	private:
		size_t capacity_;
		int sliceNum_;
		std::vector<std::unique_ptr<KLfuCache<Key, Value>>> lfuSliceCaches_;
	public:
		KHashLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10):
		capacity_(capacity),
		sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
		{
			//ceil向上取整可确保每个lru的size都足够
			size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum));
			for (int i = 0; i < sliceNum; i++)
			{
				//emplace_back直接在尾部构造vector元素
				lfuSliceCaches_.emplace_back(std::make_unique<KLfuCache<Key, Value>>(sliceSize, maxAverageNum));
			}
		}

		void put(Key key, Value value);
		Value get(Key key);
		bool get(Key key, Value& value);
		void purge();
	private:
		size_t Hash(Key key);

	};



	template <typename Key, typename Value>
	bool FreqList<Key, Value>::isEmpty() const
	{
		return head_->next == tail_;
	}

	template <typename Key, typename Value>
	void FreqList<Key, Value>::addNode(NodePtr node) //insertToTail
	{
		if (!node || !head_ || !tail_)
			return;
		NodePtr prev = tail_->pre.lock(); 
		prev->next = node;
		node->pre = prev;

		tail_->pre = node;
		node->next = tail_;
	}

	template <typename Key, typename Value>
	void FreqList<Key, Value>::removeNode(NodePtr node)
	{
		if (isEmpty())
			return;
		if (node->pre.expired() || !node->next)
			return;
		NodePtr prev = node->pre.lock();
		prev->next = node->next;
		node->next->pre = prev;
		//prev.reset();
		node->next = nullptr;
	}

	template <typename Key, typename Value>
	typename FreqList<Key, Value>::NodePtr FreqList<Key, Value>::getFirstNode() const
	{
		return head_->next;
	}







	template <typename Key, typename Value>
	void KLfuCache<Key, Value>::put(Key key, Value value)
	{
		if (capacity_ == 0)
			return;
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = nodeMap_.find(key);
		//找到key, 更新值, 变更freqList
		if (it != nodeMap_.end())
		{
			NodePtr node = it->second;
			node->value = value; //更新值
			//变更频次
			removeFromFreqList(node); //先从原位置移除节点
			++node->freq;
			addToFreqList(node); //再添加进新的freqList
			if (freqToFreqList_[node->freq - 1]->isEmpty() && minFreq_ == node->freq-1)
			{
				freqToFreqList_.erase(node->freq - 1);
				minFreq_ = node->freq;
			}
			addFreqNum();
			return;
		}

		//未找到, 先检查缓存是否已满, 淘汰节点, 添加节点, 计算FreqNum
		if (nodeMap_.size() == capacity_)
		{
			NodePtr node = freqToFreqList_[minFreq_]->getFirstNode();
			removeFromFreqList(node);
			nodeMap_.erase(node->key);
			decreaseFreqNum(node->freq);
		}
		//添加节点, 计算FreqNum
		NodePtr newNode = std::make_shared<Node>(key, value);
		nodeMap_[key] = newNode;
		addToFreqList(newNode);
		addFreqNum();
		minFreq_ = 1;
		return;
	}

	template <typename Key, typename Value>
	bool KLfuCache<Key, Value>::get(Key key, Value& value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		//找到节点后, 移动key, 更新FreqNum
		auto it = nodeMap_.find(key);
		if (it != nodeMap_.end())
		{
			NodePtr node = it->second;
			value = node->value;
			removeFromFreqList(node);
			node->freq++;
			addToFreqList(node);
			if (freqToFreqList_[node->freq-1]->isEmpty() && minFreq_ == node->freq-1)
			{
				freqToFreqList_.erase(node->freq - 1);
				minFreq_ = node->freq;
			}
			addFreqNum();
			return true;
		}
		return false;
	}

	template <typename Key, typename Value>
	Value KLfuCache<Key, Value>::get(Key key)
	{
		Value value{};
		get(key, value);
		return value;
	}

	template <typename Key, typename Value>
	void KLfuCache<Key, Value>::purge()
	{
		nodeMap_.clear();
		freqToFreqList_.clear();
	}

	template <typename Key, typename Value>
	void KLfuCache<Key, Value>::removeFromFreqList(NodePtr node)
	{
		if (!node)
			return;
		freqToFreqList_[node->freq]->removeNode(node);
	}

	template <typename Key, typename Value>
	void KLfuCache<Key, Value>::addToFreqList(NodePtr node)
	{
		if (!node)
			return;
		if (freqToFreqList_.find(node->freq) == freqToFreqList_.end())
		{
			//freqToFreqList_[node->freq] = new FreqList<Key, Value>(node->freq);
			freqToFreqList_[node->freq] = std::make_shared<FreqList<Key, Value>>(node->freq);
		}
		freqToFreqList_[node->freq]->addNode(node);
	}

	template <typename Key, typename Value>
	void KLfuCache<Key, Value>::addFreqNum()
	{
		curTotalNum_++;
		if (nodeMap_.empty())
			curAverageNum_ = 0;
		else
			curAverageNum_ = curTotalNum_ / nodeMap_.size();
		if (curAverageNum_ > maxAverageNum_)
			handleOverMaxAverageNum();
	}

	template <typename Key, typename Value>
	void KLfuCache<Key, Value>::decreaseFreqNum(int num)
	{
		curTotalNum_ -= num;
		if (nodeMap_.empty())
			curAverageNum_ = 0;
		else
		{
			curAverageNum_ = curTotalNum_ / nodeMap_.size();
		}
	}

	template <typename Key, typename Value>
	void KLfuCache<Key, Value>::handleOverMaxAverageNum()
	{
		if (nodeMap_.empty())
			return;
		for (auto it = nodeMap_.begin(); it != nodeMap_.end(); it++)
		{
			if (!it->second)
				continue;
			NodePtr node = it->second;
			removeFromFreqList(node); 
			node->freq -= maxAverageNum_ / 2;
			if (node->freq < 1) node->freq = 1;
			addToFreqList(node);
		}
		updateMinFreq();
	}

	template <typename Key, typename Value>
	void KLfuCache<Key, Value>::updateMinFreq()
	{
		minFreq_ = INT8_MAX;
		for (const auto& pair : freqToFreqList_)
		{
			if (pair.second && !pair.second->isEmpty())
				minFreq_ = std::min(minFreq_, pair.first);
		}
		if (minFreq_ == INT8_MAX)
			minFreq_ = 1;
	}


	template <typename Key, typename Value>
	void KHashLfuCache<Key, Value>::put(Key key, Value value)
	{
		size_t sliceIndex = Hash(key) % sliceNum_;
		lfuSliceCaches_[sliceIndex]->put(key, value);
	}

	template <typename Key, typename Value>
	Value KHashLfuCache<Key, Value>::get(Key key)
	{
		Value value{};
		get(key, value);
		return value;
	}

	template <typename Key, typename Value>
	bool KHashLfuCache<Key, Value>::get(Key key, Value& value)
	{
		size_t sliceIndex = Hash(key) % sliceNum_;
		return lfuSliceCaches_[sliceIndex]->get(key, value);
	}

	template <typename Key, typename Value>
	void KHashLfuCache<Key, Value>::purge()
	{
		for (auto& lfuSliceCache : lfuSliceCaches_)
			lfuSliceCache->purge();
	}

	template <typename Key, typename Value>
	size_t KHashLfuCache<Key, Value>::Hash(Key key)
	{
		std::hash<Key> Hash;
		return Hash(key);
	}

}




