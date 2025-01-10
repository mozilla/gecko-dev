/* -*- Mode: C++; tab-width: 13; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=13 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_CACHE_INVALIDATOR_H_
#define MOZILLA_CACHE_INVALIDATOR_H_

#include "DmdStdContainers.h"
#include "mozilla/Assertions.h"

#include <memory>
#include <optional>
#include <vector>

// -

namespace mozilla {

class AbstractCache;

// -

class CacheInvalidator {
  friend class AbstractCache;

 private:
  mutable webgl::dmd_unordered_set<AbstractCache*> mCaches;

 public:
  virtual ~CacheInvalidator() {
    // It's actually generally unsafe to wait until now to invalidate caches,
    // because when used as a mixin, this dtor is called after the dtor for the
    // derived class. This means that if the derived class holds a cache (or is
    // a cache!), OnInvalidate() will be called on a destroyed object.
    // MOZ_ASSERT(!mCaches);
    InvalidateCaches();
  }

  void InvalidateCaches() const;

  // -

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mso) const {
    return mCaches.SizeOfExcludingThis(mso);
  }
};

// -

class AbstractCache {
  using InvalidatorListT = std::vector<const CacheInvalidator*>;

 private:
  InvalidatorListT mInvalidators;

 public:
  AbstractCache() = default;

  explicit AbstractCache(InvalidatorListT&& invalidators) {
    ResetInvalidators(std::move(invalidators));
  }

  virtual ~AbstractCache() { ResetInvalidators({}); }

 public:
  virtual void OnInvalidate() = 0;

  void ResetInvalidators(InvalidatorListT&&);
  void AddInvalidator(const CacheInvalidator&);
};

// -

template <typename T>
class CacheMaybe : public AbstractCache {
  std::optional<T> mVal;

 public:
  CacheMaybe& operator=(std::optional<T>&& rhs) {
    mVal.swap(rhs);
    rhs.reset();
    return *this;
  }

  template <typename U>
  CacheMaybe& operator=(U&& rhs) {
    mVal.emplace(std::move(rhs));
    return *this;
  }

  CacheMaybe& operator=(nullptr_t) {
    mVal.reset();
    return *this;
  }

  void OnInvalidate() override {
    *this = {};
    ResetInvalidators({});
  }

  explicit operator bool() const { return bool(mVal); }
  T* get() const { return mVal ? &*mVal : nullptr; }
  T* operator->() const { return get(); }
};

// -

template <typename KeyT, typename ValueT>
class CacheWeakMap final {
  class Entry final : public AbstractCache {
   public:
    CacheWeakMap& mParent;
    const KeyT mKey;
    const ValueT mValue;

    Entry(CacheWeakMap& parent, const KeyT& key, ValueT&& value)
        : mParent(parent), mKey(key), mValue(std::move(value)) {}

    void OnInvalidate() override {
      const auto erased = mParent.mMap.erase(&mKey);
      MOZ_ALWAYS_TRUE(erased == 1);
    }
  };

  struct DerefHash final {
    size_t operator()(const KeyT* const a) const {
      return std::hash<KeyT>()(*a);
    }
  };
  struct DerefEqual final {
    bool operator()(const KeyT* const a, const KeyT* const b) const {
      return *a == *b;
    }
  };

  using MapT = webgl::dmd_unordered_map<const KeyT*, std::unique_ptr<Entry>,
                                        DerefHash, DerefEqual>;
  MapT mMap;

 public:
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mso) const {
    return mMap.SizeOfExcludingThis(mso);
  }

  std::unique_ptr<Entry> MakeEntry(const KeyT& key, ValueT&& value) {
    return std::unique_ptr<Entry>(new Entry(*this, key, std::move(value)));
  }
  std::unique_ptr<Entry> MakeEntry(const KeyT& key, const ValueT& value) {
    return MakeEntry(key, ValueT(value));
  }

  const ValueT* Insert(std::unique_ptr<Entry>&& entry) {
    auto insertable = typename MapT::value_type{&entry->mKey, std::move(entry)};

    const auto res = mMap.insert(std::move(insertable));
    const auto& didInsert = res.second;
    MOZ_ALWAYS_TRUE(didInsert);

    const auto& itr = res.first;
    return &itr->second->mValue;
  }

  const ValueT* Find(const KeyT& key) const {
    const auto itr = mMap.find(&key);
    if (itr == mMap.end()) return nullptr;

    return &itr->second->mValue;
  }

  void Clear() const {
    while (true) {
      const auto itr = mMap.begin();
      if (itr == mMap.end()) return;
      itr->second->OnInvalidate();
    }
  }

  ~CacheWeakMap() { Clear(); }
};

}  // namespace mozilla

#endif  // MOZILLA_CACHE_INVALIDATOR_H_
