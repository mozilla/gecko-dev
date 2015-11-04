/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_virtualfilesystem_fuserequestmonitor_h__
#define mozilla_dom_virtualfilesystem_fuserequestmonitor_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "FuseHandler.h"
#include "nsIVirtualFileSystem.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

class FuseRequestMonitor final
{
public:
  NS_INLINE_DECL_REFCOUNTING(FuseRequestMonitor)

  FuseRequestMonitor(FuseHandler* aFuseHanlder);

  void Monitor(nsIVirtualFileSystem* aVirtualFileSystem);
  void Stop();

private:
  ~FuseRequestMonitor() = default;

  class FuseMonitorRunnable final : public nsRunnable
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(FuseMonitorRunnable)

    FuseMonitorRunnable(FuseHandler* aFuseHandler,
                        nsIVirtualFileSystem* aVirtualFileSystem);

    nsresult Run();

  private:
    ~FuseMonitorRunnable() = default;

    bool HandleRequest();

    void HandleInit();
    void HandleLookup();
    void HandleGetAttr();
    void HandleOpen();
    void HandleRead();
    void HandleRelease();
    void HandleOpenDir();
    void HandleReadDir();
    void HandleReleaseDir();

    void ResponseError(int32_t aError);
    void Response(void* aData, size_t aSize);

    RefPtr<FuseHandler> mHandler;
    RefPtr<nsIVirtualFileSystem> mVirtualFileSystem;
  };

  class FuseStopRunnable final : public nsRunnable
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(FuseStopRunnable)

    FuseStopRunnable(FuseHandler* aFuseHandler);

    nsresult Run();
  private:
    ~FuseStopRunnable() = default;

    RefPtr<FuseHandler> mHandler;
  };

  RefPtr<FuseHandler> mHandler;
};

} // end namespace virtualfilesystem
} // end namespace dom
} // end namespace mozilla
#endif
