/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_virtualfilesystem_fuseresponsehandler_h__
#define mozilla_dom_virtualfilesystem_fuseresponsehandler_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "FuseHandler.h"
#include "nsIVirtualFileSystemResponseHandler.h"
#include "nsIVirtualFileSystemCallback.h"

#include "fuse.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

class FuseResponseHandler final : public nsIVirtualFileSystemResponseHandler
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMRESPONSEHANDLER

  FuseResponseHandler(FuseHandler* aFuseHandler);

private:
  class FuseSuccessRunnable final : public nsRunnable
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(FuseSuccessRunnable)
    FuseSuccessRunnable(FuseHandler* aFuseHandler,
                        const uint32_t aRequestId,
                        nsIVirtualFileSystemRequestValue* aValue);
    nsresult Run();
  private:
    ~FuseSuccessRunnable() = default;
    struct fuse_attr CreateAttrByMetadata(nsIEntryMetadata* aMetadata);
    void HandleLookup();
    void HandleGetAttr();
    void HandleOpen();
    void HandleRead();
    void HandleReadDir();

    void Response(void* aData, size_t aSize);
    void ResponseError(const uint32_t aError);

    uint32_t mRequestId;
    RefPtr<FuseHandler> mHandler;
    RefPtr<nsIVirtualFileSystemRequestValue> mValue;
  };

  class FuseErrorRunnable final : public nsRunnable
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(FuseErrorRunnable)
    FuseErrorRunnable(FuseHandler* aFuseHandler,
                     const uint32_t aRequestId,
                     const uint32_t aError);

    nsresult Run();
  private:
    ~FuseErrorRunnable() = default;

    uint32_t mRequestId;
    uint32_t mError;
    RefPtr<FuseHandler> mHandler;
  };

private:
  ~FuseResponseHandler() = default;

  RefPtr<FuseHandler> mHandler;
};

} // end namespace virtualfilesystem
} // end namespace dom
} // end namespace mozilla
#endif
