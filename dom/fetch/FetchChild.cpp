/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchChild.h"
#include "FetchLog.h"
#include "FetchObserver.h"
#include "FetchUtil.h"
#include "InternalResponse.h"
#include "Request.h"
#include "Response.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/SecurityPolicyViolationEventBinding.h"
#include "mozilla/dom/WorkerChannelInfo.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerScope.h"
#include "nsIAsyncInputStream.h"
#include "nsIGlobalObject.h"
#include "nsIObserverService.h"
#include "nsIRunnable.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS0(FetchChild)

mozilla::ipc::IPCResult FetchChild::Recv__delete__(const nsresult&& aResult) {
  FETCH_LOG(("FetchChild::Recv__delete__ [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  if (mWorkerRef) {
    MOZ_ASSERT(mWorkerRef->Private());
    mWorkerRef->Private()->AssertIsOnWorkerThread();
  } else {
    MOZ_ASSERT(mIsKeepAliveRequest);
  }

  if (mPromise->State() == Promise::PromiseState::Pending) {
    if (NS_FAILED(aResult)) {
      mPromise->MaybeReject(aResult);
      if (mFetchObserver) {
        mFetchObserver->SetState(FetchState::Errored);
      }
    } else {
      mPromise->MaybeResolve(aResult);
      if (mFetchObserver) {
        mFetchObserver->SetState(FetchState::Complete);
      }
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnResponseAvailableInternal(
    ParentToChildInternalResponse&& aResponse) {
  FETCH_LOG(("FetchChild::RecvOnResponseAvailableInternal [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  if (mWorkerRef) {
    MOZ_ASSERT(mWorkerRef->Private());
    mWorkerRef->Private()->AssertIsOnWorkerThread();
  }

  SafeRefPtr<InternalResponse> internalResponse =
      InternalResponse::FromIPC(aResponse);
  IgnoredErrorResult result;
  internalResponse->Headers()->SetGuard(HeadersGuardEnum::Immutable, result);
  MOZ_ASSERT(internalResponse);

  if (internalResponse->Type() != ResponseType::Error) {
    if (internalResponse->Type() == ResponseType::Opaque) {
      internalResponse->GeneratePaddingInfo();
    }

    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Complete);
    }

    // mFetchObserver->SetState runs JS and a blocking JS function can run
    // queued runnables, including ActorDestroy that nullifies mPromise.
    if (!mPromise) {
      return IPC_OK();
    }
    nsCOMPtr<nsIGlobalObject> global;
    global = mPromise->GetGlobalObject();
    RefPtr<Response> response =
        new Response(global, internalResponse.clonePtr(), mSignalImpl);
    mPromise->MaybeResolve(response);

    return IPC_OK();
  }

  FETCH_LOG(
      ("FetchChild::RecvOnResponseAvailableInternal [%p] response type is "
       "Error(0x%x)",
       this, static_cast<int32_t>(internalResponse->GetErrorCode())));
  if (mFetchObserver) {
    mFetchObserver->SetState(FetchState::Errored);
  }

  // mFetchObserver->SetState runs JS and a blocking JS function can run queued
  // runnables, including ActorDestroy that nullifies mPromise.
  if (!mPromise) {
    return IPC_OK();
  }
  mPromise->MaybeRejectWithTypeError<MSG_FETCH_FAILED>();
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnResponseEnd(ResponseEndArgs&& aArgs) {
  FETCH_LOG(("FetchChild::RecvOnResponseEnd [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  if (mWorkerRef) {
    MOZ_ASSERT(mWorkerRef->Private());
    mWorkerRef->Private()->AssertIsOnWorkerThread();
  }

  if (aArgs.endReason() == FetchDriverObserver::eAborted) {
    FETCH_LOG(
        ("FetchChild::RecvOnResponseEnd [%p] endReason is eAborted", this));
    if (mFetchObserver) {
      mFetchObserver->SetState(FetchState::Errored);
    }

    // mFetchObserver->SetState runs JS and a blocking JS function can run
    // queued runnables, including ActorDestroy that nullifies mPromise.
    if (!mPromise) {
      return IPC_OK();
    }
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  Unfollow();
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnDataAvailable() {
  FETCH_LOG(("FetchChild::RecvOnDataAvailable [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  if (mWorkerRef) {
    MOZ_ASSERT(mWorkerRef->Private());
    mWorkerRef->Private()->AssertIsOnWorkerThread();
  }

  if (mFetchObserver && mFetchObserver->State() == FetchState::Requesting) {
    mFetchObserver->SetState(FetchState::Responding);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnFlushConsoleReport(
    nsTArray<net::ConsoleReportCollected>&& aReports) {
  FETCH_LOG(("FetchChild::RecvOnFlushConsoleReport [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  MOZ_ASSERT(mReporter);

  if (NS_IsMainThread()) {
    MOZ_ASSERT(mIsKeepAliveRequest);
    // extract doc object to flush the console report
    for (const auto& report : aReports) {
      mReporter->AddConsoleReport(
          report.errorFlags(), report.category(),
          static_cast<nsContentUtils::PropertiesFile>(report.propertiesFile()),
          report.sourceFileURI(), report.lineNumber(), report.columnNumber(),
          report.messageName(), report.stringParams());
    }

    MOZ_ASSERT(mPromise);
    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(mPromise->GetGlobalObject());
    if (window) {
      Document* doc = window->GetExtantDoc();
      mReporter->FlushConsoleReports(doc);
    } else {
      mReporter->FlushReportsToConsole(0);
    }
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  if (mWorkerRef) {
    MOZ_ASSERT(mWorkerRef->Private());
    mWorkerRef->Private()->AssertIsOnWorkerThread();
  }

  RefPtr<ThreadSafeWorkerRef> workerRef = mWorkerRef;
  nsCOMPtr<nsIConsoleReportCollector> reporter = mReporter;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [reports = std::move(aReports), reporter = std::move(reporter),
                 workerRef = std::move(workerRef)]() mutable {
        for (const auto& report : reports) {
          reporter->AddConsoleReport(
              report.errorFlags(), report.category(),
              static_cast<nsContentUtils::PropertiesFile>(
                  report.propertiesFile()),
              report.sourceFileURI(), report.lineNumber(),
              report.columnNumber(), report.messageName(),
              report.stringParams());
        }

        if (workerRef->Private()->IsServiceWorker()) {
          reporter->FlushReportsToConsoleForServiceWorkerScope(
              workerRef->Private()->ServiceWorkerScope());
        }

        if (workerRef->Private()->IsSharedWorker()) {
          workerRef->Private()
              ->GetRemoteWorkerController()
              ->FlushReportsOnMainThread(reporter);
        }

        reporter->FlushConsoleReports(workerRef->Private()->GetLoadGroup());
      });
  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));

  return IPC_OK();
}

RefPtr<FetchChild> FetchChild::CreateForWorker(
    WorkerPrivate* aWorkerPrivate, RefPtr<Promise> aPromise,
    RefPtr<AbortSignalImpl> aSignalImpl, RefPtr<FetchObserver> aObserver) {
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  FETCH_LOG(("FetchChild::CreateForWorker [%p]", aWorkerPrivate));

  RefPtr<FetchChild> actor = MakeRefPtr<FetchChild>(
      std::move(aPromise), std::move(aSignalImpl), std::move(aObserver));

  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "FetchChild", [actor]() {
        FETCH_LOG(("StrongWorkerRef callback"));
        actor->Shutdown();
      });
  if (NS_WARN_IF(!workerRef)) {
    return nullptr;
  }

  actor->mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  if (NS_WARN_IF(!actor->mWorkerRef)) {
    return nullptr;
  }
  return actor;
}

RefPtr<FetchChild> FetchChild::CreateForMainThread(
    RefPtr<Promise> aPromise, RefPtr<AbortSignalImpl> aSignalImpl,
    RefPtr<FetchObserver> aObserver) {
  RefPtr<FetchChild> actor = MakeRefPtr<FetchChild>(
      std::move(aPromise), std::move(aSignalImpl), std::move(aObserver));
  FETCH_LOG(("FetchChild::CreateForMainThread actor[%p]", actor.get()));

  return actor;
}

mozilla::ipc::IPCResult FetchChild::RecvOnCSPViolationEvent(
    const nsAString& aJSON) {
  FETCH_LOG(("FetchChild::RecvOnCSPViolationEvent [%p] aJSON: %s\n", this,
             NS_ConvertUTF16toUTF8(aJSON).BeginReading()));

  nsString JSON(aJSON);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(__func__, [JSON]() mutable {
    SecurityPolicyViolationEventInit violationEventInit;
    if (NS_WARN_IF(!violationEventInit.Init(JSON))) {
      return;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv =
        NS_NewURI(getter_AddRefs(uri), violationEventInit.mBlockedURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService) {
      return;
    }

    rv = observerService->NotifyObservers(
        uri, CSP_VIOLATION_TOPIC, violationEventInit.mViolatedDirective.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  });
  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));

  if (mCSPEventListener) {
    Unused << NS_WARN_IF(
        NS_FAILED(mCSPEventListener->OnCSPViolationEvent(aJSON)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnReportPerformanceTiming(
    ResponseTiming&& aTiming) {
  FETCH_LOG(("FetchChild::RecvOnReportPerformanceTiming [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  if (mWorkerRef) {
    MOZ_ASSERT(mWorkerRef->Private());
    mWorkerRef->Private()->AssertIsOnWorkerThread();

    RefPtr<PerformanceStorage> performanceStorage =
        mWorkerRef->Private()->GetPerformanceStorage();
    if (performanceStorage) {
      performanceStorage->AddEntry(
          aTiming.entryName(), aTiming.initiatorType(),
          MakeUnique<PerformanceTimingData>(aTiming.timingData()));
    }
  } else if (mIsKeepAliveRequest) {
    MOZ_ASSERT(mPromise->GetGlobalObject());
    auto* innerWindow = mPromise->GetGlobalObject()->GetAsInnerWindow();
    if (innerWindow) {
      mozilla::dom::Performance* performance = innerWindow->GetPerformance();
      if (performance) {
        performance->AsPerformanceStorage()->AddEntry(
            aTiming.entryName(), aTiming.initiatorType(),
            MakeUnique<PerformanceTimingData>(aTiming.timingData()));
      }
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FetchChild::RecvOnNotifyNetworkMonitorAlternateStack(
    uint64_t aChannelID) {
  FETCH_LOG(
      ("FetchChild::RecvOnNotifyNetworkMonitorAlternateStack [%p]", this));
  if (mIsShutdown) {
    return IPC_OK();
  }
  // Shutdown has not been called, so mWorkerRef->Private() should be still
  // alive.
  if (mWorkerRef) {
    MOZ_ASSERT(mWorkerRef->Private());
    mWorkerRef->Private()->AssertIsOnWorkerThread();

    if (!mOriginStack) {
      return IPC_OK();
    }

    if (!mWorkerChannelInfo) {
      mWorkerChannelInfo = MakeRefPtr<WorkerChannelInfo>(
          aChannelID, mWorkerRef->Private()->AssociatedBrowsingContextID());
    }

    // Unfortunately, SerializedStackHolder can only be read on the main thread.
    // However, it doesn't block the fetch execution.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        __func__, [channel = mWorkerChannelInfo,
                   stack = std::move(mOriginStack)]() mutable {
          NotifyNetworkMonitorAlternateStack(channel, std::move(stack));
        });

    MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));
  }
  // Currently we only support sending notifications for worker-thread initiated
  // Fetch requests. We need to extend this to main-thread fetch requests as
  // well. See Bug 1897424.

  return IPC_OK();
}

void FetchChild::SetCSPEventListener(nsICSPEventListener* aListener) {
  MOZ_ASSERT(aListener && !mCSPEventListener);
  mCSPEventListener = aListener;
}

FetchChild::FetchChild(RefPtr<Promise>&& aPromise,
                       RefPtr<AbortSignalImpl>&& aSignalImpl,
                       RefPtr<FetchObserver>&& aObserver)
    : mPromise(std::move(aPromise)),
      mSignalImpl(std::move(aSignalImpl)),
      mFetchObserver(std::move(aObserver)),
      mReporter(new ConsoleReportCollector()) {
  FETCH_LOG(("FetchChild::FetchChild [%p]", this));
}

void FetchChild::RunAbortAlgorithm() {
  FETCH_LOG(("FetchChild::RunAbortAlgorithm [%p]", this));
  if (mIsShutdown) {
    return;
  }
  if (mWorkerRef || mIsKeepAliveRequest) {
    Unused << SendAbortFetchOp();
  }
}

void FetchChild::DoFetchOp(const FetchOpArgs& aArgs) {
  FETCH_LOG(("FetchChild::DoFetchOp [%p]", this));
  // we need to store this for keepalive request
  // as we need to update the load group during actor termination
  mIsKeepAliveRequest = aArgs.request().keepalive();
  if (mIsKeepAliveRequest) {
    mKeepaliveRequestSize =
        aArgs.request().bodySize() > 0 ? aArgs.request().bodySize() : 0;
  }
  if (mSignalImpl) {
    if (mSignalImpl->Aborted()) {
      Unused << SendAbortFetchOp();
      return;
    }
    Follow(mSignalImpl);
  }
  Unused << SendFetchOp(aArgs);
}

void FetchChild::Shutdown() {
  FETCH_LOG(("FetchChild::Shutdown [%p]", this));
  if (mIsShutdown) {
    return;
  }
  mIsShutdown.Flip();

  // If mWorkerRef is nullptr here, that means Recv__delete__() must be called
  if (!mWorkerRef) {
    return;
  }
  mPromise = nullptr;
  mFetchObserver = nullptr;
  Unfollow();
  mSignalImpl = nullptr;
  mCSPEventListener = nullptr;
  // TODO
  // For workers we need to skip aborting the fetch requests if keepalive is set
  // This is just a quick fix for Worker.
  // Usually, we want FetchChild to get destroyed while FetchParent calls
  // Senddelete(). When Worker shutdown, FetchChild must call
  // FetchChild::SendAbortFetchOp() to parent, and let FetchParent decide if
  // canceling the underlying fetch() or not. But currently, we have no good way
  // to distinguish whether the abort is intent by script or by Worker/Window
  // shutdown. So, we provide a quick fix here, which makes
  // FetchChild/FetchParent live a bit longer, but corresponding resources are
  // released in FetchChild::Shutdown(), so this quick fix should not cause any
  // leaking.
  // And we will fix it in Bug 1901082
  if (!mIsKeepAliveRequest) {
    Unused << SendAbortFetchOp();
  }

  mWorkerRef = nullptr;
}

void FetchChild::ActorDestroy(ActorDestroyReason aReason) {
  FETCH_LOG(("FetchChild::ActorDestroy [%p]", this));
  // for keepalive request decrement the pending keepalive count
  if (mIsKeepAliveRequest) {
    // we only support keepalive for main thread fetch requests
    // See Bug 1901759
    // For workers we need to dispatch a runnable to the main thread for
    // updating the loadgroup
    if (NS_IsMainThread()) {
      MOZ_ASSERT(mPromise->GetGlobalObject());
      nsCOMPtr<nsILoadGroup> loadGroup =
          FetchUtil::GetLoadGroupFromGlobal(mPromise->GetGlobalObject());
      if (loadGroup) {
        FetchUtil::DecrementPendingKeepaliveRequestSize(loadGroup,
                                                        mKeepaliveRequestSize);
      }
    }
  }
  mPromise = nullptr;
  mFetchObserver = nullptr;
  mSignalImpl = nullptr;
  mCSPEventListener = nullptr;
  mWorkerRef = nullptr;
}

}  // namespace mozilla::dom
