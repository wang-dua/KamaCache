#pragma once
#include "KICachePolicy.h"
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
namespace KamaCache {
	template <typename Key, typename Value>
	class LruCache;

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
		
	};
};