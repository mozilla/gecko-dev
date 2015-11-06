/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemProvider_h
#define mozilla_dom_FileSystemProvider_h

#include <map>
#include "mozilla/DOMEventTargetHelper.h"
#include "nsIVirtualFileSystemCallback.h"
#include "nsIFileSystemProviderEventDispatcher.h"

class nsIVirtualFileSystemRequestManager;
class nsIVirtualFileSystemService;

namespace mozilla {
namespace dom {

struct MountOptions;
class Promise;
struct UnmountOptions;

} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace dom {

class FileSystemProvider final : public DOMEventTargetHelper
                               , public nsIVirtualFileSystemCallback
                               , public nsIFileSystemProviderEventDispatcher
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFILESYSTEMPROVIDEREVENTDISPATCHER
  NS_DECL_NSIVIRTUALFILESYSTEMCALLBACK

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemProvider, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(unmountrequested)
  IMPL_EVENT_HANDLER(getmetadatarequested)
  IMPL_EVENT_HANDLER(readdirectoryrequested)
  IMPL_EVENT_HANDLER(openfilerequested)
  IMPL_EVENT_HANDLER(closefilerequested)
  IMPL_EVENT_HANDLER(readfilerequested)
  IMPL_EVENT_HANDLER(abortrequested)

  static already_AddRefed<FileSystemProvider> Create(nsPIDOMWindow* aWindow);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Mount(const MountOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<Promise> Unmount(const UnmountOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<Promise> Get(const nsAString& aFileSystemId, ErrorResult& aRv);

private:
  explicit FileSystemProvider(nsPIDOMWindow* aWindow);
  ~FileSystemProvider();
  bool Init();

  nsCOMPtr<nsIVirtualFileSystemService> mVirtualFileSystemService;
  nsCOMPtr<nsIVirtualFileSystemRequestManager> mRequestManager;
  std::map<uint32_t, RefPtr<Promise>> mPendingRequestPromises;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemProvider_h
