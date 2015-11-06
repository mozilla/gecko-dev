/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "nsVirtualFileSystemRequestManager.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"

#if defined(VIRTUAL_FILE_SYSTEM_LOG_TAG)
#undef VIRTUAL_FILE_SYSTEM_LOG_TAG
#endif
#define VIRTUAL_FILE_SYSTEM_LOG_TAG "VirtualFileSystemRequestManager"
#include "VirtualFileSystemLog.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

class RunVirtualFileSystemSuccessCallback : public nsRunnable
{
public:
  explicit RunVirtualFileSystemSuccessCallback(nsIVirtualFileSystemCallback* aCallback,
                                          uint32_t aRequestId,
                                          nsIVirtualFileSystemRequestValue* aValue,
                                          bool aHasMore)
    : mCallback(aCallback)
    , mRequestId(aRequestId)
    , mValue(aValue)
    , mHasMore(aHasMore)
  {}

  NS_IMETHOD Run()
  {
    printf_stderr("########################## mCallback->OnSuccess\n");
    mCallback->OnSuccess(mRequestId, mValue, mHasMore);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIVirtualFileSystemCallback> mCallback;
  uint32_t mRequestId;
  nsCOMPtr<nsIVirtualFileSystemRequestValue> mValue;
  bool mHasMore;
};

class RunVirtualFileSystemErrorCallback : public nsRunnable
{
public:
  explicit RunVirtualFileSystemErrorCallback(nsIVirtualFileSystemCallback* aCallback,
                                        uint32_t aRequestId,
                                        uint32_t aErrorCode)
    : mCallback(aCallback)
    , mRequestId(aRequestId)
    , mErrorCode(aErrorCode)
  {}

  NS_IMETHOD Run()
  {
    mCallback->OnError(mRequestId, mErrorCode);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIVirtualFileSystemCallback> mCallback;
  uint32_t mRequestId;
  uint32_t mErrorCode;
};

class DispatchRequestTask : public nsRunnable
{
public:
  explicit DispatchRequestTask(uint32_t aRequestId,
                               uint32_t aRequestType,
                               nsIVirtualFileSystemRequestOption* aOption,
                               nsIFileSystemProviderEventDispatcher* aDispatcher)
    : mRequestId(aRequestId)
    , mRequestType(aRequestType)
    , mOption(aOption)
    , mDispatcher(aDispatcher)
  {}

  NS_IMETHOD Run()
  {
    mDispatcher->DispatchFileSystemProviderEvent(mRequestId, mRequestType, mOption);
    return NS_OK;
  }

private:
  uint32_t mRequestId;
  uint32_t mRequestType;
  nsCOMPtr<nsIVirtualFileSystemRequestOption> mOption;
  nsCOMPtr<nsIFileSystemProviderEventDispatcher> mDispatcher;
};

NS_IMPL_ISUPPORTS0(nsVirtualFileSystemRequestManager::nsVirtualFileSystemRequest)

nsVirtualFileSystemRequestManager::nsVirtualFileSystemRequest
  ::nsVirtualFileSystemRequest(uint32_t aRequestType,
                          uint32_t aRequestId,
                          nsIVirtualFileSystemRequestOption* aOption,
                          nsIVirtualFileSystemCallback* aCallback)
  : mRequestType(aRequestType)
  , mRequestId(aRequestId)
  , mOption(aOption)
  , mCallback(aCallback)
  , mIsCompleted(false)
{
}

static uint32_t sMonitorId = 0;

NS_IMPL_ISUPPORTS(nsVirtualFileSystemRequestManager,
                  nsIVirtualFileSystemRequestManager)

nsVirtualFileSystemRequestManager::nsVirtualFileSystemRequestManager()
  : mMonitor(nsPrintfCString("RequestIdQueueMonitor%d", sMonitorId++).get())
  , mRequestId(0)
{
}

nsVirtualFileSystemRequestManager::nsVirtualFileSystemRequestManager(
  nsIFileSystemProviderEventDispatcher* dispatcher)
  : mMonitor(nsPrintfCString("RequestIdQueueMonitor%d", sMonitorId++).get())
  , mDispatcher(dispatcher)
  , mRequestId(0)
{
}

NS_IMETHODIMP
nsVirtualFileSystemRequestManager::CreateRequest(uint32_t aRequestType,
                                            nsIVirtualFileSystemRequestOption *aOption,
                                            nsIVirtualFileSystemCallback* aCallback,
                                            uint32_t* aRequestId)
{
  if (NS_WARN_IF(!aRequestId)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aRequestId = 0;

  if (NS_WARN_IF(!(aRequestType < REQUEST_UNKNOWN))) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!(aOption && aCallback))) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mDispatcher)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MonitorAutoLock lock(mMonitor);
  RefPtr<nsVirtualFileSystemRequest> request = new nsVirtualFileSystemRequest(aRequestType,
                                                                      ++mRequestId,
                                                                      aOption,
                                                                      aCallback);
  mRequestMap[mRequestId] = request;
  mRequestIdQueue.push_back(mRequestId);

