/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_StreamList_h
#define mozilla_dom_cache_StreamList_h

#include "mozilla/dom/cache/Context.h"
#include "mozilla/dom/cache/Types.h"
#include "nsRefPtr.h"
#include "nsTArray.h"

class nsIInputStream;

namespace mozilla {
namespace dom {
namespace cache {

class CacheStreamControlParent;
class Manager;

class StreamList final : public Context::Activity
{
public:
  StreamList(Manager* aManager, Context* aContext);

  void SetStreamControl(CacheStreamControlParent* aStreamControl);
  void RemoveStreamControl(CacheStreamControlParent* aStreamControl);

  void Activate(CacheId aCacheId);

  void Add(const nsID& aId, nsIInputStream* aStream);
  already_AddRefed<nsIInputStream> Extract(const nsID& aId);

  void NoteClosed(const nsID& aId);
  void NoteClosedAll();
  void Close(const nsID& aId);
  void CloseAll();

  // Context::Activity methods
  virtual void Cancel() override;
  virtual bool MatchesCacheId(CacheId aCacheId) const override;

private:
  ~StreamList();
  struct Entry
  {
    nsID mId;
    nsCOMPtr<nsIInputStream> mStream;
  };
  nsRefPtr<Manager> mManager;
  nsRefPtr<Context> mContext;
  CacheId mCacheId;
  CacheStreamControlParent* mStreamControl;
  nsTArray<Entry> mList;
  bool mActivated;

public:
  NS_INLINE_DECL_REFCOUNTING(cache::StreamList)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_StreamList_h
