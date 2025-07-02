#pragma once
#include "KICachePolicy.h"
#include <cstring>
#include <list>
#include <memory> 
#include <mutex> //互斥锁
#include <unordered_map>
namespace KamaCache { //namespace include LruCache and KLruCache

	template <typename Key, typename Value>
	class KLruCache;

	template <typename Key, typename Value>
	class LruNode {
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
		{ }

		Key getKey() const { return key_; }
		Value getValue() const { return value_; }
		void setValue(const Value& value) { value_ = value; }
		size_t getAccessCount() const { return accessCount_; }
		void incrementAccessCount() { ++accessCount_; }

		friend class KLruCache<Key, Value>;
	};

	template <typename Key, typename Value>
	class KLruCache: public KICachePolicy<Key, Value>{
	public:
		using LruNodeType = LruNode<Key, Value>; //节点n
		using NodePtr = std::shared_ptr<LruNodeType>; //节点指针, 使用 shared_ptr 管理
		using NodeMap = std::unordered_map <Key, NodePtr>; //维护一个 hashmap, 装 Key和 node
	private:
		int capacity_;
		NodeMap nodeMap_;
		std::mutex mutex_;
		NodePtr dummyHead_;
		NodePtr dummyTail_;
	public:
		KLruCache(int capacity): capacity_(capacity){
			initializeList();
		}
		~KLruCache() override = default;

		void put(Key key, Value value) override;
		bool get(Key key, Value& value) override;
		Value get(Key key) override;
		void remove(Key key);
	private:
		void initializeList();
		void updateExistingNode(NodePtr node, const Value& value);
		void addNewNode(const Key& key, const Value& value);
		void moveToMostRecent(NodePtr node);
		void removeNode(NodePtr node);
		void insertNode(NodePtr node);
		void evictLeastRecent();
	};
	//public
	template <typename Key, typename Value>
	void KLruCache<Key, Value>::put(Key key, Value value){
		if (capacity_ <= 0)
			return;
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = nodeMap_.find(key);
		if (it != nodeMap_.end()) {
			updateExistingNode(it->second, value);
		}
		else
		{
			addNewNode(key, value);
		}
	}
	template <typename Key, typename Value>
	bool KLruCache<Key, Value>::get(Key key, Value& value){
		std::lock_guard<std::mutex> lock(mutex_); //mutex_是私有成员, lock构造会自动上锁, 析构解锁
		auto it = nodeMap_.find(key); 
		if (it != nodeMap_.end()) {
			moveToMostRecent(it->second);
			value = it->second->getValue();
			return true;
		}
		return false;
	}
	template <typename Key, typename Value>
	Value KLruCache<Key, Value>::get(Key key){
		Value value{};
		//memset(&value, 0, sizeof(value)); 当Value为基本类型时可用
		get(key, value);
		return value;
	}
	template <typename Key, typename Value>
	void KLruCache<Key, Value>::remove(Key key) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = nodeMap_.find(key);
		if (it != nodeMap_.end()) {
			removeNode(it->second);
			nodeMap_.erase(it);
		}
	}
	//private
	template <typename Key, typename Value>
	void KLruCache<Key, Value>::initializeList(){
		dummyHead_ = std::make_shared<LruNodeType>(Key(), Value()); //(Key(), Value())是类型的默认构造, 
		//对LruNodeType传入两个参数构造Node对象
		dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
		dummyHead_->next_ = dummyTail_;
		dummyTail_->prev_ = dummyHead_;
	}
	template <typename Key, typename Value>
	void KLruCache<Key, Value>::updateExistingNode(NodePtr node, const Value& value){
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




};