  nsCOMPtr<nsIRunnable> dispatchTask = new DispatchRequestTask(mRequestId,
                                                               aRequestType,
                                                               aOption,
                                                               mDispatcher);
  nsresult rv = NS_DispatchToCurrentThread(dispatchTask);
  if (NS_FAILED(rv)) {
    DestroyRequest(mRequestId);
    return rv;
  }

  *aRequestId = mRequestId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemRequestManager::FufillRequest(uint32_t aRequestId,
                                            nsIVirtualFileSystemRequestValue* aValue,
                                            bool aHasMore)
{
  if (NS_WARN_IF(aHasMore && !aValue)) {
    return NS_ERROR_INVALID_ARG;
  }

  MonitorAutoLock lock(mMonitor);
  if (mRequestMap.find(aRequestId) == mRequestMap.end()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsVirtualFileSystemRequest> request = mRequestMap[aRequestId];
  if (aHasMore) {
    if (!request->mValue) {
      request->mValue = aValue;
    }
    else {
      request->mValue->Concat(aValue);
    }
    return NS_OK;
  }

  request->mIsCompleted = true;
  if (request->mValue) {
    request->mValue->Concat(aValue);
  }
  else {
    request->mValue = aValue;
  }
  printf_stderr("########################## Dump mRequestIdQueue before ##############\n");
  for (uint32_t i = 0; i < mRequestIdQueue.size(); i++) {
    printf_stderr("########################## mRequestIdQueue[%d] = %u\n", i, mRequestIdQueue[i]);
  }
  printf_stderr("########################## Dump mRequestIdQueue end\n");

  for (auto it = mRequestIdQueue.begin(); it != mRequestIdQueue.end();) {
    MOZ_ASSERT(mRequestMap.find(*it) != mRequestMap.end());

    RefPtr<nsVirtualFileSystemRequest> req = mRequestMap[*it];
    if (!req->mIsCompleted) {
      break;
    }

    nsCOMPtr<nsIRunnable> callback = new RunVirtualFileSystemSuccessCallback(req->mCallback,
                                                                        req->mRequestId,
                                                                        req->mValue,
                                                                        false);
    NS_DispatchToCurrentThread(callback);
    mRequestMap.erase(req->mRequestId);
    mRequestIdQueue.erase(it);
  }

  printf_stderr("########################## Dump mRequestIdQueue after ###############\n");
  for (uint32_t i = 0; i < mRequestIdQueue.size(); i++) {
    printf_stderr("########################## mRequestIdQueue[%d] = %u\n", i, mRequestIdQueue[i]);
  }
  printf_stderr("########################## Dump mRequestIdQueue end\n");
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemRequestManager::RejectRequest(uint32_t aRequestId, uint32_t aErrorCode)
{
  MonitorAutoLock lock(mMonitor);
  if (mRequestMap.find(aRequestId) == mRequestMap.end()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsVirtualFileSystemRequest> request = mRequestMap[aRequestId];
  nsCOMPtr<nsIRunnable> callback = new RunVirtualFileSystemErrorCallback(request->mCallback,
                                                                    aRequestId,
                                                                    aErrorCode);
  NS_DispatchToCurrentThread(callback);
  DestroyRequest(aRequestId);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemRequestManager::SetRequestDispatcher(
  nsIFileSystemProviderEventDispatcher* aDispatcher)
{
  mDispatcher = aDispatcher;
  return NS_OK;
}

void
nsVirtualFileSystemRequestManager::DestroyRequest(uint32_t aRequestId)
{
  mMonitor.AssertCurrentThreadOwns();

  mRequestMap.erase(aRequestId);
  auto it = std::find(mRequestIdQueue.begin(), mRequestIdQueue.end(), aRequestId);
  if (it != mRequestIdQueue.end()) {
    mRequestIdQueue.erase(it);
  }
}

} // end of namespace virtualfilesystem
} // end of namespace dom
} // end of namespace mozilla
