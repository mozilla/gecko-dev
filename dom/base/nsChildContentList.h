/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChildContentList_h__
#define nsChildContentList_h__

#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsINodeList.h"   // base class
#include "js/TypeDecls.h"  // for Handle, Value, JSObject, JSContext

class nsIContent;
class nsINode;

/**
 * Class that implements the nsINodeList interface (a list of children of
 * the content), by holding a reference to the content and delegating Length
 * and Item to its existing child list.
 * @see nsINodeList
 */
class nsAttrChildContentList : public nsINodeList {
 public:
  explicit nsAttrChildContentList(nsINode* aNode) : mNode(aNode) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS(nsAttrChildContentList)

  // nsWrapperCache
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  // nsINodeList interface
  int32_t IndexOf(nsIContent* aContent) override;
  nsIContent* Item(uint32_t aIndex) override;
  uint32_t Length() override;
  nsINode* GetParentObject() final { return mNode; }

  virtual void InvalidateCacheIfAvailable() {}

 protected:
  virtual ~nsAttrChildContentList() = default;

  // The node whose children make up the list.
  RefPtr<nsINode> mNode;
};

class nsParentNodeChildContentList final : public nsAttrChildContentList {
 public:
  explicit nsParentNodeChildContentList(nsINode* aNode)
      : nsAttrChildContentList(aNode) {
    ValidateCache();
  }

  // nsINodeList interface
  int32_t IndexOf(nsIContent* aContent) override;
  nsIContent* Item(uint32_t aIndex) override;
  uint32_t Length() override;

  void InvalidateCacheIfAvailable() final { InvalidateCache(); }

  void InvalidateCache() {
    mIsCacheValid = false;
    mCachedChildArray.Clear();
  }

 private:
  ~nsParentNodeChildContentList() = default;

  // Return true if validation succeeds, false otherwise
  void ValidateCache();
  void EnsureCacheValid() {
    if (!mIsCacheValid) {
      ValidateCache();
    }
    MOZ_ASSERT(mIsCacheValid);
  }

  // Whether cached array of child nodes is valid
  bool mIsCacheValid = false;

  // Cached array of child nodes
  AutoTArray<nsIContent*, 8> mCachedChildArray;
};

#endif /* nsChildContentList_h__ */
