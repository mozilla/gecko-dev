
/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkTMultiMap_DEFINED
#define SkTMultiMap_DEFINED

#include "GrTypes.h"
#include "SkTDynamicHash.h"

/** A set that contains pointers to instances of T. Instances can be looked up with key Key.
 * Multiple (possibly same) values can have the same key.
 */
template <typename T,
          typename Key,
          typename HashTraits=T>
class SkTMultiMap {
    struct ValueList {
        explicit ValueList(T* value) : fValue(value), fNext(NULL) {}

        static const Key& GetKey(const ValueList& e) { return HashTraits::GetKey(*e.fValue); }
        static uint32_t Hash(const Key& key) { return HashTraits::Hash(key); }
        T* fValue;
        ValueList* fNext;
    };
public:
    SkTMultiMap() : fCount(0) {}

    ~SkTMultiMap() {
        SkASSERT(fCount == 0);
        SkASSERT(fHash.count() == 0);
    }

    void insert(const Key& key, T* value) {
        ValueList* list = fHash.find(key);
        if (NULL != list) {
            // The new ValueList entry is inserted as the second element in the
            // linked list, and it will contain the value of the first element.
            ValueList* newEntry = SkNEW_ARGS(ValueList, (list->fValue));
            newEntry->fNext = list->fNext;
            // The existing first ValueList entry is updated to contain the
            // inserted value.
            list->fNext = newEntry;
            list->fValue = value;
        } else {
            fHash.add(SkNEW_ARGS(ValueList, (value)));
        }

        ++fCount;
    }

    void remove(const Key& key, const T* value) {
        ValueList* list = fHash.find(key);
        // Since we expect the caller to be fully aware of what is stored, just
        // assert that the caller removes an existing value.
        SkASSERT(NULL != list);
        ValueList* prev = NULL;
        while (list->fValue != value) {
            prev = list;
            list = list->fNext;
        }

        if (NULL != list->fNext) {
            ValueList* next = list->fNext;
            list->fValue = next->fValue;
            list->fNext = next->fNext;
            SkDELETE(next);
        } else if (NULL != prev) {
            prev->fNext = NULL;
            SkDELETE(list);
        } else {
            fHash.remove(key);
            SkDELETE(list);
        }

        --fCount;
    }

    T* find(const Key& key) const {
        ValueList* list = fHash.find(key);
        if (NULL != list) {
            return list->fValue;
        }
        return NULL;
    }

    template<class FindPredicate>
    T* find(const Key& key, const FindPredicate f) {
        ValueList* list = fHash.find(key);
        while (NULL != list) {
            if (f(list->fValue)){
                return list->fValue;
            }
            list = list->fNext;
        }
        return NULL;
    }

    int count() const { return fCount; }

private:
    SkTDynamicHash<ValueList, Key> fHash;
    int fCount;
};

#endif
