#pragma once
#include "KICachePolicy.h"
#include <cstring>
#include <list>
#include <memory>
#include <mutex> //������
#include <unordered_map>
#include <vector>

namespace KamaCache
{
	//namespace include LruCache and KLruCache

	template <typename Key, typename Value>
	class KLruCache;

	//LruNodeά��key value ���ʼ��� ǰ��ָ��
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
		using LruNodeType = LruNode<Key, Value>; //�ڵ�n
		using NodePtr = std::shared_ptr<LruNodeType>; //�ڵ�ָ��, ʹ�� shared_ptr ����
		using NodeMap = std::unordered_map<Key, NodePtr>; //ά��һ�� hashmap, װ Key�� node
	private:
		int capacity_;
		NodeMap nodeMap_; //nodeMap_ ���key��LruNode�ڵ�(key value ���� ǰ��ָ��)
		std::mutex mutex_; //�������
		NodePtr dummyHead_; //ͷ���
		NodePtr dummyTail_; //β�ڵ�, ˫���������β�巨 
	public:
		KLruCache(int capacity): capacity_(capacity)
		{
			initializeList();
		}

		~KLruCache() override = default;

		void put(Key key, Value value) override; //��ӽڵ�����value
		bool get(Key key, Value& value) override; //����key
		Value get(Key key) override;
		void remove(Key key); //ȥ��key��Ӧ����ڵ�
	private:
		void initializeList(); //��ʼ��ͷβ�ڵ� 
		void updateExistingNode(NodePtr node, const Value& value); //���½ڵ�
		void addNewNode(const Key& key, const Value& value); //����½ڵ�
		void moveToMostRecent(NodePtr node); //���Ƴ��ڵ�, ���½ڵ�嵽��β
		void removeNode(NodePtr node); //�Ƴ���ǰ�ڵ�, ���Ƴ���ɾ��
		void insertNode(NodePtr node); //���ڽڵ��ƶ�����β
		void evictLeastRecent(); //ɾ���ڵ�, nodeMap_�л�һ��ɾ��
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
		std::lock_guard<std::mutex> lock(mutex_); //mutex_��˽�г�Ա, lock������Զ�����, ��������
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
		//memset(&value, 0, sizeof(value)); ��ValueΪ��������ʱ����
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
		dummyHead_ = std::make_shared<LruNodeType>(Key(), Value()); //(Key(), Value())�����͵�Ĭ�Ϲ���, 
		//��LruNodeType����������������Node����
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
		//expired���weak_ptrָ��ָ������Ƿ����, weak_ptr����ֱ������ָͨ��һ���ж�
		//lock�����᷵��һ��shared_ptr, ��ʹ��weak_ptr����ĳ�Աʱ(�����������next_), ��Ҫlock����
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
		node->next_ = dummyTail_; //node��shared_ptr
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


	//KLruKCache-----put��get ��������ӽ�����, get�����ӷ��ʴ���
	template <typename Key, typename Value>
	class KLruKCache: public KLruCache<Key, Value>
	{
	private:
		int k_;  //���ʴ�����ֵ, �ﵽk����ӽ�����
		std::unique_ptr<KLruCache<Key, size_t>> historyList_;	//��¼key�� count
		std::unordered_map<Key, Value> historyValueMap_; //δ�ﵽk�η��ʵ�key-value
	public:
		KLruKCache(int capacity, int historyCapacity, int k):
		KLruCache<Key, Value>(capacity),						//����KLru�Ĺ���, �����������ĳ�ʼ��
		historyList_(std::make_unique<KLruCache<Key, size_t>>(historyCapacity)), //historyCapacity ��ΪKLruCache��ʼ���Ĳ���, ��capacity_
		k_(k)
		{}

		Value get(Key key);
		void put(Key key, Value value);
	};

	template<typename Key, typename Value>
	Value KLruKCache<Key, Value>::get(Key key)
	{
		//get key �����ʴ�������, �޸�historyList_
		Value value;
		bool inMainCache = KLruCache<Key, Value>::get(key, value);
		size_t historyCount = historyList_->get(key);  //key is the count
		historyCount++;
		historyList_->put(key, historyCount); //historyList use LruCache method 

		//�ڻ�����
		if (inMainCache)
		{
			return value;
		}

		//���ڻ���, ��������k��ȡֵ, 
		if (historyCount >= k_)
		{
			auto it = historyValueMap_.find(key);
			//��historyValueMap���ҵ���ȡֵ
			if (it != historyValueMap_.end())
			{
				Value storedValue = it->second; //ȡֵ
				//��history_Listɾ��key - count, ���key - value������(LruCache������)
				historyList_->remove(key); //delete key
				historyValueMap_.erase(key); //delete key
				KLruCache<Key, Value>::put(key, storedValue); //��key��ӽ��˻���
				return storedValue;
			}
		}
		//���ڻ����ҷ��ʴ���С��k_
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

		size_t historyCount = historyList_->get(key); // get ������û����Ӧkey��ʱ�򷵻ص���Ĭ��ֵ(size_t 0)
		historyCount++;
		historyList_->put(key, historyCount); //����historyCount�������ֵ
		historyValueMap_[key] = value;

		//�ж��Ƿ���Լ��뻺��
		if (historyCount >= k_)
		{
			historyList_->remove(key);
			historyValueMap_.erase(key);
			KLruCache<Key, Value>::put(key, value);
		}
	}



	//KHashLruCaches----------�Ի����Ƭ, ����ֱ�Ӱ���(ʹ��)KLruCache��
	template <typename Key, typename Value>
	class KHashLruCaches
	{
	private:
		size_t capacity_;	//������
		int sliceNum_;	//��Ƭ��
		std::vector<std::unique_ptr<KLruCache<Key, Value>>> lruSliceCaches_; //KLruCache ����ָ��ʸ��
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
				//��ʼ��vector, ÿ�������cache��������sliceSize
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
		return hashFunc(key); //��key���㷵�ع�ϣֵ
	}

	template <typename Key, typename Value>
	void KHashLruCaches<Key, Value>::put(Key key, Value value)
	{
		size_t sliceIndex = Hash(key) % sliceNum_; //ʹ��hashӳ�䵽vector��������Ƭ
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
