/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_GTK
#  include <gtk/gtk.h>
#  include <gdk/gdkx.h>
#endif

#include "ContentChild.h"

#include "GeckoProfiler.h"
#include "BrowserChild.h"
#include "HandlerServiceChild.h"

#include "mozilla/Attributes.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/BenchmarkStorageChild.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MemoryTelemetry.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/PerfStats.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessHangMonitorIPC.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/Unused.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TelemetryIPC.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/devtools/HeapSnapshotTempFileHelperChild.h"
#include "mozilla/docshell/OfflineCacheUpdateChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/BrowserBridgeHost.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientOpenWindowOpActors.h"
#include "mozilla/dom/ChildProcessMessageManager.h"
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ExternalHelperAppChild.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "mozilla/dom/IPCBlobInputStreamChild.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/JSWindowActorService.h"
#include "mozilla/dom/LSObject.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/dom/PLoginReputationChild.h"
#include "mozilla/dom/PSessionStorageObserverChild.h"
#include "mozilla/dom/PostMessageEvent.h"
#include "mozilla/dom/PushNotifier.h"
#include "mozilla/dom/RemoteWorkerService.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/dom/URLClassifierChild.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerDebuggerManager.h"
#include "mozilla/dom/ipc/SharedMap.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/psm/PSMContentListener.h"
#include "mozilla/hal_sandbox/PHalChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/FileDescriptorSetChild.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/PChildToParentStreamChild.h"
#include "mozilla/ipc/PParentToChildStreamChild.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/ipc/TestShellChild.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/jsipc/PJavaScript.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/ContentProcessController.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/SynchronousTask.h"  // for LaunchRDDProcess
#include "mozilla/loader/ScriptCacheActors.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/CaptivePortalService.h"
#include "mozilla/PerformanceMetricsCollector.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/plugins/PluginInstanceParent.h"
#include "mozilla/plugins/PluginModuleParent.h"
#include "mozilla/recordreplay/ParentIPC.h"
#include "mozilla/widget/ScreenManager.h"
#include "mozilla/widget/WidgetMessageUtils.h"
#include "nsBaseDragService.h"
#include "mozilla/media/MediaChild.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/WebBrowserPersistDocumentChild.h"
#include "mozilla/HangDetails.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/UnderrunHandler.h"
#include "nsIChildProcessChannelListener.h"
#include "mozilla/net/HttpChannelChild.h"
#include "nsQueryObject.h"
#include "imgLoader.h"
#include "GMPServiceChild.h"
#include "nsISimpleEnumerator.h"
#include "nsIStringBundle.h"
#include "nsIWorkerDebuggerManager.h"
#include "nsGeolocation.h"
#include "audio_thread_priority.h"
#include "nsIConsoleService.h"
#include "audio_thread_priority.h"

#if !defined(XP_WIN)
#  include "mozilla/Omnijar.h"
#endif

#ifdef MOZ_GECKO_PROFILER
#  include "ChildProfilerController.h"
#endif

#if defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#  if defined(XP_WIN)
#    include "mozilla/sandboxTarget.h"
#  elif defined(XP_LINUX)
#    include "mozilla/Sandbox.h"
#    include "mozilla/SandboxInfo.h"
#    include "CubebUtils.h"
#  elif defined(XP_MACOSX)
#    include "mozilla/Sandbox.h"
#  elif defined(__OpenBSD__)
#    include <unistd.h>
#  endif
#  if defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
#    include "mozilla/SandboxTestingChild.h"
#  endif
#endif

#include "mozilla/Unused.h"

#include "mozInlineSpellChecker.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIConsoleListener.h"
#include "nsIContentViewer.h"
#include "nsICycleCollectorListener.h"
#include "nsIIdlePeriod.h"
#include "nsIDragService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMemoryReporter.h"
#include "nsIMemoryInfoDumper.h"
#include "nsIMutable.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsMemoryInfoDumper.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleSheetService.h"
#include "nsVariant.h"
#include "nsXULAppAPI.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsJSEnvironment.h"
#include "SandboxHal.h"
#include "nsDebugImpl.h"
#include "nsHashPropertyBag.h"
#include "nsLayoutStylesheetCache.h"
#include "nsThreadManager.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsClipboardProxy.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsContentPermissionHelper.h"
#include "nsPluginHost.h"
#ifdef NS_PRINTING
#  include "nsPrintingProxy.h"
#endif
#include "nsWindowMemoryReporter.h"
#include "ReferrerInfo.h"

#include "IHistory.h"
#include "nsNetUtil.h"

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/task.h"

#include "nsChromeRegistryContent.h"
#include "nsFrameMessageManager.h"

#include "nsIGeolocationProvider.h"
#include "mozilla/dom/PCycleCollectWithLogsChild.h"

#include "nsIScriptSecurityManager.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"

#ifdef MOZ_WEBRTC
#  include "signaling/src/peerconnection/WebrtcGlobalChild.h"
#endif

#include "nsPermission.h"
#include "nsPermissionManager.h"

#include "PermissionMessageUtils.h"

#if defined(MOZ_WIDGET_ANDROID)
#  include "APKOpen.h"
#endif

#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#  include "mozilla/widget/AudioSession.h"
#  include "mozilla/audio/AudioNotificationReceiver.h"
#endif

#if defined(XP_MACOSX)
#  include "nsMacUtilsImpl.h"
#endif /* XP_MACOSX */

#ifdef MOZ_X11
#  include "mozilla/X11Util.h"
#endif

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#  ifdef XP_WIN
#    include "mozilla/a11y/AccessibleWrap.h"
#  endif
#  include "mozilla/a11y/DocAccessible.h"
#  include "mozilla/a11y/DocManager.h"
#  include "mozilla/a11y/OuterDocAccessible.h"
#endif

#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaController.h"
#include "mozilla/dom/PPresentationChild.h"
#include "mozilla/dom/PresentationIPCService.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/IPCStreamAlloc.h"
#include "mozilla/ipc/IPCStreamDestination.h"
#include "mozilla/ipc/IPCStreamSource.h"

#ifdef MOZ_WEBSPEECH
#  include "mozilla/dom/PSpeechSynthesisChild.h"
#endif

#include "ClearOnShutdown.h"
#include "ProcessUtils.h"
#include "URIUtils.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "DomainPolicy.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "mozilla/widget/PuppetBidiKeyboard.h"
#include "mozilla/RemoteSpellCheckEngineChild.h"
#include "GMPServiceChild.h"
#include "GfxInfoBase.h"
#include "gfxPlatform.h"
#include "gfxPlatformFontList.h"
#include "nscore.h"  // for NS_FREE_PERMANENT_DATA
#include "VRManagerChild.h"
#include "private/pprio.h"
#include "nsString.h"
#include "MMPrinter.h"

#ifdef MOZ_WIDGET_GTK
#  include "nsAppRunner.h"
#endif

#ifdef MOZ_CODE_COVERAGE
#  include "mozilla/CodeCoverageHandler.h"
#endif

using namespace mozilla;
using namespace mozilla::docshell;
using namespace mozilla::dom::ipc;
using namespace mozilla::media;
using namespace mozilla::embedding;
using namespace mozilla::gmp;
using namespace mozilla::hal_sandbox;
using namespace mozilla::ipc;
using namespace mozilla::intl;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::net;
using namespace mozilla::jsipc;
using namespace mozilla::psm;
using namespace mozilla::widget;
using mozilla::loader::PScriptCacheChild;

namespace mozilla {

namespace dom {

// IPC sender for remote GC/CC logging.
class CycleCollectWithLogsChild final : public PCycleCollectWithLogsChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(CycleCollectWithLogsChild)

  class Sink final : public nsICycleCollectorLogSink {
    NS_DECL_ISUPPORTS

    Sink(CycleCollectWithLogsChild* aActor, const FileDescriptor& aGCLog,
         const FileDescriptor& aCCLog) {
      mActor = aActor;
      mGCLog = FileDescriptorToFILE(aGCLog, "w");
      mCCLog = FileDescriptorToFILE(aCCLog, "w");
    }

    NS_IMETHOD Open(FILE** aGCLog, FILE** aCCLog) override {
      if (NS_WARN_IF(!mGCLog) || NS_WARN_IF(!mCCLog)) {
        return NS_ERROR_FAILURE;
      }
      *aGCLog = mGCLog;
      *aCCLog = mCCLog;
      return NS_OK;
    }

    NS_IMETHOD CloseGCLog() override {
      MOZ_ASSERT(mGCLog);
      fclose(mGCLog);
      mGCLog = nullptr;
      mActor->SendCloseGCLog();
      return NS_OK;
    }

    NS_IMETHOD CloseCCLog() override {
      MOZ_ASSERT(mCCLog);
      fclose(mCCLog);
      mCCLog = nullptr;
      mActor->SendCloseCCLog();
      return NS_OK;
    }

    NS_IMETHOD GetFilenameIdentifier(nsAString& aIdentifier) override {
      return UnimplementedProperty();
    }

    NS_IMETHOD SetFilenameIdentifier(const nsAString& aIdentifier) override {
      return UnimplementedProperty();
    }

    NS_IMETHOD GetProcessIdentifier(int32_t* aIdentifier) override {
      return UnimplementedProperty();
    }

    NS_IMETHOD SetProcessIdentifier(int32_t aIdentifier) override {
      return UnimplementedProperty();
    }

    NS_IMETHOD GetGcLog(nsIFile** aPath) override {
      return UnimplementedProperty();
    }

    NS_IMETHOD GetCcLog(nsIFile** aPath) override {
      return UnimplementedProperty();
    }

   private:
    ~Sink() {
      if (mGCLog) {
        fclose(mGCLog);
        mGCLog = nullptr;
      }
      if (mCCLog) {
        fclose(mCCLog);
        mCCLog = nullptr;
      }
      // The XPCOM refcount drives the IPC lifecycle;
      Unused << mActor->Send__delete__(mActor);
    }

    nsresult UnimplementedProperty() {
      MOZ_ASSERT(false,
                 "This object is a remote GC/CC logger;"
                 " this property isn't meaningful.");
      return NS_ERROR_UNEXPECTED;
    }

    RefPtr<CycleCollectWithLogsChild> mActor;
    FILE* mGCLog;
    FILE* mCCLog;
  };

 private:
  ~CycleCollectWithLogsChild() {}
};

NS_IMPL_ISUPPORTS(CycleCollectWithLogsChild::Sink, nsICycleCollectorLogSink);

class AlertObserver {
 public:
  AlertObserver(nsIObserver* aObserver, const nsString& aData)
      : mObserver(aObserver), mData(aData) {}

  ~AlertObserver() = default;

  bool ShouldRemoveFrom(nsIObserver* aObserver, const nsString& aData) const {
    return (mObserver == aObserver && mData == aData);
  }

  bool Observes(const nsString& aData) const { return mData.Equals(aData); }

  bool Notify(const nsCString& aType) const {
    mObserver->Observe(nullptr, aType.get(), mData.get());
    return true;
  }

 private:
  nsCOMPtr<nsIObserver> mObserver;
  nsString mData;
};

class ConsoleListener final : public nsIConsoleListener {
 public:
  explicit ConsoleListener(ContentChild* aChild) : mChild(aChild) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONSOLELISTENER

 private:
  ~ConsoleListener() = default;

  ContentChild* mChild;
  friend class ContentChild;
};

NS_IMPL_ISUPPORTS(ConsoleListener, nsIConsoleListener)

// Before we send the error to the parent process (which
// involves copying the memory), truncate any long lines.  CSS
// errors in particular share the memory for long lines with
// repeated errors, but the IPC communication we're about to do
// will break that sharing, so we better truncate now.
static void TruncateString(nsAString& aString) {
  if (aString.Length() > 1000) {
    aString.Truncate(1000);
  }
}

NS_IMETHODIMP
ConsoleListener::Observe(nsIConsoleMessage* aMessage) {
  if (!mChild) {
    return NS_OK;
  }

  nsCOMPtr<nsIScriptError> scriptError = do_QueryInterface(aMessage);
  if (scriptError) {
    nsAutoString msg, sourceName, sourceLine;
    nsCString category;
    uint32_t lineNum, colNum, flags;
    bool fromPrivateWindow, fromChromeContext;

    nsresult rv = scriptError->GetErrorMessage(msg);
    NS_ENSURE_SUCCESS(rv, rv);
    TruncateString(msg);
    rv = scriptError->GetSourceName(sourceName);
    NS_ENSURE_SUCCESS(rv, rv);
    TruncateString(sourceName);
    rv = scriptError->GetSourceLine(sourceLine);
    NS_ENSURE_SUCCESS(rv, rv);
    TruncateString(sourceLine);

    rv = scriptError->GetCategory(getter_Copies(category));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetLineNumber(&lineNum);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetColumnNumber(&colNum);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetFlags(&flags);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetIsFromPrivateWindow(&fromPrivateWindow);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = scriptError->GetIsFromChromeContext(&fromChromeContext);
    NS_ENSURE_SUCCESS(rv, rv);

    {
      AutoJSAPI jsapi;
      jsapi.Init();
      JSContext* cx = jsapi.cx();

      JS::RootedValue stack(cx);
      rv = scriptError->GetStack(&stack);
      NS_ENSURE_SUCCESS(rv, rv);

      if (stack.isObject()) {
        // Because |stack| might be a cross-compartment wrapper, we can't use it
        // with JSAutoRealm. Use the stackGlobal for that.
        JS::RootedValue stackGlobal(cx);
        rv = scriptError->GetStackGlobal(&stackGlobal);
        NS_ENSURE_SUCCESS(rv, rv);

        JSAutoRealm ar(cx, &stackGlobal.toObject());

        StructuredCloneData data;
        ErrorResult err;
        data.Write(cx, stack, err);
        if (err.Failed()) {
          return err.StealNSResult();
        }

        ClonedMessageData cloned;
        if (!data.BuildClonedMessageDataForChild(mChild, cloned)) {
          return NS_ERROR_FAILURE;
        }

        mChild->SendScriptErrorWithStack(
            msg, sourceName, sourceLine, lineNum, colNum, flags, category,
            fromPrivateWindow, fromChromeContext, cloned);
        return NS_OK;
      }
    }

    mChild->SendScriptError(msg, sourceName, sourceLine, lineNum, colNum, flags,
                            category, fromPrivateWindow, 0, fromChromeContext);
    return NS_OK;
  }

  nsString msg;
  nsresult rv = aMessage->GetMessageMoz(msg);
  NS_ENSURE_SUCCESS(rv, rv);
  mChild->SendConsoleMessage(msg);
  return NS_OK;
}

#ifdef NIGHTLY_BUILD
/**
 * The singleton of this class is registered with the BackgroundHangMonitor as
 * an annotator, so that the hang monitor can record whether or not there were
 * pending input events when the thread hung.
 */
class PendingInputEventHangAnnotator final : public BackgroundHangAnnotator {
 public:
  virtual void AnnotateHang(BackgroundHangAnnotations& aAnnotations) override {
    int32_t pending = ContentChild::GetSingleton()->GetPendingInputEvents();
    if (pending > 0) {
      aAnnotations.AddAnnotation(NS_LITERAL_STRING("PendingInput"), pending);
    }
  }

  static PendingInputEventHangAnnotator sSingleton;
};
PendingInputEventHangAnnotator PendingInputEventHangAnnotator::sSingleton;
#endif

class ContentChild::ShutdownCanary final {};

ContentChild* ContentChild::sSingleton;
StaticAutoPtr<ContentChild::ShutdownCanary> ContentChild::sShutdownCanary;

ContentChild::ContentChild()
    : mID(uint64_t(-1))
#if defined(XP_WIN) && defined(ACCESSIBILITY)
      ,
      mMainChromeTid(0),
      mMsaaID(0)
#endif
      ,
      mIsForBrowser(false),
      mIsAlive(true),
      mShuttingDown(false) {
  // This process is a content process, so it's clearly running in
  // multiprocess mode!
  nsDebugImpl::SetMultiprocessMode("Child");

  // When ContentChild is created, the observer service does not even exist.
  // When ContentChild::RecvSetXPCOMProcessAttributes is called (the first
  // IPDL call made on this object), shutdown may have already happened. Thus
  // we create a canary here that relies upon getting cleared if shutdown
  // happens without requiring the observer service at this time.
  if (!sShutdownCanary) {
    sShutdownCanary = new ShutdownCanary();
    ClearOnShutdown(&sShutdownCanary, ShutdownPhase::Shutdown);
  }
}

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(                                                  \
      disable : 4722) /* Silence "destructor never returns" warning \
                       */
#endif

ContentChild::~ContentChild() {
#ifndef NS_FREE_PERMANENT_DATA
  MOZ_CRASH("Content Child shouldn't be destroyed.");
#endif
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

NS_INTERFACE_MAP_BEGIN(ContentChild)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

mozilla::ipc::IPCResult ContentChild::RecvSetXPCOMProcessAttributes(
    const XPCOMInitData& aXPCOMInit, const StructuredCloneData& aInitialData,
    nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache,
    nsTArray<SystemFontListEntry>&& aFontList,
    const Maybe<SharedMemoryHandle>& aSharedUASheetHandle,
    const uintptr_t& aSharedUASheetAddress) {
  if (!sShutdownCanary) {
    return IPC_OK();
  }

  mLookAndFeelCache = std::move(aLookAndFeelIntCache);
  mFontList = std::move(aFontList);
  gfx::gfxVars::SetValuesForInitialize(aXPCOMInit.gfxNonDefaultVarUpdates());
  InitSharedUASheets(aSharedUASheetHandle, aSharedUASheetAddress);
  InitXPCOM(aXPCOMInit, aInitialData);
  InitGraphicsDeviceData(aXPCOMInit.contentDeviceData());

  return IPC_OK();
}

bool ContentChild::Init(MessageLoop* aIOLoop, base::ProcessId aParentPid,
                        const char* aParentBuildID, IPC::Channel* aChannel,
                        uint64_t aChildID, bool aIsForBrowser) {
#ifdef MOZ_WIDGET_GTK
  // When running X11 only build we need to pass a display down
  // to gtk_init because it's not going to use the one from the environment
  // on its own when deciding which backend to use, and when starting under
  // XWayland, it may choose to start with the wayland backend
  // instead of the x11 backend.
  // The DISPLAY environment variable is normally set by the parent process.
  // The MOZ_GDK_DISPLAY environment variable is set from nsAppRunner.cpp
  // when --display is set by the command line.
  if (!gfxPlatform::IsHeadless()) {
    const char* display_name = PR_GetEnv("MOZ_GDK_DISPLAY");
    if (!display_name) {
      bool waylandDisabled = true;
#  ifdef MOZ_WAYLAND
      waylandDisabled = IsWaylandDisabled();
#  endif
      if (waylandDisabled) {
        display_name = PR_GetEnv("DISPLAY");
      }
    }
    if (display_name) {
      int argc = 3;
      char option_name[] = "--display";
      char* argv[] = {
          // argv0 is unused because g_set_prgname() was called in
          // XRE_InitChildProcess().
          nullptr, option_name, const_cast<char*>(display_name), nullptr};
      char** argvp = argv;
      gtk_init(&argc, &argvp);
    } else {
      gtk_init(nullptr, nullptr);
    }
  }
#endif

#ifdef MOZ_X11
  if (!gfxPlatform::IsHeadless()) {
    // Do this after initializing GDK, or GDK will install its own handler.
    XRE_InstallX11ErrorHandler();
  }
#endif

  NS_ASSERTION(!sSingleton, "only one ContentChild per child");

  // Once we start sending IPC messages, we need the thread manager to be
  // initialized so we can deal with the responses. Do that here before we
  // try to construct the crash reporter.
  nsresult rv = nsThreadManager::get().Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Middleman processes use a special channel for forwarding messages to
  // their own children.
  if (recordreplay::IsMiddleman()) {
    SetMiddlemanIPCChannel(recordreplay::parent::ChannelToUIProcess());

    // Eagerly mark this child as connected, as using another IPC channel will
    // cause that channel's protocol to be marked as connected instead and
    // prevent this one from being able to send IPDL messages.
    ActorConnected();
  }

  if (!Open(aChannel, aParentPid, aIOLoop)) {
    return false;
  }
  sSingleton = this;

  // If communications with the parent have broken down, take the process
  // down so it's not hanging around.
  GetIPCChannel()->SetAbortOnError(true);
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  GetIPCChannel()->SetChannelFlags(MessageChannel::REQUIRE_A11Y_REENTRY);
#endif

  // This must be checked before any IPDL message, which may hit sentinel
  // errors due to parent and content processes having different
  // versions.
  MessageChannel* channel = GetIPCChannel();
  if (channel && !channel->SendBuildIDsMatchMessage(aParentBuildID)) {
    // We need to quit this process if the buildID doesn't match the parent's.
    // This can occur when an update occurred in the background.
    ProcessChild::QuickExit();
  }

#ifdef MOZ_X11
#  ifdef MOZ_WIDGET_GTK
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default()) &&
      !gfxPlatform::IsHeadless()) {
    // Send the parent our X socket to act as a proxy reference for our X
    // resources.
    int xSocketFd = ConnectionNumber(DefaultXDisplay());
    SendBackUpXResources(FileDescriptor(xSocketFd));
  }
#  endif
#endif

  CrashReporterClient::InitSingleton(this);

  mID = aChildID;
  mIsForBrowser = aIsForBrowser;

#ifdef NS_PRINTING
  // Force the creation of the nsPrintingProxy so that it's IPC counterpart,
  // PrintingParent, is always available for printing initiated from the parent.
  RefPtr<nsPrintingProxy> printingProxy = nsPrintingProxy::GetInstance();
#endif

  SetProcessName(NS_LITERAL_STRING("Web Content"));

#ifdef NIGHTLY_BUILD
  // NOTE: We have to register the annotator on the main thread, as annotators
  // only affect a single thread.
  SystemGroup::Dispatch(
      TaskCategory::Other,
      NS_NewRunnableFunction("RegisterPendingInputEventHangAnnotator", [] {
        BackgroundHangMonitor::RegisterAnnotator(
            PendingInputEventHangAnnotator::sSingleton);
      }));
#endif

  return true;
}

void ContentChild::SetProcessName(const nsAString& aName) {
  char* name;
  if ((name = PR_GetEnv("MOZ_DEBUG_APP_PROCESS")) && aName.EqualsASCII(name)) {
#ifdef OS_POSIX
    printf_stderr("\n\nCHILDCHILDCHILDCHILD\n  [%s] debug me @%d\n\n", name,
                  getpid());
    sleep(30);
#elif defined(OS_WIN)
    // Windows has a decent JIT debugging story, so NS_DebugBreak does the
    // right thing.
    NS_DebugBreak(NS_DEBUG_BREAK,
                  "Invoking NS_DebugBreak() to debug child process", nullptr,
                  __FILE__, __LINE__);
#endif
  }

  mProcessName = aName;
  NS_LossyConvertUTF16toASCII asciiName(aName);
  mozilla::ipc::SetThisProcessName(asciiName.get());
#ifdef MOZ_GECKO_PROFILER
  profiler_set_process_name(asciiName);
#endif
}

NS_IMETHODIMP
ContentChild::ProvideWindow(mozIDOMWindowProxy* aParent, uint32_t aChromeFlags,
                            bool aCalledFromJS, bool aPositionSpecified,
                            bool aSizeSpecified, nsIURI* aURI,
                            const nsAString& aName, const nsACString& aFeatures,
                            bool aForceNoOpener, bool aForceNoReferrer,
                            nsDocShellLoadState* aLoadState, bool* aWindowIsNew,
                            BrowsingContext** aReturn) {
  return ProvideWindowCommon(
      nullptr, aParent, false, aChromeFlags, aCalledFromJS, aPositionSpecified,
      aSizeSpecified, aURI, aName, aFeatures, aForceNoOpener, aForceNoReferrer,
      aLoadState, aWindowIsNew, aReturn);
}

static nsresult GetCreateWindowParams(mozIDOMWindowProxy* aParent,
                                      nsDocShellLoadState* aLoadState,
                                      bool aForceNoReferrer, float* aFullZoom,
                                      nsIReferrerInfo** aReferrerInfo,
                                      nsIPrincipal** aTriggeringPrincipal,
                                      nsIContentSecurityPolicy** aCsp) {
  *aFullZoom = 1.0f;
  if (!aTriggeringPrincipal || !aCsp) {
    NS_ERROR("aTriggeringPrincipal || aCsp is null");
    return NS_ERROR_FAILURE;
  }

  if (!aReferrerInfo) {
    NS_ERROR("aReferrerInfo is null");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  if (aForceNoReferrer) {
    referrerInfo = new ReferrerInfo(nullptr, ReferrerPolicy::_empty, false);
  }
  if (aLoadState && !referrerInfo) {
    referrerInfo = aLoadState->GetReferrerInfo();
  }

  auto* opener = nsPIDOMWindowOuter::From(aParent);
  if (!opener) {
    nsCOMPtr<nsIPrincipal> nullPrincipal =
        NullPrincipal::CreateWithoutOriginAttributes();
    if (!referrerInfo) {
      referrerInfo = new ReferrerInfo(nullptr, ReferrerPolicy::_empty);
    }

    referrerInfo.swap(*aReferrerInfo);
    NS_ADDREF(*aTriggeringPrincipal = nullPrincipal);
    return NS_OK;
  }

  nsCOMPtr<Document> doc = opener->GetDoc();
  NS_ADDREF(*aTriggeringPrincipal = doc->NodePrincipal());

  nsCOMPtr<nsIContentSecurityPolicy> csp = doc->GetCsp();
  if (csp) {
    csp.forget(aCsp);
  }

  nsCOMPtr<nsIURI> baseURI = doc->GetDocBaseURI();
  if (!baseURI) {
    NS_ERROR("Document didn't return a base URI");
    return NS_ERROR_FAILURE;
  }

  if (!referrerInfo) {
    referrerInfo = new ReferrerInfo();
    referrerInfo->InitWithDocument(doc);
  }

  referrerInfo.swap(*aReferrerInfo);

  RefPtr<nsDocShell> openerDocShell =
      static_cast<nsDocShell*>(opener->GetDocShell());
  if (!openerDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIContentViewer> cv;
  nsresult rv = openerDocShell->GetContentViewer(getter_AddRefs(cv));
  if (NS_SUCCEEDED(rv) && cv) {
    cv->GetFullZoom(aFullZoom);
  }

  return NS_OK;
}

nsresult ContentChild::ProvideWindowCommon(
    BrowserChild* aTabOpener, mozIDOMWindowProxy* aParent, bool aIframeMoz,
    uint32_t aChromeFlags, bool aCalledFromJS, bool aPositionSpecified,
    bool aSizeSpecified, nsIURI* aURI, const nsAString& aName,
    const nsACString& aFeatures, bool aForceNoOpener, bool aForceNoReferrer,
    nsDocShellLoadState* aLoadState, bool* aWindowIsNew,
    BrowsingContext** aReturn) {
  *aReturn = nullptr;

  nsAutoPtr<IPCTabContext> ipcContext;
  TabId openerTabId = TabId(0);
  nsAutoCString features(aFeatures);
  nsAutoString name(aName);

  nsresult rv;

  MOZ_ASSERT(!aParent || aTabOpener,
             "If aParent is non-null, we should have an aTabOpener");

  // Cache the boolean preference for allowing noopener windows to open in a
  // separate process.
  static bool sNoopenerNewProcess = false;
  static bool sNoopenerNewProcessInited = false;
  if (!sNoopenerNewProcessInited) {
    Preferences::AddBoolVarCache(&sNoopenerNewProcess,
                                 "dom.noopener.newprocess.enabled");
    sNoopenerNewProcessInited = true;
  }

  bool useRemoteSubframes =
      aChromeFlags & nsIWebBrowserChrome::CHROME_FISSION_WINDOW;

  // Check if we should load in a different process. Under Fission, we never
  // want to do this, since the Fission process selection logic will handle
  // everything for us. Outside of Fission, we always want to load in a
  // different process if we have noopener set, but we also might if we can't
  // load in the current process.
  bool loadInDifferentProcess =
      aForceNoOpener && sNoopenerNewProcess && !useRemoteSubframes;
  if (aTabOpener && !loadInDifferentProcess && aURI) {
    nsCOMPtr<nsILoadContext> context;
    if (aParent) {
      context = do_GetInterface(aTabOpener->WebNavigation());
    }
    // Only special-case cross-process loads if Fission is disabled. With
    // Fission enabled, the initial in-process load will automatically be
    // retargeted to the correct process.
    if (!(context && context->UseRemoteSubframes())) {
      nsCOMPtr<nsIWebBrowserChrome3> browserChrome3;
      rv = aTabOpener->GetWebBrowserChrome(getter_AddRefs(browserChrome3));
      if (NS_SUCCEEDED(rv) && browserChrome3) {
        bool shouldLoad;
        rv = browserChrome3->ShouldLoadURIInThisProcess(aURI, &shouldLoad);
        loadInDifferentProcess = NS_SUCCEEDED(rv) && !shouldLoad;
      }
    }
  }

  // If we're in a content process and we have noopener set, there's no reason
  // to load in our process, so let's load it elsewhere!
  if (loadInDifferentProcess) {
    float fullZoom;
    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    nsCOMPtr<nsIReferrerInfo> referrerInfo;
    rv = GetCreateWindowParams(aParent, aLoadState, aForceNoReferrer, &fullZoom,
                               getter_AddRefs(referrerInfo),
                               getter_AddRefs(triggeringPrincipal),
                               getter_AddRefs(csp));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    Maybe<URIParams> uriToLoad;
    SerializeURI(aURI, uriToLoad);

    if (name.LowerCaseEqualsLiteral("_blank")) {
      name = EmptyString();
    }

    MOZ_DIAGNOSTIC_ASSERT(!nsContentUtils::IsSpecialName(name));

    Unused << SendCreateWindowInDifferentProcess(
        aTabOpener, aChromeFlags, aCalledFromJS, aPositionSpecified,
        aSizeSpecified, uriToLoad, features, fullZoom, name,
        triggeringPrincipal, csp, referrerInfo);

    // We return NS_ERROR_ABORT, so that the caller knows that we've abandoned
    // the window open as far as it is concerned.
    return NS_ERROR_ABORT;
  }

  if (aTabOpener) {
    PopupIPCTabContext context;
    openerTabId = aTabOpener->GetTabId();
    context.opener() = openerTabId;
    context.isMozBrowserElement() = aTabOpener->IsMozBrowserElement();
    ipcContext = new IPCTabContext(context);
  } else {
    // It's possible to not have a BrowserChild opener in the case
    // of ServiceWorker::OpenWindow.
    UnsafeIPCTabContext unsafeTabContext;
    ipcContext = new IPCTabContext(unsafeTabContext);
  }

  MOZ_ASSERT(ipcContext);
  TabId tabId(nsContentUtils::GenerateTabId());

  // We need to assign a TabGroup to the PBrowser actor before we send it to the
  // parent. Otherwise, the parent could send messages to us before we have a
  // proper TabGroup for that actor.
  RefPtr<TabGroup> tabGroup;
  RefPtr<BrowsingContext> openerBC;
  if (aTabOpener && !aForceNoOpener) {
    // The new actor will use the same tab group as the opener.
    tabGroup = aTabOpener->TabGroup();
    if (aParent) {
      openerBC = nsPIDOMWindowOuter::From(aParent)->GetBrowsingContext();
    }
  } else {
    tabGroup = new TabGroup();
  }

  RefPtr<BrowsingContext> browsingContext = BrowsingContext::Create(
      nullptr, openerBC, aName, BrowsingContext::Type::Content);

  TabContext newTabContext = aTabOpener ? *aTabOpener : TabContext();

  // The initial about:blank document we generate within the nsDocShell will
  // almost certainly be replaced at some point. Unfortunately, getting the
  // principal right here causes bugs due to frame scripts not getting events
  // they expect, due to the real initial about:blank not being created yet.
  //
  // For this reason, we intentionally mispredict the initial principal here, so
  // that we can act the same as we did before when not predicting a result
  // principal. This `PWindowGlobal` will almost immediately be destroyed.
  nsCOMPtr<nsIPrincipal> initialPrincipal =
      NullPrincipal::Create(newTabContext.OriginAttributesRef());
  WindowGlobalInit windowInit = WindowGlobalActor::AboutBlankInitializer(
      browsingContext, initialPrincipal);

  auto windowChild = MakeRefPtr<WindowGlobalChild>(windowInit, nullptr);

  auto newChild = MakeRefPtr<BrowserChild>(this, tabId, tabGroup, newTabContext,
                                           browsingContext, aChromeFlags,
                                           /* aIsTopLevel */ true);

  if (aTabOpener) {
    MOZ_ASSERT(ipcContext->type() == IPCTabContext::TPopupIPCTabContext);
    ipcContext->get_PopupIPCTabContext().opener() = aTabOpener;
  }

  nsCOMPtr<nsIEventTarget> target =
      tabGroup->EventTargetFor(TaskCategory::Other);
  SetEventTargetForActor(newChild, target);

  if (IsShuttingDown()) {
    return NS_ERROR_ABORT;
  }

  // Open a remote endpoint for our PBrowser actor.
  ManagedEndpoint<PBrowserParent> parentEp = OpenPBrowserEndpoint(newChild);
  if (NS_WARN_IF(!parentEp.IsValid())) {
    return NS_ERROR_ABORT;
  }

  // Open a remote endpoint for our PWindowGlobal actor.
  ManagedEndpoint<PWindowGlobalParent> windowParentEp =
      newChild->OpenPWindowGlobalEndpoint(windowChild);
  if (NS_WARN_IF(!windowParentEp.IsValid())) {
    return NS_ERROR_ABORT;
  }

  // Tell the parent process to set up its PBrowserParent.
  if (NS_WARN_IF(!SendConstructPopupBrowser(
          std::move(parentEp), std::move(windowParentEp), tabId, *ipcContext,
          windowInit, aChromeFlags))) {
    return NS_ERROR_ABORT;
  }

  windowChild->Init();

  // Now that |newChild| has had its IPC link established, call |Init| to set it
  // up.
  if (NS_FAILED(newChild->Init(aParent, windowChild))) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsPIDOMWindowInner> parentTopInnerWindow;
  if (aParent) {
    nsCOMPtr<nsPIDOMWindowOuter> parentTopWindow =
        nsPIDOMWindowOuter::From(aParent)->GetInProcessTop();
    if (parentTopWindow) {
      parentTopInnerWindow = parentTopWindow->GetCurrentInnerWindow();
    }
  }

  // Set to true when we're ready to return from this function.
  bool ready = false;

  // NOTE: Capturing by reference here is safe, as this function won't return
  // until one of these callbacks is called.
  auto resolve = [&](const CreatedWindowInfo& info) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    rv = info.rv();
    *aWindowIsNew = info.windowOpened();
    nsTArray<FrameScriptInfo> frameScripts(info.frameScripts());
    nsCString urlToLoad = info.urlToLoad();
    uint32_t maxTouchPoints = info.maxTouchPoints();
    DimensionInfo dimensionInfo = info.dimensions();
    bool hasSiblings = info.hasSiblings();

    // Once this function exits, we should try to exit the nested event loop.
    ready = true;

    // NOTE: We have to handle this immediately in the resolve callback in order
    // to make sure that we don't process any more IPC messages before returning
    // from ProvideWindowCommon.

    // Handle the error which we got back from the parent process, if we got
    // one.
    if (NS_FAILED(rv)) {
      return;
    }

    if (!*aWindowIsNew) {
      rv = NS_ERROR_ABORT;
      return;
    }

    // If the BrowserChild has been torn down, we don't need to do this anymore.
    if (NS_WARN_IF(!newChild->IPCOpen() || newChild->IsDestroyed())) {
      rv = NS_ERROR_ABORT;
      return;
    }

    ShowInfo showInfo(EmptyString(), false, false, true, false, 0, 0, 0);
    auto* opener = nsPIDOMWindowOuter::From(aParent);
    nsIDocShell* openerShell;
    if (opener && (openerShell = opener->GetDocShell())) {
      nsCOMPtr<nsILoadContext> context = do_QueryInterface(openerShell);
      showInfo = ShowInfo(EmptyString(), false, context->UsePrivateBrowsing(),
                          true, false, aTabOpener->WebWidget()->GetDPI(),
                          aTabOpener->WebWidget()->RoundsWidgetCoordinatesTo(),
                          aTabOpener->WebWidget()->GetDefaultScale().scale);
    }

    newChild->SetMaxTouchPoints(maxTouchPoints);
    newChild->SetHasSiblings(hasSiblings);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    if (nsCOMPtr<nsPIDOMWindowOuter> outer =
            do_GetInterface(newChild->WebNavigation())) {
      BrowsingContext* bc = outer->GetBrowsingContext();
      auto parentBC =
          aParent
              ? nsPIDOMWindowOuter::From(aParent)->GetBrowsingContext()->Id()
              : 0;

      if (aForceNoOpener) {
        MOZ_DIAGNOSTIC_ASSERT(!*aWindowIsNew || !bc->HadOriginalOpener());
        MOZ_DIAGNOSTIC_ASSERT(bc->GetOpenerId() == 0);
      } else {
        MOZ_DIAGNOSTIC_ASSERT(!*aWindowIsNew ||
                              bc->HadOriginalOpener() == !!parentBC);
        MOZ_DIAGNOSTIC_ASSERT(bc->GetOpenerId() == parentBC);
      }
    }
#endif

    // Unfortunately we don't get a window unless we've shown the frame.  That's
    // pretty bogus; see bug 763602.
    newChild->DoFakeShow(showInfo);

    newChild->RecvUpdateDimensions(dimensionInfo);

    for (size_t i = 0; i < frameScripts.Length(); i++) {
      FrameScriptInfo& info = frameScripts[i];
      if (!newChild->RecvLoadRemoteScript(info.url(),
                                          info.runInGlobalScope())) {
        MOZ_CRASH();
      }
    }

    if (xpc::IsInAutomation()) {
      if (nsCOMPtr<nsPIDOMWindowOuter> outer =
              do_GetInterface(newChild->WebNavigation())) {
        nsCOMPtr<nsIObserverService> obs(services::GetObserverService());
        obs->NotifyObservers(
            outer, "dangerous:test-only:new-browser-child-ready", nullptr);
      }
    }

    browsingContext.forget(aReturn);
  };

  // NOTE: Capturing by reference here is safe, as this function won't return
  // until one of these callbacks is called.
  auto reject = [&](ResponseRejectReason) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    NS_WARNING("windowCreated promise rejected");
    rv = NS_ERROR_NOT_AVAILABLE;
    ready = true;
  };

  // Send down the request to open the window.
  if (aIframeMoz) {
    MOZ_ASSERT(aTabOpener);
    nsAutoCString url;
    if (aURI) {
      aURI->GetSpec(url);
    } else {
      // We can't actually send a nullptr up as the URI, since IPDL doesn't let
      // us send nullptr's for primitives. We indicate that the nsString for the
      // URI should be converted to a nullptr by voiding the string.
      url.SetIsVoid(true);
    }

    // NOTE: BrowserFrameOpenWindowPromise is the same type as
    // CreateWindowPromise, and this code depends on that fact.
    newChild->SendBrowserFrameOpenWindow(
        aTabOpener, NS_ConvertUTF8toUTF16(url), name, aForceNoReferrer,
        NS_ConvertUTF8toUTF16(features), std::move(resolve), std::move(reject));
  } else {
    float fullZoom;
    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    nsCOMPtr<nsIReferrerInfo> referrerInfo;
    rv = GetCreateWindowParams(aParent, aLoadState, aForceNoReferrer, &fullZoom,
                               getter_AddRefs(referrerInfo),
                               getter_AddRefs(triggeringPrincipal),
                               getter_AddRefs(csp));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    Maybe<URIParams> uriToLoad;
    if (aURI) {
      SerializeURI(aURI, uriToLoad);
    }

    SendCreateWindow(aTabOpener, newChild, aChromeFlags, aCalledFromJS,
                     aPositionSpecified, aSizeSpecified, uriToLoad, features,
                     fullZoom, Principal(triggeringPrincipal), csp,
                     referrerInfo, std::move(resolve), std::move(reject));
  }

  // =======================
  // Begin Nested Event Loop
  // =======================

  // We have to wait for a response from either SendCreateWindow or
  // SendBrowserFrameOpenWindow with information we're going to need to return
  // from this function, So we spin a nested event loop until they get back to
  // us.

  // Prevent the docshell from becoming active while the nested event loop is
  // spinning.
  newChild->AddPendingDocShellBlocker();
  auto removePendingDocShellBlocker = MakeScopeExit([&] {
    if (newChild) {
      newChild->RemovePendingDocShellBlocker();
    }
  });

  // Suspend our window if we have one to make sure we don't re-enter it.
  if (parentTopInnerWindow) {
    parentTopInnerWindow->Suspend();
  }

  {
    AutoNoJSAPI nojsapi;

    // Spin the event loop until we get a response. Callers of this function
    // already have to guard against an inner event loop spinning in the
    // non-e10s case because of the need to spin one to create a new chrome
    // window.
    SpinEventLoopUntil([&]() { return ready; });
    MOZ_RELEASE_ASSERT(ready,
                       "We are on the main thread, so we should not exit this "
                       "loop without ready being true.");
  }

  if (parentTopInnerWindow) {
    parentTopInnerWindow->Resume();
  }

  // =====================
  // End Nested Event Loop
  // =====================

  // It's possible for our new BrowsingContext to become discarded during the
  // nested event loop, in which case we shouldn't return it, since our callers
  // will generally not be prepared to deal with that.
  if (*aReturn && (*aReturn)->IsDiscarded()) {
    NS_RELEASE(*aReturn);
    return NS_ERROR_ABORT;
  }

  // We should have the results already set by the callbacks.
  MOZ_ASSERT_IF(NS_SUCCEEDED(rv), *aReturn);
  return rv;
}

void ContentChild::GetProcessName(nsAString& aName) const {
  aName.Assign(mProcessName);
}

void ContentChild::LaunchRDDProcess() {
  SynchronousTask task("LaunchRDDProcess");
  SystemGroup::Dispatch(
      TaskCategory::Other,
      NS_NewRunnableFunction("LaunchRDDProcess", [&task, this] {
        AutoCompleteTask complete(&task);
        nsresult rv;
        Endpoint<PRemoteDecoderManagerChild> endpoint;
        Unused << SendLaunchRDDProcess(&rv, &endpoint);
        if (rv == NS_OK) {
          RemoteDecoderManagerChild::InitForRDDProcess(std::move(endpoint));
        }
      }));
  task.Wait();
}

bool ContentChild::IsAlive() const { return mIsAlive; }

bool ContentChild::IsShuttingDown() const { return mShuttingDown; }

void ContentChild::GetProcessName(nsACString& aName) const {
  aName.Assign(NS_ConvertUTF16toUTF8(mProcessName));
}

/* static */
void ContentChild::AppendProcessId(nsACString& aName) {
  if (!aName.IsEmpty()) {
    aName.Append(' ');
  }
  unsigned pid = getpid();
  aName.Append(nsPrintfCString("(pid %u)", pid));
}

void ContentChild::InitGraphicsDeviceData(const ContentDeviceData& aData) {
  gfxPlatform::InitChild(aData);
}

void ContentChild::InitSharedUASheets(const Maybe<SharedMemoryHandle>& aHandle,
                                      uintptr_t aAddress) {
  MOZ_ASSERT_IF(!aHandle, !aAddress);

  if (!aAddress) {
    return;
  }

  // Map the shared memory storing the user agent style sheets.  Do this as
  // early as possible to maximize the chance of being able to map at the
  // address we want.
  nsLayoutStylesheetCache::SetSharedMemory(*aHandle, aAddress);
}

void ContentChild::InitXPCOM(
    const XPCOMInitData& aXPCOMInit,
    const mozilla::dom::ipc::StructuredCloneData& aInitialData) {
  // Do this as early as possible to get the parent process to initialize the
  // background thread since we'll likely need database information very soon.
  BackgroundChild::Startup();

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    MOZ_ASSERT_UNREACHABLE("PBackground init can't fail at this point");
    return;
  }

  LSObject::Initialize();

  ClientManager::Startup();

  RemoteWorkerService::Initialize();

  nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!svc) {
    NS_WARNING("Couldn't acquire console service");
    return;
  }

  mConsoleListener = new ConsoleListener(this);
  if (NS_FAILED(svc->RegisterListener(mConsoleListener)))
    NS_WARNING("Couldn't register console listener for child process");

  mAvailableDictionaries = aXPCOMInit.dictionaries();

  RecvSetOffline(aXPCOMInit.isOffline());
  RecvSetConnectivity(aXPCOMInit.isConnected());
  LocaleService::GetInstance()->AssignAppLocales(aXPCOMInit.appLocales());
  LocaleService::GetInstance()->AssignRequestedLocales(
      aXPCOMInit.requestedLocales());

  RecvSetCaptivePortalState(aXPCOMInit.captivePortalState());
  RecvBidiKeyboardNotify(aXPCOMInit.isLangRTL(),
                         aXPCOMInit.haveBidiKeyboards());

  // Create the CPOW manager as soon as possible. Middleman processes don't use
  // CPOWs, because their recording child will also have a CPOW manager that
  // communicates with the UI process.
  if (!recordreplay::IsMiddleman()) {
    SendPJavaScriptConstructor();
  }

  if (aXPCOMInit.domainPolicy().active()) {
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(ssm);
    ssm->ActivateDomainPolicyInternal(getter_AddRefs(mPolicy));
    if (!mPolicy) {
      MOZ_CRASH("Failed to activate domain policy.");
    }
    mPolicy->ApplyClone(&aXPCOMInit.domainPolicy());
  }

  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1"));
  if (nsCOMPtr<nsIClipboardProxy> clipboardProxy =
          do_QueryInterface(clipboard)) {
    clipboardProxy->SetCapabilities(aXPCOMInit.clipboardCaps());
  }

  {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(xpc::PrivilegedJunkScope()))) {
      MOZ_CRASH();
    }
    ErrorResult rv;
    JS::RootedValue data(jsapi.cx());
    mozilla::dom::ipc::StructuredCloneData id;
    id.Copy(aInitialData);
    id.Read(jsapi.cx(), &data, rv);
    if (NS_WARN_IF(rv.Failed())) {
      MOZ_CRASH();
    }
    auto* global = ContentProcessMessageManager::Get();
    global->SetInitialProcessData(data);
  }

  // The stylesheet cache is not ready yet. Store this URL for future use.
  nsCOMPtr<nsIURI> ucsURL = DeserializeURI(aXPCOMInit.userContentSheetURL());
  nsLayoutStylesheetCache::SetUserContentCSSURL(ucsURL);

  GfxInfoBase::SetFeatureStatus(aXPCOMInit.gfxFeatureStatus());

  DataStorage::SetCachedStorageEntries(aXPCOMInit.dataStorage());

  // Set the dynamic scalar definitions for this process.
  TelemetryIPC::AddDynamicScalarDefinitions(aXPCOMInit.dynamicScalarDefs());
}

mozilla::ipc::IPCResult ContentChild::RecvRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage,
    const Maybe<mozilla::ipc::FileDescriptor>& aDMDFile) {
  nsCString process;
  GetProcessName(process);
  AppendProcessId(process);

  MemoryReportRequestClient::Start(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile, process,
      [&](const MemoryReport& aReport) {
        Unused << GetSingleton()->SendAddMemoryReport(aReport);
      },
      [&](const uint32_t& aGeneration) {
        return GetSingleton()->SendFinishMemoryReport(aGeneration);
      });
  return IPC_OK();
}

PCycleCollectWithLogsChild* ContentChild::AllocPCycleCollectWithLogsChild(
    const bool& aDumpAllTraces, const FileDescriptor& aGCLog,
    const FileDescriptor& aCCLog) {
  return do_AddRef(new CycleCollectWithLogsChild()).take();
}

mozilla::ipc::IPCResult ContentChild::RecvPCycleCollectWithLogsConstructor(
    PCycleCollectWithLogsChild* aActor, const bool& aDumpAllTraces,
    const FileDescriptor& aGCLog, const FileDescriptor& aCCLog) {
  // The sink's destructor is called when the last reference goes away, which
  // will cause the actor to be closed down.
  auto* actor = static_cast<CycleCollectWithLogsChild*>(aActor);
  RefPtr<CycleCollectWithLogsChild::Sink> sink =
      new CycleCollectWithLogsChild::Sink(actor, aGCLog, aCCLog);

  // Invoke the dumper, which will take a reference to the sink.
  nsCOMPtr<nsIMemoryInfoDumper> dumper =
      do_GetService("@mozilla.org/memory-info-dumper;1");
  dumper->DumpGCAndCCLogsToSink(aDumpAllTraces, sink);
  return IPC_OK();
}

bool ContentChild::DeallocPCycleCollectWithLogsChild(
    PCycleCollectWithLogsChild* aActor) {
  RefPtr<CycleCollectWithLogsChild> actor =
      dont_AddRef(static_cast<CycleCollectWithLogsChild*>(aActor));
  return true;
}

mozilla::ipc::IPCResult ContentChild::RecvInitGMPService(
    Endpoint<PGMPServiceChild>&& aGMPService) {
  if (!GMPServiceChild::Create(std::move(aGMPService))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvInitProfiler(
    Endpoint<PProfilerChild>&& aEndpoint) {
#ifdef MOZ_GECKO_PROFILER
  mProfilerController = ChildProfilerController::Create(std::move(aEndpoint));
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvGMPsChanged(
    nsTArray<GMPCapabilityData>&& capabilities) {
  GeckoMediaPluginServiceChild::UpdateGMPCapabilities(std::move(capabilities));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvInitProcessHangMonitor(
    Endpoint<PProcessHangMonitorChild>&& aHangMonitor) {
  CreateHangMonitorChild(std::move(aHangMonitor));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::GetResultForRenderingInitFailure(
    base::ProcessId aOtherPid) {
  if (aOtherPid == base::GetCurrentProcId() || aOtherPid == OtherPid()) {
    // If we are talking to ourselves, or the UI process, then that is a fatal
    // protocol error.
    return IPC_FAIL_NO_REASON(this);
  }

  // If we are talking to the GPU process, then we should recover from this on
  // the next ContentChild::RecvReinitRendering call.
  gfxCriticalNote << "Could not initialize rendering with GPU process";
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRequestPerformanceMetrics(
    const nsID& aID) {
  RefPtr<ContentChild> self = this;
  RefPtr<AbstractThread> mainThread =
      SystemGroup::AbstractMainThreadFor(TaskCategory::Performance);
  nsTArray<RefPtr<PerformanceInfoPromise>> promises = CollectPerformanceInfo();

  PerformanceInfoPromise::All(mainThread, promises)
      ->Then(
          mainThread, __func__,
          [self, aID](const nsTArray<mozilla::dom::PerformanceInfo>& aResult) {
            self->SendAddPerformanceMetrics(aID, aResult);
          },
          []() { /* silently fails -- the parent times out
                    and proceeds when the data is not coming back */
          });

  return IPC_OK();
}

#if defined(XP_MACOSX)
extern "C" {
void CGSShutdownServerConnections();
};
#endif

mozilla::ipc::IPCResult ContentChild::RecvInitRendering(
    Endpoint<PCompositorManagerChild>&& aCompositor,
    Endpoint<PImageBridgeChild>&& aImageBridge,
    Endpoint<PVRManagerChild>&& aVRBridge,
    Endpoint<PRemoteDecoderManagerChild>&& aVideoManager,
    nsTArray<uint32_t>&& namespaces) {
  MOZ_ASSERT(namespaces.Length() == 3);

  // Note that for all of the methods below, if it can fail, it should only
  // return false if the failure is an IPDL error. In such situations,
  // ContentChild can reason about whether or not to wait for
  // RecvReinitRendering (because we surmised the GPU process crashed), or if it
  // should crash itself (because we are actually talking to the UI process). If
  // there are localized failures (e.g. failed to spawn a thread), then it
  // should MOZ_RELEASE_ASSERT or MOZ_CRASH as necessary instead.
  if (!CompositorManagerChild::Init(std::move(aCompositor), namespaces[0])) {
    return GetResultForRenderingInitFailure(aCompositor.OtherPid());
  }
  if (!CompositorManagerChild::CreateContentCompositorBridge(namespaces[1])) {
    return GetResultForRenderingInitFailure(aCompositor.OtherPid());
  }
  if (!ImageBridgeChild::InitForContent(std::move(aImageBridge),
                                        namespaces[2])) {
    return GetResultForRenderingInitFailure(aImageBridge.OtherPid());
  }
  if (!gfx::VRManagerChild::InitForContent(std::move(aVRBridge))) {
    return GetResultForRenderingInitFailure(aVRBridge.OtherPid());
  }
  RemoteDecoderManagerChild::InitForGPUProcess(std::move(aVideoManager));

#if defined(XP_MACOSX) && !defined(MOZ_SANDBOX)
  // Close all current connections to the WindowServer. This ensures that the
  // Activity Monitor will not label the content process as "Not responding"
  // because it's not running a native event loop. See bug 1384336. When the
  // build is configured with sandbox support, this is called during sandbox
  // setup.
  CGSShutdownServerConnections();
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvReinitRendering(
    Endpoint<PCompositorManagerChild>&& aCompositor,
    Endpoint<PImageBridgeChild>&& aImageBridge,
    Endpoint<PVRManagerChild>&& aVRBridge,
    Endpoint<PRemoteDecoderManagerChild>&& aVideoManager,
    nsTArray<uint32_t>&& namespaces) {
  MOZ_ASSERT(namespaces.Length() == 3);
  nsTArray<RefPtr<BrowserChild>> tabs = BrowserChild::GetAll();

  // Zap all the old layer managers we have lying around.
  for (const auto& browserChild : tabs) {
    if (browserChild->GetLayersId().IsValid()) {
      browserChild->InvalidateLayers();
    }
  }

  // Re-establish singleton bridges to the compositor.
  if (!CompositorManagerChild::Init(std::move(aCompositor), namespaces[0])) {
    return GetResultForRenderingInitFailure(aCompositor.OtherPid());
  }
  if (!CompositorManagerChild::CreateContentCompositorBridge(namespaces[1])) {
    return GetResultForRenderingInitFailure(aCompositor.OtherPid());
  }
  if (!ImageBridgeChild::ReinitForContent(std::move(aImageBridge),
                                          namespaces[2])) {
    return GetResultForRenderingInitFailure(aImageBridge.OtherPid());
  }
  if (!gfx::VRManagerChild::ReinitForContent(std::move(aVRBridge))) {
    return GetResultForRenderingInitFailure(aVRBridge.OtherPid());
  }
  gfxPlatform::GetPlatform()->CompositorUpdated();

  // Establish new PLayerTransactions.
  for (const auto& browserChild : tabs) {
    if (browserChild->GetLayersId().IsValid()) {
      browserChild->ReinitRendering();
    }
  }

  RemoteDecoderManagerChild::InitForGPUProcess(std::move(aVideoManager));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvAudioDefaultDeviceChange() {
#ifdef XP_WIN
  audio::AudioNotificationReceiver::NotifyDefaultDeviceChanged();
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvReinitRenderingForDeviceReset() {
  gfxPlatform::GetPlatform()->CompositorUpdated();

  nsTArray<RefPtr<BrowserChild>> tabs = BrowserChild::GetAll();
  for (const auto& browserChild : tabs) {
    if (browserChild->GetLayersId().IsValid()) {
      browserChild->ReinitRenderingForDeviceReset();
    }
  }
  return IPC_OK();
}

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
extern "C" {
CGError CGSSetDenyWindowServerConnections(bool);
};

static bool StartMacOSContentSandbox() {
  // Close all current connections to the WindowServer. This ensures that the
  // Activity Monitor will not label the content process as "Not responding"
  // because it's not running a native event loop. See bug 1384336.
  // This is required with or without the sandbox enabled. Until the
  // window server is blocked as the policy level, this should be called
  // just before CGSSetDenyWindowServerConnections() so there are no
  // windowserver connections active when CGSSetDenyWindowServerConnections()
  // is called.
  CGSShutdownServerConnections();

  int sandboxLevel = GetEffectiveContentSandboxLevel();
  if (sandboxLevel < 1) {
    return false;
  }

  // Actual security benefits are only acheived when we additionally deny
  // future connections, however this currently breaks WebGL so it's not done
  // by default.
  if (Preferences::GetBool(
          "security.sandbox.content.mac.disconnect-windowserver")) {
    CGError result = CGSSetDenyWindowServerConnections(true);
    MOZ_DIAGNOSTIC_ASSERT(result == kCGErrorSuccess);
#  if !MOZ_DIAGNOSTIC_ASSERT_ENABLED
    Unused << result;
#  endif
  }

  // If the sandbox is already enabled, there's nothing more to do here.
  if (Preferences::GetBool("security.sandbox.content.mac.earlyinit") &&
      !recordreplay::IsRecordingOrReplaying()) {
    return true;
  }

  nsAutoCString appPath;
  if (!nsMacUtilsImpl::GetAppPath(appPath)) {
    MOZ_CRASH("Error resolving child process app path");
  }

  ContentChild* cc = ContentChild::GetSingleton();

  nsresult rv;
  nsCOMPtr<nsIFile> profileDir;
  cc->GetProfileDir(getter_AddRefs(profileDir));
  nsCString profileDirPath;
  if (profileDir) {
    profileDir->Normalize();
    rv = profileDir->GetNativePath(profileDirPath);
    if (NS_FAILED(rv) || profileDirPath.IsEmpty()) {
      MOZ_CRASH("Failed to get profile path");
    }
  }

  bool isFileProcess = cc->GetRemoteType().EqualsLiteral(FILE_REMOTE_TYPE);

  MacSandboxInfo info;
  info.type = MacSandboxType_Content;
  info.level = sandboxLevel;
  info.hasFilePrivileges = isFileProcess;
  info.shouldLog = Preferences::GetBool("security.sandbox.logging.enabled") ||
                   PR_GetEnv("MOZ_SANDBOX_LOGGING");
  info.appPath.assign(appPath.get());
  info.hasAudio = !Preferences::GetBool("media.cubeb.sandbox");
  info.hasWindowServer = !Preferences::GetBool(
      "security.sandbox.content.mac.disconnect-windowserver");

  // These paths are used to allowlist certain directories used by the testing
  // system. They should not be considered a public API, and are only intended
  // for use in automation.
  nsAutoCString testingReadPath1;
  Preferences::GetCString("security.sandbox.content.mac.testing_read_path1",
                          testingReadPath1);
  if (!testingReadPath1.IsEmpty()) {
    info.testingReadPath1.assign(testingReadPath1.get());
  }
  nsAutoCString testingReadPath2;
  Preferences::GetCString("security.sandbox.content.mac.testing_read_path2",
                          testingReadPath2);
  if (!testingReadPath2.IsEmpty()) {
    info.testingReadPath2.assign(testingReadPath2.get());
  }

  if (mozilla::IsDevelopmentBuild()) {
    nsCOMPtr<nsIFile> repoDir;
    rv = nsMacUtilsImpl::GetRepoDir(getter_AddRefs(repoDir));
    if (NS_FAILED(rv)) {
      MOZ_CRASH("Failed to get path to repo dir");
    }
    nsCString repoDirPath;
    Unused << repoDir->GetNativePath(repoDirPath);
    info.testingReadPath3.assign(repoDirPath.get());

    nsCOMPtr<nsIFile> objDir;
    rv = nsMacUtilsImpl::GetObjDir(getter_AddRefs(objDir));
    if (NS_FAILED(rv)) {
      MOZ_CRASH("Failed to get path to build object dir");
    }

    nsCString objDirPath;
    Unused << objDir->GetNativePath(objDirPath);
    info.testingReadPath4.assign(objDirPath.get());
  }

  if (profileDir) {
    info.hasSandboxedProfile = true;
    info.profileDir.assign(profileDirPath.get());
  } else {
    info.hasSandboxedProfile = false;
  }

#  ifdef DEBUG
  // For bloat/leak logging or when a content process dies intentionally
  // (|NoteIntentionalCrash|) for tests, it wants to log that it did this.
  // Allow writing to this location.
  nsAutoCString bloatLogDirPath;
  if (NS_SUCCEEDED(nsMacUtilsImpl::GetBloatLogDir(bloatLogDirPath))) {
    info.debugWriteDir = bloatLogDirPath.get();
  }
#  endif  // DEBUG

  std::string err;
  if (!mozilla::StartMacSandbox(info, err)) {
    NS_WARNING(err.c_str());
    MOZ_CRASH("sandbox_init() failed");
  }

  return true;
}
#endif

mozilla::ipc::IPCResult ContentChild::RecvSetProcessSandbox(
    const Maybe<mozilla::ipc::FileDescriptor>& aBroker) {
  // We may want to move the sandbox initialization somewhere else
  // at some point; see bug 880808.
#if defined(MOZ_SANDBOX)
  bool sandboxEnabled = true;
#  if defined(XP_LINUX)
  // On Linux, we have to support systems that can't use any sandboxing.
  if (!SandboxInfo::Get().CanSandboxContent()) {
    sandboxEnabled = false;
  } else {
    // Pre-start audio before sandboxing; see bug 1443612.
    if (Preferences::GetBool("media.cubeb.sandbox")) {
      if (atp_set_real_time_limit(0, 48000)) {
        NS_WARNING("could not set real-time limit at process startup");
      }
      InstallSoftRealTimeLimitHandler();
    } else {
      Unused << CubebUtils::GetCubebContext();
    }
  }

  if (sandboxEnabled) {
    sandboxEnabled = SetContentProcessSandbox(
        ContentProcessSandboxParams::ForThisProcess(aBroker));
  }
#  elif defined(XP_WIN)
  mozilla::SandboxTarget::Instance()->StartSandbox();
#  elif defined(XP_MACOSX)
  sandboxEnabled = StartMacOSContentSandbox();
#  elif defined(__OpenBSD__)
  sandboxEnabled = StartOpenBSDSandbox(GeckoProcessType_Content);
  /* dont overwrite an existing session dbus address, but ensure it is set */
  if (!PR_GetEnv("DBUS_SESSION_BUS_ADDRESS")) {
    static LazyLogModule sPledgeLog("SandboxPledge");
    MOZ_LOG(sPledgeLog, LogLevel::Debug,
            ("no session dbus found, faking one\n"));
    PR_SetEnv("DBUS_SESSION_BUS_ADDRESS=");
  }
#  endif

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::ContentSandboxEnabled, sandboxEnabled);
#  if defined(XP_LINUX) && !defined(OS_ANDROID)
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::ContentSandboxCapabilities,
      static_cast<int>(SandboxInfo::Get().AsInteger()));
#  endif /* XP_LINUX && !OS_ANDROID */
  // Use the prefix to avoid URIs from Fission isolated processes.
  auto remoteTypePrefix = RemoteTypePrefix(GetRemoteType());
  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::RemoteType,
                                     NS_ConvertUTF16toUTF8(remoteTypePrefix));
#endif /* MOZ_SANDBOX */

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvBidiKeyboardNotify(
    const bool& aIsLangRTL, const bool& aHaveBidiKeyboards) {
  // bidi is always of type PuppetBidiKeyboard* (because in the child, the only
  // possible implementation of nsIBidiKeyboard is PuppetBidiKeyboard).
  PuppetBidiKeyboard* bidi =
      static_cast<PuppetBidiKeyboard*>(nsContentUtils::GetBidiKeyboard());
  if (bidi) {
    bidi->SetBidiKeyboardInfo(aIsLangRTL, aHaveBidiKeyboards);
  }
  return IPC_OK();
}

static StaticRefPtr<CancelableRunnable> gFirstIdleTask;

static void FirstIdle(void) {
  MOZ_ASSERT(gFirstIdleTask);
  gFirstIdleTask = nullptr;

  // When recording or replaying, the middleman process will send this message
  // instead.
  if (!recordreplay::IsRecordingOrReplaying()) {
    ContentChild::GetSingleton()->SendFirstIdle();
  }
}

mozilla::jsipc::PJavaScriptChild* ContentChild::AllocPJavaScriptChild() {
  MOZ_ASSERT(ManagedPJavaScriptChild().IsEmpty());

  return NewJavaScriptChild();
}

bool ContentChild::DeallocPJavaScriptChild(PJavaScriptChild* aChild) {
  ReleaseJavaScriptChild(aChild);
  return true;
}

mozilla::ipc::IPCResult ContentChild::RecvConstructBrowser(
    ManagedEndpoint<PBrowserChild>&& aBrowserEp,
    ManagedEndpoint<PWindowGlobalChild>&& aWindowEp, const TabId& aTabId,
    const TabId& aSameTabGroupAs, const IPCTabContext& aContext,
    const WindowGlobalInit& aWindowInit, const uint32_t& aChromeFlags,
    const ContentParentId& aCpID, const bool& aIsForBrowser,
    const bool& aIsTopLevel) {
  MOZ_ASSERT(!IsShuttingDown());

  static bool hasRunOnce = false;
  if (!hasRunOnce) {
    hasRunOnce = true;
    MOZ_ASSERT(!gFirstIdleTask);
    RefPtr<CancelableRunnable> firstIdleTask =
        NewCancelableRunnableFunction("FirstIdleRunnable", FirstIdle);
    gFirstIdleTask = firstIdleTask;
    if (NS_FAILED(NS_DispatchToCurrentThreadQueue(firstIdleTask.forget(),
                                                  EventQueuePriority::Idle))) {
      gFirstIdleTask = nullptr;
      hasRunOnce = false;
    }
  }

  // We'll happily accept any kind of IPCTabContext here; we don't need to
  // check that it's of a certain type for security purposes, because we
  // believe whatever the parent process tells us.
  MaybeInvalidTabContext tc(aContext);
  if (!tc.IsValid()) {
    NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                             "the parent process. (%s)  Crashing...",
                             tc.GetInvalidReason())
                 .get());
    MOZ_CRASH("Invalid TabContext received from the parent process.");
  }

  auto windowChild = MakeRefPtr<WindowGlobalChild>(aWindowInit, nullptr);

  RefPtr<BrowserChild> browserChild = BrowserChild::Create(
      this, aTabId, aSameTabGroupAs, tc.GetTabContext(),
      aWindowInit.browsingContext(), aChromeFlags, aIsTopLevel);

  // Bind the created BrowserChild to IPC to actually link the actor.
  if (NS_WARN_IF(!BindPBrowserEndpoint(std::move(aBrowserEp), browserChild))) {
    return IPC_FAIL(this, "BindPBrowserEndpoint failed");
  }

  if (NS_WARN_IF(!browserChild->BindPWindowGlobalEndpoint(std::move(aWindowEp),
                                                          windowChild))) {
    return IPC_FAIL(this, "BindPWindowGlobalEndpoint failed");
  }
  windowChild->Init();

  // Ensure that a TabGroup is set for our BrowserChild before running `Init`.
  if (!browserChild->mTabGroup) {
    browserChild->mTabGroup = TabGroup::GetFromActor(browserChild);

    if (!browserChild->mTabGroup) {
      browserChild->mTabGroup = new TabGroup();
      MOZ_DIAGNOSTIC_ASSERT(aSameTabGroupAs != 0);
    }
  }

  if (NS_WARN_IF(
          NS_FAILED(browserChild->Init(/* aOpener */ nullptr, windowChild)))) {
    return IPC_FAIL(browserChild, "BrowserChild::Init failed");
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(static_cast<nsIBrowserChild*>(browserChild),
                        "tab-child-created", nullptr);
  }
  // Notify parent that we are ready to handle input events.
  browserChild->SendRemoteIsReadyToHandleInputEvents();
  return IPC_OK();
}

void ContentChild::GetAvailableDictionaries(nsTArray<nsString>& aDictionaries) {
  aDictionaries = mAvailableDictionaries;
}

PFileDescriptorSetChild* ContentChild::SendPFileDescriptorSetConstructor(
    const FileDescriptor& aFD) {
  if (IsShuttingDown()) {
    return nullptr;
  }

  return PContentChild::SendPFileDescriptorSetConstructor(aFD);
}

PFileDescriptorSetChild* ContentChild::AllocPFileDescriptorSetChild(
    const FileDescriptor& aFD) {
  return new FileDescriptorSetChild(aFD);
}

bool ContentChild::DeallocPFileDescriptorSetChild(
    PFileDescriptorSetChild* aActor) {
  delete static_cast<FileDescriptorSetChild*>(aActor);
  return true;
}

already_AddRefed<PIPCBlobInputStreamChild>
ContentChild::AllocPIPCBlobInputStreamChild(const nsID& aID,
                                            const uint64_t& aSize) {
  RefPtr<IPCBlobInputStreamChild> actor =
      new IPCBlobInputStreamChild(aID, aSize);
  return actor.forget();
}

mozilla::PRemoteSpellcheckEngineChild*
ContentChild::AllocPRemoteSpellcheckEngineChild() {
  MOZ_CRASH(
      "Default Constructor for PRemoteSpellcheckEngineChild should never be "
      "called");
  return nullptr;
}

bool ContentChild::DeallocPRemoteSpellcheckEngineChild(
    PRemoteSpellcheckEngineChild* child) {
  delete child;
  return true;
}

PPresentationChild* ContentChild::AllocPPresentationChild() {
  MOZ_CRASH("We should never be manually allocating PPresentationChild actors");
  return nullptr;
}

bool ContentChild::DeallocPPresentationChild(PPresentationChild* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult ContentChild::RecvNotifyPresentationReceiverLaunched(
    PBrowserChild* aIframe, const nsString& aSessionId) {
  nsCOMPtr<nsIDocShell> docShell =
      do_GetInterface(static_cast<BrowserChild*>(aIframe)->WebNavigation());
  NS_WARNING_ASSERTION(docShell, "WebNavigation failed");

  nsCOMPtr<nsIPresentationService> service =
      do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(service, "presentation service is missing");

  Unused << NS_WARN_IF(
      NS_FAILED(static_cast<PresentationIPCService*>(service.get())
                    ->MonitorResponderLoading(aSessionId, docShell)));

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvNotifyPresentationReceiverCleanUp(
    const nsString& aSessionId) {
  nsCOMPtr<nsIPresentationService> service =
      do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(service, "presentation service is missing");

  Unused << NS_WARN_IF(NS_FAILED(service->UntrackSessionInfo(
      aSessionId, nsIPresentationService::ROLE_RECEIVER)));

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvNotifyEmptyHTTPCache() {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->NotifyObservers(nullptr, "cacheservice:empty-cache", nullptr);
  return IPC_OK();
}

PHalChild* ContentChild::AllocPHalChild() { return CreateHalChild(); }

bool ContentChild::DeallocPHalChild(PHalChild* aHal) {
  delete aHal;
  return true;
}

devtools::PHeapSnapshotTempFileHelperChild*
ContentChild::AllocPHeapSnapshotTempFileHelperChild() {
  return devtools::HeapSnapshotTempFileHelperChild::Create();
}

bool ContentChild::DeallocPHeapSnapshotTempFileHelperChild(
    devtools::PHeapSnapshotTempFileHelperChild* aHeapSnapshotHelper) {
  delete aHeapSnapshotHelper;
  return true;
}

PTestShellChild* ContentChild::AllocPTestShellChild() {
  return new TestShellChild();
}

bool ContentChild::DeallocPTestShellChild(PTestShellChild* shell) {
  delete shell;
  return true;
}

jsipc::CPOWManager* ContentChild::GetCPOWManager() {
  if (PJavaScriptChild* c =
          LoneManagedOrNullAsserts(ManagedPJavaScriptChild())) {
    return CPOWManagerFor(c);
  }
  if (recordreplay::IsMiddleman()) {
    return nullptr;
  }
  return CPOWManagerFor(SendPJavaScriptConstructor());
}

mozilla::ipc::IPCResult ContentChild::RecvPTestShellConstructor(
    PTestShellChild* actor) {
  return IPC_OK();
}

void ContentChild::UpdateCookieStatus(nsIChannel* aChannel) {
  RefPtr<CookieServiceChild> csChild = CookieServiceChild::GetSingleton();
  NS_ASSERTION(csChild, "Couldn't get CookieServiceChild");

  csChild->TrackCookieLoad(aChannel);
}

already_AddRefed<RemoteBrowser> ContentChild::CreateBrowser(
    nsFrameLoader* aFrameLoader, const TabContext& aContext,
    const nsString& aRemoteType, BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(XRE_IsContentProcess());

  // Determine our embedder's BrowserChild actor.
  RefPtr<Element> owner = aFrameLoader->GetOwnerContent();
  MOZ_DIAGNOSTIC_ASSERT(owner);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(owner->GetOwnerGlobal());
  MOZ_DIAGNOSTIC_ASSERT(docShell);

  RefPtr<BrowserChild> browserChild = BrowserChild::GetFrom(docShell);
  MOZ_DIAGNOSTIC_ASSERT(browserChild);

  uint32_t chromeFlags = 0;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  if (docShell) {
    docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  }
  if (treeOwner) {
    nsCOMPtr<nsIWebBrowserChrome> wbc = do_GetInterface(treeOwner);
    if (wbc) {
      wbc->GetChromeFlags(&chromeFlags);
    }
  }

  // Checking that this actually does something useful is
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1542710
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
  if (loadContext && loadContext->UsePrivateBrowsing()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
  }
  if (loadContext && loadContext->UseRemoteTabs()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_REMOTE_WINDOW;
  }
  if (loadContext && loadContext->UseRemoteSubframes()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_FISSION_WINDOW;
  }
  if (docShell->GetAffectPrivateSessionLifetime()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  }

  TabId tabId(nsContentUtils::GenerateTabId());
  RefPtr<BrowserBridgeChild> browserBridge =
      new BrowserBridgeChild(aBrowsingContext, tabId);

  browserChild->SendPBrowserBridgeConstructor(
      browserBridge, PromiseFlatString(aContext.PresentationURL()), aRemoteType,
      aBrowsingContext, chromeFlags, tabId);

  return browserBridge->FinishInit(aFrameLoader);
}

PScriptCacheChild* ContentChild::AllocPScriptCacheChild(
    const FileDescOrError& cacheFile, const bool& wantCacheData) {
  return new loader::ScriptCacheChild();
}

bool ContentChild::DeallocPScriptCacheChild(PScriptCacheChild* cache) {
  delete static_cast<loader::ScriptCacheChild*>(cache);
  return true;
}

mozilla::ipc::IPCResult ContentChild::RecvPScriptCacheConstructor(
    PScriptCacheChild* actor, const FileDescOrError& cacheFile,
    const bool& wantCacheData) {
  Maybe<FileDescriptor> fd;
  if (cacheFile.type() == cacheFile.TFileDescriptor) {
    fd.emplace(cacheFile.get_FileDescriptor());
  }

  static_cast<loader::ScriptCacheChild*>(actor)->Init(fd, wantCacheData);
  return IPC_OK();
}

PNeckoChild* ContentChild::AllocPNeckoChild() { return new NeckoChild(); }

mozilla::ipc::IPCResult ContentChild::RecvNetworkLinkTypeChange(
    const uint32_t& aType) {
  mNetworkLinkType = aType;
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "contentchild:network-link-type-changed",
                         nullptr);
  }
  return IPC_OK();
}

bool ContentChild::DeallocPNeckoChild(PNeckoChild* necko) {
  delete necko;
  return true;
}

PPrintingChild* ContentChild::AllocPPrintingChild() {
  // The ContentParent should never attempt to allocate the nsPrintingProxy,
  // which implements PPrintingChild. Instead, the nsPrintingProxy service is
  // requested and instantiated via XPCOM, and the constructor of
  // nsPrintingProxy sets up the IPC connection.
  MOZ_CRASH("Should never get here!");
  return nullptr;
}

bool ContentChild::DeallocPPrintingChild(PPrintingChild* printing) {
  return true;
}

PChildToParentStreamChild* ContentChild::SendPChildToParentStreamConstructor(
    PChildToParentStreamChild* aActor) {
  if (IsShuttingDown()) {
    return nullptr;
  }

  return PContentChild::SendPChildToParentStreamConstructor(aActor);
}

PChildToParentStreamChild* ContentChild::AllocPChildToParentStreamChild() {
  MOZ_CRASH("PChildToParentStreamChild actors should be manually constructed!");
}

bool ContentChild::DeallocPChildToParentStreamChild(
    PChildToParentStreamChild* aActor) {
  delete aActor;
  return true;
}

PParentToChildStreamChild* ContentChild::AllocPParentToChildStreamChild() {
  return mozilla::ipc::AllocPParentToChildStreamChild();
}

bool ContentChild::DeallocPParentToChildStreamChild(
    PParentToChildStreamChild* aActor) {
  delete aActor;
  return true;
}

PPSMContentDownloaderChild* ContentChild::AllocPPSMContentDownloaderChild(
    const uint32_t& aCertType) {
  // NB: We don't need aCertType in the child actor.
  RefPtr<PSMContentDownloaderChild> child = new PSMContentDownloaderChild();
  return child.forget().take();
}

bool ContentChild::DeallocPPSMContentDownloaderChild(
    PPSMContentDownloaderChild* aListener) {
  auto* listener = static_cast<PSMContentDownloaderChild*>(aListener);
  RefPtr<PSMContentDownloaderChild> child = dont_AddRef(listener);
  return true;
}

media::PMediaChild* ContentChild::AllocPMediaChild() {
  return media::AllocPMediaChild();
}

bool ContentChild::DeallocPMediaChild(media::PMediaChild* aActor) {
  return media::DeallocPMediaChild(aActor);
}

PBenchmarkStorageChild* ContentChild::AllocPBenchmarkStorageChild() {
  return BenchmarkStorageChild::Instance();
}

bool ContentChild::DeallocPBenchmarkStorageChild(
    PBenchmarkStorageChild* aActor) {
  delete aActor;
  return true;
}

PSpeechSynthesisChild* ContentChild::AllocPSpeechSynthesisChild() {
#ifdef MOZ_WEBSPEECH
  MOZ_CRASH("No one should be allocating PSpeechSynthesisChild actors");
#else
  return nullptr;
#endif
}

bool ContentChild::DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor) {
#ifdef MOZ_WEBSPEECH
  delete aActor;
  return true;
#else
  return false;
#endif
}

PWebrtcGlobalChild* ContentChild::AllocPWebrtcGlobalChild() {
#ifdef MOZ_WEBRTC
  auto* child = new WebrtcGlobalChild();
  return child;
#else
  return nullptr;
#endif
}

bool ContentChild::DeallocPWebrtcGlobalChild(PWebrtcGlobalChild* aActor) {
#ifdef MOZ_WEBRTC
  delete static_cast<WebrtcGlobalChild*>(aActor);
  return true;
#else
  return false;
#endif
}

mozilla::ipc::IPCResult ContentChild::RecvRegisterChrome(
    nsTArray<ChromePackage>&& packages,
    nsTArray<SubstitutionMapping>&& resources,
    nsTArray<OverrideMapping>&& overrides, const nsCString& locale,
    const bool& reset) {
  nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
  nsChromeRegistryContent* chromeRegistry =
      static_cast<nsChromeRegistryContent*>(registrySvc.get());
  if (!chromeRegistry) {
    return IPC_FAIL(this, "ChromeRegistryContent is null!");
  }
  chromeRegistry->RegisterRemoteChrome(packages, resources, overrides, locale,
                                       reset);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRegisterChromeItem(
    const ChromeRegistryItem& item) {
  nsCOMPtr<nsIChromeRegistry> registrySvc = nsChromeRegistry::GetService();
  nsChromeRegistryContent* chromeRegistry =
      static_cast<nsChromeRegistryContent*>(registrySvc.get());
  if (!chromeRegistry) {
    return IPC_FAIL(this, "ChromeRegistryContent is null!");
  }
  switch (item.type()) {
    case ChromeRegistryItem::TChromePackage:
      chromeRegistry->RegisterPackage(item.get_ChromePackage());
      break;

    case ChromeRegistryItem::TOverrideMapping:
      chromeRegistry->RegisterOverride(item.get_OverrideMapping());
      break;

    case ChromeRegistryItem::TSubstitutionMapping:
      chromeRegistry->RegisterSubstitution(item.get_SubstitutionMapping());
      break;

    default:
      MOZ_ASSERT(false, "bad chrome item");
      return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvClearImageCache(
    const bool& privateLoader, const bool& chrome) {
  imgLoader* loader = privateLoader ? imgLoader::PrivateBrowsingLoader()
                                    : imgLoader::NormalLoader();

  loader->ClearCache(chrome);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvSetOffline(const bool& offline) {
  nsCOMPtr<nsIIOService> io(do_GetIOService());
  NS_ASSERTION(io, "IO Service can not be null");

  io->SetOffline(offline);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvSetConnectivity(
    const bool& connectivity) {
  nsCOMPtr<nsIIOService> io(do_GetIOService());
  nsCOMPtr<nsIIOServiceInternal> ioInternal(do_QueryInterface(io));
  NS_ASSERTION(ioInternal, "IO Service can not be null");

  ioInternal->SetConnectivity(connectivity);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvSetCaptivePortalState(
    const int32_t& aState) {
  nsCOMPtr<nsICaptivePortalService> cps = do_GetService(NS_CAPTIVEPORTAL_CID);
  if (!cps) {
    return IPC_OK();
  }

  mozilla::net::CaptivePortalService* portal =
      static_cast<mozilla::net::CaptivePortalService*>(cps.get());
  portal->SetStateInChild(aState);

  return IPC_OK();
}

void ContentChild::ActorDestroy(ActorDestroyReason why) {
  if (mForceKillTimer) {
    mForceKillTimer->Cancel();
    mForceKillTimer = nullptr;
  }

  if (AbnormalShutdown == why) {
    NS_WARNING("shutting down early because of crash!");
    ProcessChild::QuickExit();
  }

#ifndef NS_FREE_PERMANENT_DATA
  // In release builds, there's no point in the content process
  // going through the full XPCOM shutdown path, because it doesn't
  // keep persistent state.
  ProcessChild::QuickExit();
#else
  if (gFirstIdleTask) {
    gFirstIdleTask->Cancel();
    gFirstIdleTask = nullptr;
  }

  BlobURLProtocolHandler::RemoveDataEntries();

  mSharedData = nullptr;

  mAlertObservers.Clear();

  mIdleObservers.Clear();

  mBrowsingContextGroupHolder.Clear();

  nsCOMPtr<nsIConsoleService> svc(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (svc) {
    svc->UnregisterListener(mConsoleListener);
    mConsoleListener->mChild = nullptr;
  }
  mIsAlive = false;

  CrashReporterClient::DestroySingleton();

  XRE_ShutdownChildProcess();
#endif  // NS_FREE_PERMANENT_DATA
}

void ContentChild::ProcessingError(Result aCode, const char* aReason) {
  switch (aCode) {
    case MsgDropped:
      NS_WARNING("MsgDropped in ContentChild");
      return;

    case MsgNotKnown:
    case MsgNotAllowed:
    case MsgPayloadError:
    case MsgProcessingError:
    case MsgRouteError:
    case MsgValueError:
      break;

    default:
      MOZ_CRASH("not reached");
  }

  nsDependentCString reason(aReason);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::ipc_channel_error, reason);

  MOZ_CRASH("Content child abort due to IPC error");
}

nsresult ContentChild::AddRemoteAlertObserver(const nsString& aData,
                                              nsIObserver* aObserver) {
  NS_ASSERTION(aObserver, "Adding a null observer?");
  mAlertObservers.AppendElement(new AlertObserver(aObserver, aData));
  return NS_OK;
}

mozilla::ipc::IPCResult ContentChild::RecvPreferenceUpdate(const Pref& aPref) {
  Preferences::SetPreference(aPref);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvVarUpdate(const GfxVarUpdate& aVar) {
  gfx::gfxVars::ApplyUpdate(aVar);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUpdatePerfStatsCollectionMask(
    const uint64_t& aMask) {
  PerfStats::SetCollectionMask(static_cast<PerfStats::MetricMask>(aMask));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvCollectPerfStatsJSON(
    CollectPerfStatsJSONResolver&& aResolver) {
  aResolver(PerfStats::CollectLocalPerfStatsJSON());
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvDataStoragePut(
    const nsString& aFilename, const DataStorageItem& aItem) {
  RefPtr<DataStorage> storage = DataStorage::GetFromRawFileName(aFilename);
  if (storage) {
    storage->Put(aItem.key(), aItem.value(), aItem.type());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvDataStorageRemove(
    const nsString& aFilename, const nsCString& aKey,
    const DataStorageType& aType) {
  RefPtr<DataStorage> storage = DataStorage::GetFromRawFileName(aFilename);
  if (storage) {
    storage->Remove(aKey, aType);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvDataStorageClear(
    const nsString& aFilename) {
  RefPtr<DataStorage> storage = DataStorage::GetFromRawFileName(aFilename);
  if (storage) {
    storage->Clear();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvNotifyAlertsObserver(
    const nsCString& aType, const nsString& aData) {
  for (uint32_t i = 0; i < mAlertObservers.Length();
       /*we mutate the array during the loop; ++i iff no mutation*/) {
    AlertObserver* observer = mAlertObservers[i];
    if (observer->Observes(aData) && observer->Notify(aType)) {
      // if aType == alertfinished, this alert is done.  we can
      // remove the observer.
      if (aType.Equals(nsDependentCString("alertfinished"))) {
        mAlertObservers.RemoveElementAt(i);
        continue;
      }
    }
    ++i;
  }
  return IPC_OK();
}

// NOTE: This method is being run in the SystemGroup, and thus cannot directly
// touch pages. See GetSpecificMessageEventTarget.
mozilla::ipc::IPCResult ContentChild::RecvNotifyVisited(
    nsTArray<URIParams>&& aURIs) {
  for (const URIParams& uri : aURIs) {
    nsCOMPtr<nsIURI> newURI = DeserializeURI(uri);
    if (!newURI) {
      return IPC_FAIL_NO_REASON(this);
    }
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    if (history) {
      history->NotifyVisited(newURI);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvLoadProcessScript(
    const nsString& aURL) {
  auto* global = ContentProcessMessageManager::Get();
  global->LoadScript(aURL);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvAsyncMessage(
    const nsString& aMsg, nsTArray<CpowEntry>&& aCpows,
    const IPC::Principal& aPrincipal, const ClonedMessageData& aData) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("ContentChild::RecvAsyncMessage",
                                             OTHER, aMsg);
  MMPrinter::Print("ContentChild::RecvAsyncMessage", aMsg, aData);

  CrossProcessCpowHolder cpows(this, aCpows);
  RefPtr<nsFrameMessageManager> cpm =
      nsFrameMessageManager::GetChildProcessManager();
  if (cpm) {
    StructuredCloneData data;
    ipc::UnpackClonedMessageDataForChild(aData, data);
    cpm->ReceiveMessage(cpm, nullptr, aMsg, false, &data, &cpows, aPrincipal,
                        nullptr, IgnoreErrors());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRegisterStringBundles(
    nsTArray<mozilla::dom::StringBundleDescriptor>&& aDescriptors) {
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      services::GetStringBundleService();

  for (auto& descriptor : aDescriptors) {
    stringBundleService->RegisterContentBundle(
        descriptor.bundleURL(), descriptor.mapFile(), descriptor.mapSize());
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUpdateSharedData(
    const FileDescriptor& aMapFile, const uint32_t& aMapSize,
    nsTArray<IPCBlob>&& aBlobs, nsTArray<nsCString>&& aChangedKeys) {
  nsTArray<RefPtr<BlobImpl>> blobImpls(aBlobs.Length());
  for (auto& ipcBlob : aBlobs) {
    blobImpls.AppendElement(IPCBlobUtils::Deserialize(ipcBlob));
  }

  if (mSharedData) {
    mSharedData->Update(aMapFile, aMapSize, std::move(blobImpls),
                        std::move(aChangedKeys));
  } else {
    mSharedData =
        new SharedMap(ContentProcessMessageManager::Get()->GetParentObject(),
                      aMapFile, aMapSize, std::move(blobImpls));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvFontListChanged() {
  gfxPlatformFontList::PlatformFontList()->FontListChanged();

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvGeolocationUpdate(
    nsIDOMGeoPosition* aPosition) {
  RefPtr<nsGeolocationService> gs =
      nsGeolocationService::GetGeolocationService();
  if (!gs) {
    return IPC_OK();
  }
  gs->Update(aPosition);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvGeolocationError(
    const uint16_t& errorCode) {
  RefPtr<nsGeolocationService> gs =
      nsGeolocationService::GetGeolocationService();
  if (!gs) {
    return IPC_OK();
  }
  gs->NotifyError(errorCode);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUpdateDictionaryList(
    nsTArray<nsString>&& aDictionaries) {
  mAvailableDictionaries = aDictionaries;
  mozInlineSpellChecker::UpdateCanEnableInlineSpellChecking();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUpdateFontList(
    nsTArray<SystemFontListEntry>&& aFontList) {
  mFontList = std::move(aFontList);
  gfxPlatform::GetPlatform()->UpdateFontList();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRebuildFontList() {
  gfxPlatform::GetPlatform()->UpdateFontList();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUpdateAppLocales(
    nsTArray<nsCString>&& aAppLocales) {
  LocaleService::GetInstance()->AssignAppLocales(aAppLocales);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUpdateRequestedLocales(
    nsTArray<nsCString>&& aRequestedLocales) {
  LocaleService::GetInstance()->AssignRequestedLocales(aRequestedLocales);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvAddPermission(
    const IPC::Permission& permission) {
  nsCOMPtr<nsIPermissionManager> permissionManagerIface =
      services::GetPermissionManager();
  nsPermissionManager* permissionManager =
      static_cast<nsPermissionManager*>(permissionManagerIface.get());
  MOZ_ASSERT(permissionManager,
             "We have no permissionManager in the Content process !");

  // note we do not need to force mUserContextId to the default here because
  // the permission manager does that internally.
  nsAutoCString originNoSuffix;
  OriginAttributes attrs;
  bool success = attrs.PopulateFromOrigin(permission.origin, originNoSuffix);
  NS_ENSURE_TRUE(success, IPC_FAIL_NO_REASON(this));

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateContentPrincipal(uri, attrs);

  // child processes don't care about modification time.
  int64_t modificationTime = 0;

  permissionManager->AddInternal(
      principal, nsCString(permission.type), permission.capability, 0,
      permission.expireType, permission.expireTime, modificationTime,
      nsPermissionManager::eNotify, nsPermissionManager::eNoDBOperation);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRemoveAllPermissions() {
  nsCOMPtr<nsIPermissionManager> permissionManagerIface =
      services::GetPermissionManager();
  nsPermissionManager* permissionManager =
      static_cast<nsPermissionManager*>(permissionManagerIface.get());
  MOZ_ASSERT(permissionManager,
             "We have no permissionManager in the Content process !");

  permissionManager->RemoveAllFromIPC();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvFlushMemory(const nsString& reason) {
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "memory-pressure", reason.get());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvActivateA11y(
    const uint32_t& aMainChromeTid, const uint32_t& aMsaaID) {
#ifdef ACCESSIBILITY
#  ifdef XP_WIN
  MOZ_ASSERT(aMainChromeTid != 0);
  mMainChromeTid = aMainChromeTid;

  MOZ_ASSERT(aMsaaID != 0);
  mMsaaID = aMsaaID;
#  endif  // XP_WIN

  // Start accessibility in content process if it's running in chrome
  // process.
  GetOrCreateAccService(nsAccessibilityService::eMainProcess);
#endif  // ACCESSIBILITY
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvShutdownA11y() {
#ifdef ACCESSIBILITY
  // Try to shutdown accessibility in content process if it's shutting down in
  // chrome process.
  MaybeShutdownAccService(nsAccessibilityService::eMainProcess);
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvGarbageCollect() {
  // Rebroadcast the "child-gc-request" so that workers will GC.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "child-gc-request", nullptr);
  }
  nsJSContext::GarbageCollectNow(JS::GCReason::DOM_IPC);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvCycleCollect() {
  // Rebroadcast the "child-cc-request" so that workers will CC.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "child-cc-request", nullptr);
  }
  nsJSContext::CycleCollectNow();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUnlinkGhosts() {
#ifdef DEBUG
  nsWindowMemoryReporter::UnlinkGhostWindows();
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvAppInfo(
    const nsCString& version, const nsCString& buildID, const nsCString& name,
    const nsCString& UAName, const nsCString& ID, const nsCString& vendor,
    const nsCString& sourceURL) {
  mAppInfo.version.Assign(version);
  mAppInfo.buildID.Assign(buildID);
  mAppInfo.name.Assign(name);
  mAppInfo.UAName.Assign(UAName);
  mAppInfo.ID.Assign(ID);
  mAppInfo.vendor.Assign(vendor);
  mAppInfo.sourceURL.Assign(sourceURL);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRemoteType(
    const nsString& aRemoteType) {
  MOZ_ASSERT(DOMStringIsNull(mRemoteType));

  mRemoteType.Assign(aRemoteType);

  // For non-default ("web") types, update the process name so about:memory's
  // process names are more obvious.
  if (aRemoteType.EqualsLiteral(FILE_REMOTE_TYPE)) {
    SetProcessName(NS_LITERAL_STRING("file:// Content"));
  } else if (aRemoteType.EqualsLiteral(EXTENSION_REMOTE_TYPE)) {
    SetProcessName(NS_LITERAL_STRING("WebExtensions"));
  } else if (aRemoteType.EqualsLiteral(PRIVILEGEDABOUT_REMOTE_TYPE)) {
    SetProcessName(NS_LITERAL_STRING("Privileged Content"));
  } else if (aRemoteType.EqualsLiteral(LARGE_ALLOCATION_REMOTE_TYPE)) {
    SetProcessName(NS_LITERAL_STRING("Large Allocation Web Content"));
  }

  return IPC_OK();
}

// Call RemoteTypePrefix() on the result to remove URIs if you want to use this
// for telemetry.
const nsAString& ContentChild::GetRemoteType() const { return mRemoteType; }

mozilla::ipc::IPCResult ContentChild::RecvInitServiceWorkers(
    const ServiceWorkerConfiguration& aConfig) {
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown began
    return IPC_OK();
  }
  swm->LoadRegistrations(aConfig.serviceWorkerRegistrations());
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvInitBlobURLs(
    nsTArray<BlobURLRegistrationData>&& aRegistrations) {
  for (uint32_t i = 0; i < aRegistrations.Length(); ++i) {
    BlobURLRegistrationData& registration = aRegistrations[i];
    RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(registration.blob());
    MOZ_ASSERT(blobImpl);

    BlobURLProtocolHandler::AddDataEntry(registration.url(),
                                         registration.principal(), blobImpl);
    // If we have received an already-revoked blobURL, we have to keep it alive
    // for a while (see BlobURLProtocolHandler) in order to support pending
    // operations such as navigation, download and so on.
    if (registration.revoked()) {
      BlobURLProtocolHandler::RemoveDataEntry(registration.url(), false);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvInitJSWindowActorInfos(
    nsTArray<JSWindowActorInfo>&& aInfos) {
  RefPtr<JSWindowActorService> actSvc = JSWindowActorService::GetSingleton();
  actSvc->LoadJSWindowActorInfos(aInfos);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUnregisterJSWindowActor(
    const nsString& aName) {
  RefPtr<JSWindowActorService> actSvc = JSWindowActorService::GetSingleton();
  actSvc->UnregisterWindowActor(aName);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvLastPrivateDocShellDestroyed() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->NotifyObservers(nullptr, "last-pb-context-exited", nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvNotifyProcessPriorityChanged(
    const hal::ProcessPriority& aPriority) {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  NS_ENSURE_TRUE(os, IPC_OK());

  RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
  props->SetPropertyAsInt32(NS_LITERAL_STRING("priority"),
                            static_cast<int32_t>(aPriority));

  os->NotifyObservers(static_cast<nsIPropertyBag2*>(props),
                      "ipc:process-priority-changed", nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvMinimizeMemoryUsage() {
  nsCOMPtr<nsIMemoryReporterManager> mgr =
      do_GetService("@mozilla.org/memory-reporter-manager;1");
  NS_ENSURE_TRUE(mgr, IPC_OK());

  Unused << mgr->MinimizeMemoryUsage(/* callback = */ nullptr);
  return IPC_OK();
}

void ContentChild::AddIdleObserver(nsIObserver* aObserver,
                                   uint32_t aIdleTimeInS) {
  MOZ_ASSERT(aObserver, "null idle observer");
  // Make sure aObserver isn't released while we wait for the parent
  aObserver->AddRef();
  SendAddIdleObserver(reinterpret_cast<uint64_t>(aObserver), aIdleTimeInS);
  mIdleObservers.PutEntry(aObserver);
}

void ContentChild::RemoveIdleObserver(nsIObserver* aObserver,
                                      uint32_t aIdleTimeInS) {
  MOZ_ASSERT(aObserver, "null idle observer");
  SendRemoveIdleObserver(reinterpret_cast<uint64_t>(aObserver), aIdleTimeInS);
  aObserver->Release();
  mIdleObservers.RemoveEntry(aObserver);
}

mozilla::ipc::IPCResult ContentChild::RecvNotifyIdleObserver(
    const uint64_t& aObserver, const nsCString& aTopic,
    const nsString& aTimeStr) {
  nsIObserver* observer = reinterpret_cast<nsIObserver*>(aObserver);
  if (mIdleObservers.Contains(observer)) {
    observer->Observe(nullptr, aTopic.get(), aTimeStr.get());
  } else {
    NS_WARNING("Received notification for an idle observer that was removed.");
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvLoadAndRegisterSheet(
    const URIParams& aURI, const uint32_t& aType) {
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) {
    return IPC_OK();
  }

  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();
  if (sheetService) {
    sheetService->LoadAndRegisterSheet(uri, aType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUnregisterSheet(
    const URIParams& aURI, const uint32_t& aType) {
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) {
    return IPC_OK();
  }

  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();
  if (sheetService) {
    sheetService->UnregisterSheet(uri, aType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvDomainSetChanged(
    const uint32_t& aSetType, const uint32_t& aChangeType,
    const Maybe<URIParams>& aDomain) {
  if (aChangeType == ACTIVATE_POLICY) {
    if (mPolicy) {
      return IPC_OK();
    }
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(ssm);
    ssm->ActivateDomainPolicyInternal(getter_AddRefs(mPolicy));
    if (!mPolicy) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }
  if (!mPolicy) {
    MOZ_ASSERT_UNREACHABLE(
        "If the domain policy is not active yet,"
        " the first message should be ACTIVATE_POLICY");
    return IPC_FAIL_NO_REASON(this);
  }

  NS_ENSURE_TRUE(mPolicy, IPC_FAIL_NO_REASON(this));

  if (aChangeType == DEACTIVATE_POLICY) {
    mPolicy->Deactivate();
    mPolicy = nullptr;
    return IPC_OK();
  }

  nsCOMPtr<nsIDomainSet> set;
  switch (aSetType) {
    case BLOCKLIST:
      mPolicy->GetBlocklist(getter_AddRefs(set));
      break;
    case SUPER_BLOCKLIST:
      mPolicy->GetSuperBlocklist(getter_AddRefs(set));
      break;
    case ALLOWLIST:
      mPolicy->GetAllowlist(getter_AddRefs(set));
      break;
    case SUPER_ALLOWLIST:
      mPolicy->GetSuperAllowlist(getter_AddRefs(set));
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected setType");
      return IPC_FAIL_NO_REASON(this);
  }

  MOZ_ASSERT(set);

  nsCOMPtr<nsIURI> uri = DeserializeURI(aDomain);

  switch (aChangeType) {
    case ADD_DOMAIN:
      NS_ENSURE_TRUE(uri, IPC_FAIL_NO_REASON(this));
      set->Add(uri);
      break;
    case REMOVE_DOMAIN:
      NS_ENSURE_TRUE(uri, IPC_FAIL_NO_REASON(this));
      set->Remove(uri);
      break;
    case CLEAR_DOMAINS:
      set->Clear();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected changeType");
      return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

void ContentChild::StartForceKillTimer() {
  if (mForceKillTimer) {
    return;
  }

  int32_t timeoutSecs = StaticPrefs::dom_ipc_tabs_shutdownTimeoutSecs();
  if (timeoutSecs > 0) {
    NS_NewTimerWithFuncCallback(getter_AddRefs(mForceKillTimer),
                                ContentChild::ForceKillTimerCallback, this,
                                timeoutSecs * 1000, nsITimer::TYPE_ONE_SHOT,
                                "dom::ContentChild::StartForceKillTimer");
    MOZ_ASSERT(mForceKillTimer);
  }
}

/* static */
void ContentChild::ForceKillTimerCallback(nsITimer* aTimer, void* aClosure) {
  ProcessChild::QuickExit();
}

mozilla::ipc::IPCResult ContentChild::RecvShutdown() {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(this, "content-child-will-shutdown", nullptr);
  }

  ShutdownInternal();
  return IPC_OK();
}

void ContentChild::ShutdownInternal() {
  // If we receive the shutdown message from within a nested event loop, we want
  // to wait for that event loop to finish. Otherwise we could prematurely
  // terminate an "unload" or "pagehide" event handler (which might be doing a
  // sync XHR, for example).
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::IPCShutdownState,
      NS_LITERAL_CSTRING("RecvShutdown"));

  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<nsThread> mainThread = nsThreadManager::get().GetCurrentThread();
  // Note that we only have to check the recursion count for the current
  // cooperative thread. Since the Shutdown message is not labeled with a
  // SchedulerGroup, there can be no other cooperative threads doing work while
  // we're running.
  if (mainThread && mainThread->RecursionDepth() > 1) {
    // We're in a nested event loop. Let's delay for an arbitrary period of
    // time (100ms) in the hopes that the event loop will have finished by
    // then.
    MessageLoop::current()->PostDelayedTask(
        NewRunnableMethod("dom::ContentChild::RecvShutdown", this,
                          &ContentChild::ShutdownInternal),
        100);
    return;
  }

  mShuttingDown = true;

#ifdef NIGHTLY_BUILD
  BackgroundHangMonitor::UnregisterAnnotator(
      PendingInputEventHangAnnotator::sSingleton);
#endif

  if (mPolicy) {
    mPolicy->Deactivate();
    mPolicy = nullptr;
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(this, "content-child-shutdown", nullptr);
  }

#if defined(XP_WIN)
  mozilla::widget::StopAudioSession();
#endif

  GetIPCChannel()->SetAbortOnError(false);

#ifdef MOZ_GECKO_PROFILER
  if (mProfilerController) {
    nsCString shutdownProfile =
        mProfilerController->GrabShutdownProfileAndShutdown();
    mProfilerController = nullptr;
    // Send the shutdown profile to the parent process through our own
    // message channel, which we know will survive for long enough.
    Unused << SendShutdownProfile(shutdownProfile);
  }
#endif

  // Start a timer that will insure we quickly exit after a reasonable
  // period of time. Prevents shutdown hangs after our connection to the
  // parent closes.
  StartForceKillTimer();

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::IPCShutdownState,
      NS_LITERAL_CSTRING("SendFinishShutdown (sending)"));
  bool sent = SendFinishShutdown();
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::IPCShutdownState,
      sent ? NS_LITERAL_CSTRING("SendFinishShutdown (sent)")
           : NS_LITERAL_CSTRING("SendFinishShutdown (failed)"));
}

PBrowserOrId ContentChild::GetBrowserOrId(BrowserChild* aBrowserChild) {
  if (!aBrowserChild || this == aBrowserChild->Manager()) {
    return PBrowserOrId(aBrowserChild);
  }
  return PBrowserOrId(aBrowserChild->GetTabId());
}

mozilla::ipc::IPCResult ContentChild::RecvUpdateWindow(
    const uintptr_t& aChildId) {
#if defined(XP_WIN)
  NS_ASSERTION(aChildId,
               "Expected child hwnd value for remote plugin instance.");
  mozilla::plugins::PluginInstanceParent* parentInstance =
      mozilla::plugins::PluginInstanceParent::LookupPluginInstanceByID(
          aChildId);
  if (parentInstance) {
    // sync! update call to the plugin instance that forces the
    // plugin to paint its child window.
    if (!parentInstance->CallUpdateWindow()) {
      return IPC_FAIL_NO_REASON(this);
    }
  }
  return IPC_OK();
#else
  MOZ_ASSERT(
      false,
      "ContentChild::RecvUpdateWindow calls unexpected on this platform.");
  return IPC_FAIL_NO_REASON(this);
#endif
}

PContentPermissionRequestChild*
ContentChild::AllocPContentPermissionRequestChild(
    const nsTArray<PermissionRequest>& aRequests,
    const IPC::Principal& aPrincipal, const IPC::Principal& aTopLevelPrincipal,
    const bool& aIsHandlingUserInput, const bool& aDocumentHasUserInput,
    const DOMTimeStamp aPageLoadTimestamp, const TabId& aTabId) {
  MOZ_CRASH("unused");
  return nullptr;
}

bool ContentChild::DeallocPContentPermissionRequestChild(
    PContentPermissionRequestChild* actor) {
  nsContentPermissionUtils::NotifyRemoveContentPermissionRequestChild(actor);
  auto child = static_cast<RemotePermissionRequest*>(actor);
  child->IPDLRelease();
  return true;
}

PWebBrowserPersistDocumentChild*
ContentChild::AllocPWebBrowserPersistDocumentChild(
    PBrowserChild* aBrowser, const uint64_t& aOuterWindowID) {
  return new WebBrowserPersistDocumentChild();
}

mozilla::ipc::IPCResult ContentChild::RecvPWebBrowserPersistDocumentConstructor(
    PWebBrowserPersistDocumentChild* aActor, PBrowserChild* aBrowser,
    const uint64_t& aOuterWindowID) {
  if (NS_WARN_IF(!aBrowser)) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<Document> rootDoc =
      static_cast<BrowserChild*>(aBrowser)->GetTopLevelDocument();
  nsCOMPtr<Document> foundDoc;
  if (aOuterWindowID) {
    foundDoc = nsContentUtils::GetSubdocumentWithOuterWindowId(rootDoc,
                                                               aOuterWindowID);
  } else {
    foundDoc = rootDoc;
  }

  if (!foundDoc) {
    aActor->SendInitFailure(NS_ERROR_NO_CONTENT);
  } else {
    static_cast<WebBrowserPersistDocumentChild*>(aActor)->Start(foundDoc);
  }
  return IPC_OK();
}

bool ContentChild::DeallocPWebBrowserPersistDocumentChild(
    PWebBrowserPersistDocumentChild* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult ContentChild::RecvSetAudioSessionData(
    const nsID& aId, const nsString& aDisplayName, const nsString& aIconPath) {
#if defined(XP_WIN)
  if (NS_FAILED(mozilla::widget::RecvAudioSessionData(aId, aDisplayName,
                                                      aIconPath))) {
    return IPC_OK();
  }

  // Ignore failures here; we can't really do anything about them
  mozilla::widget::StartAudioSession();
  return IPC_OK();
#else
  MOZ_CRASH("Not Reached!");
#endif
}

mozilla::ipc::IPCResult ContentChild::RecvInvokeDragSession(
    nsTArray<IPCDataTransfer>&& aTransfers, const uint32_t& aAction) {
  nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService) {
    dragService->StartDragSession();
    nsCOMPtr<nsIDragSession> session;
    dragService->GetCurrentSession(getter_AddRefs(session));
    if (session) {
      session->SetDragAction(aAction);
      // Check if we are receiving any file objects. If we are we will want
      // to hide any of the other objects coming in from content.
      bool hasFiles = false;
      for (uint32_t i = 0; i < aTransfers.Length() && !hasFiles; ++i) {
        auto& items = aTransfers[i].items();
        for (uint32_t j = 0; j < items.Length() && !hasFiles; ++j) {
          if (items[j].data().type() == IPCDataTransferData::TIPCBlob) {
            hasFiles = true;
          }
        }
      }

      // Add the entries from the IPC to the new DataTransfer
      nsCOMPtr<DataTransfer> dataTransfer =
          new DataTransfer(nullptr, eDragStart, false, -1);
      for (uint32_t i = 0; i < aTransfers.Length(); ++i) {
        auto& items = aTransfers[i].items();
        for (uint32_t j = 0; j < items.Length(); ++j) {
          const IPCDataTransferItem& item = items[j];
          RefPtr<nsVariantCC> variant = new nsVariantCC();
          if (item.data().type() == IPCDataTransferData::TnsString) {
            const nsString& data = item.data().get_nsString();
            variant->SetAsAString(data);
          } else if (item.data().type() == IPCDataTransferData::TShmem) {
            Shmem data = item.data().get_Shmem();
            variant->SetAsACString(
                nsDependentCSubstring(data.get<char>(), data.Size<char>()));
            Unused << DeallocShmem(data);
          } else if (item.data().type() == IPCDataTransferData::TIPCBlob) {
            RefPtr<BlobImpl> blobImpl =
                IPCBlobUtils::Deserialize(item.data().get_IPCBlob());
            variant->SetAsISupports(blobImpl);
          } else {
            continue;
          }
          // We should hide this data from content if we have a file, and we
          // aren't a file.
          bool hidden =
              hasFiles && item.data().type() != IPCDataTransferData::TIPCBlob;
          dataTransfer->SetDataWithPrincipalFromOtherProcess(
              NS_ConvertUTF8toUTF16(item.flavor()), variant, i,
              nsContentUtils::GetSystemPrincipal(), hidden);
        }
      }
      session->SetDataTransfer(dataTransfer);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvEndDragSession(
    const bool& aDoneDrag, const bool& aUserCancelled,
    const LayoutDeviceIntPoint& aDragEndPoint, const uint32_t& aKeyModifiers) {
  nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService) {
    if (aUserCancelled) {
      nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
      if (dragSession) {
        dragSession->UserCancelled();
      }
    }
    static_cast<nsBaseDragService*>(dragService.get())
        ->SetDragEndPoint(aDragEndPoint);
    dragService->EndDragSession(aDoneDrag, aKeyModifiers);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvPush(const nsCString& aScope,
                                               const IPC::Principal& aPrincipal,
                                               const nsString& aMessageId) {
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Nothing());
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvPushWithData(
    const nsCString& aScope, const IPC::Principal& aPrincipal,
    const nsString& aMessageId, nsTArray<uint8_t>&& aData) {
  PushMessageDispatcher dispatcher(aScope, aPrincipal, aMessageId, Some(aData));
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvPushSubscriptionChange(
    const nsCString& aScope, const IPC::Principal& aPrincipal) {
  PushSubscriptionChangeDispatcher dispatcher(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvPushError(
    const nsCString& aScope, const IPC::Principal& aPrincipal,
    const nsString& aMessage, const uint32_t& aFlags) {
  PushErrorDispatcher dispatcher(aScope, aPrincipal, aMessage, aFlags);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObserversAndWorkers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ContentChild::RecvNotifyPushSubscriptionModifiedObservers(
    const nsCString& aScope, const IPC::Principal& aPrincipal) {
  PushSubscriptionModifiedDispatcher dispatcher(aScope, aPrincipal);
  Unused << NS_WARN_IF(NS_FAILED(dispatcher.NotifyObservers()));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvBlobURLRegistration(
    const nsCString& aURI, const IPCBlob& aBlob,
    const IPC::Principal& aPrincipal) {
  RefPtr<BlobImpl> blobImpl = IPCBlobUtils::Deserialize(aBlob);
  MOZ_ASSERT(blobImpl);

  BlobURLProtocolHandler::AddDataEntry(aURI, aPrincipal, blobImpl);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvBlobURLUnregistration(
    const nsCString& aURI) {
  BlobURLProtocolHandler::RemoveDataEntry(
      aURI,
      /* aBroadcastToOtherProcesses = */ false);
  return IPC_OK();
}

#if defined(XP_WIN) && defined(ACCESSIBILITY)
bool ContentChild::SendGetA11yContentId() {
  return PContentChild::SendGetA11yContentId(&mMsaaID);
}
#endif  // defined(XP_WIN) && defined(ACCESSIBILITY)

void ContentChild::CreateGetFilesRequest(const nsAString& aDirectoryPath,
                                         bool aRecursiveFlag, nsID& aUUID,
                                         GetFilesHelperChild* aChild) {
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(!mGetFilesPendingRequests.GetWeak(aUUID));

  Unused << SendGetFilesRequest(aUUID, nsString(aDirectoryPath),
                                aRecursiveFlag);
  mGetFilesPendingRequests.Put(aUUID, aChild);
}

void ContentChild::DeleteGetFilesRequest(nsID& aUUID,
                                         GetFilesHelperChild* aChild) {
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(mGetFilesPendingRequests.GetWeak(aUUID));

  Unused << SendDeleteGetFilesRequest(aUUID);
  mGetFilesPendingRequests.Remove(aUUID);
}

mozilla::ipc::IPCResult ContentChild::RecvGetFilesResponse(
    const nsID& aUUID, const GetFilesResponseResult& aResult) {
  GetFilesHelperChild* child = mGetFilesPendingRequests.GetWeak(aUUID);
  // This object can already been deleted in case DeleteGetFilesRequest has
  // been called when the response was sending by the parent.
  if (!child) {
    return IPC_OK();
  }

  if (aResult.type() == GetFilesResponseResult::TGetFilesResponseFailure) {
    child->Finished(aResult.get_GetFilesResponseFailure().errorCode());
  } else {
    MOZ_ASSERT(aResult.type() ==
               GetFilesResponseResult::TGetFilesResponseSuccess);

    const nsTArray<IPCBlob>& ipcBlobs =
        aResult.get_GetFilesResponseSuccess().blobs();

    bool succeeded = true;
    for (uint32_t i = 0; succeeded && i < ipcBlobs.Length(); ++i) {
      RefPtr<BlobImpl> impl = IPCBlobUtils::Deserialize(ipcBlobs[i]);
      succeeded = child->AppendBlobImpl(impl);
    }

    child->Finished(succeeded ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
  }

  mGetFilesPendingRequests.Remove(aUUID);
  return IPC_OK();
}

/* static */
void ContentChild::FatalErrorIfNotUsingGPUProcess(const char* const aErrorMsg,
                                                  base::ProcessId aOtherPid) {
  // If we're communicating with the same process or the UI process then we
  // want to crash normally. Otherwise we want to just warn as the other end
  // must be the GPU process and it crashing shouldn't be fatal for us.
  if (aOtherPid == base::GetCurrentProcId() ||
      (GetSingleton() && GetSingleton()->OtherPid() == aOtherPid)) {
    mozilla::ipc::FatalError(aErrorMsg, false);
  } else {
    nsAutoCString formattedMessage("IPDL error: \"");
    formattedMessage.AppendASCII(aErrorMsg);
    formattedMessage.AppendLiteral(R"(".)");
    NS_WARNING(formattedMessage.get());
  }
}

PURLClassifierChild* ContentChild::AllocPURLClassifierChild(
    const Principal& aPrincipal, bool* aSuccess) {
  *aSuccess = true;
  return new URLClassifierChild();
}

bool ContentChild::DeallocPURLClassifierChild(PURLClassifierChild* aActor) {
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

PURLClassifierLocalChild* ContentChild::AllocPURLClassifierLocalChild(
    const URIParams& aUri, const nsTArray<IPCURLClassifierFeature>& aFeatures) {
  return new URLClassifierLocalChild();
}

bool ContentChild::DeallocPURLClassifierLocalChild(
    PURLClassifierLocalChild* aActor) {
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

PLoginReputationChild* ContentChild::AllocPLoginReputationChild(
    const URIParams& aUri) {
  return new PLoginReputationChild();
}

bool ContentChild::DeallocPLoginReputationChild(PLoginReputationChild* aActor) {
  MOZ_ASSERT(aActor);
  delete aActor;
  return true;
}

PSessionStorageObserverChild*
ContentChild::AllocPSessionStorageObserverChild() {
  MOZ_CRASH(
      "PSessionStorageObserverChild actors should be manually constructed!");
}

bool ContentChild::DeallocPSessionStorageObserverChild(
    PSessionStorageObserverChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

mozilla::ipc::IPCResult ContentChild::RecvActivate(PBrowserChild* aTab) {
  BrowserChild* tab = static_cast<BrowserChild*>(aTab);
  return tab->RecvActivate();
}

mozilla::ipc::IPCResult ContentChild::RecvDeactivate(PBrowserChild* aTab) {
  BrowserChild* tab = static_cast<BrowserChild*>(aTab);
  return tab->RecvDeactivate();
}

mozilla::ipc::IPCResult ContentChild::RecvProvideAnonymousTemporaryFile(
    const uint64_t& aID, const FileDescOrError& aFDOrError) {
  nsAutoPtr<AnonymousTemporaryFileCallback> callback;
  mPendingAnonymousTemporaryFiles.Remove(aID, &callback);
  MOZ_ASSERT(callback);

  PRFileDesc* prfile = nullptr;
  if (aFDOrError.type() == FileDescOrError::Tnsresult) {
    DebugOnly<nsresult> rv = aFDOrError.get_nsresult();
    MOZ_ASSERT(NS_FAILED(rv));
  } else {
    auto rawFD = aFDOrError.get_FileDescriptor().ClonePlatformHandle();
    prfile = PR_ImportFile(PROsfd(rawFD.release()));
  }
  (*callback)(prfile);
  return IPC_OK();
}

nsresult ContentChild::AsyncOpenAnonymousTemporaryFile(
    const AnonymousTemporaryFileCallback& aCallback) {
  MOZ_ASSERT(NS_IsMainThread());

  static uint64_t id = 0;
  auto newID = id++;
  if (!SendRequestAnonymousTemporaryFile(newID)) {
    return NS_ERROR_FAILURE;
  }

  // Remember the association with the callback.
  MOZ_ASSERT(!mPendingAnonymousTemporaryFiles.Get(newID));
  mPendingAnonymousTemporaryFiles.LookupOrAdd(newID, aCallback);
  return NS_OK;
}

mozilla::ipc::IPCResult ContentChild::RecvSetPermissionsWithKey(
    const nsCString& aPermissionKey, nsTArray<IPC::Permission>&& aPerms) {
  nsCOMPtr<nsIPermissionManager> permissionManager =
      services::GetPermissionManager();
  permissionManager->SetPermissionsWithKey(aPermissionKey, aPerms);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRefreshScreens(
    nsTArray<ScreenDetails>&& aScreens) {
  ScreenManager& screenManager = ScreenManager::GetSingleton();
  screenManager.Refresh(std::move(aScreens));
  return IPC_OK();
}

already_AddRefed<nsIEventTarget> ContentChild::GetEventTargetFor(
    BrowserChild* aBrowserChild) {
  return IToplevelProtocol::GetActorEventTarget(aBrowserChild);
}

mozilla::ipc::IPCResult ContentChild::RecvSetPluginList(
    const uint32_t& aPluginEpoch, nsTArray<plugins::PluginTag>&& aPluginTags,
    nsTArray<plugins::FakePluginTag>&& aFakePluginTags) {
  RefPtr<nsPluginHost> host = nsPluginHost::GetInst();
  host->SetPluginsInContent(aPluginEpoch, aPluginTags, aFakePluginTags);
  return IPC_OK();
}

PClientOpenWindowOpChild* ContentChild::AllocPClientOpenWindowOpChild(
    const ClientOpenWindowArgs& aArgs) {
  return AllocClientOpenWindowOpChild();
}

IPCResult ContentChild::RecvPClientOpenWindowOpConstructor(
    PClientOpenWindowOpChild* aActor, const ClientOpenWindowArgs& aArgs) {
  InitClientOpenWindowOpChild(aActor, aArgs);
  return IPC_OK();
}

bool ContentChild::DeallocPClientOpenWindowOpChild(
    PClientOpenWindowOpChild* aActor) {
  return DeallocClientOpenWindowOpChild(aActor);
}

mozilla::ipc::IPCResult ContentChild::RecvShareCodeCoverageMutex(
    const CrossProcessMutexHandle& aHandle) {
#ifdef MOZ_CODE_COVERAGE
  CodeCoverageHandler::Init(aHandle);
  return IPC_OK();
#else
  MOZ_CRASH("Shouldn't receive this message in non-code coverage builds!");
#endif
}

mozilla::ipc::IPCResult ContentChild::RecvFlushCodeCoverageCounters(
    FlushCodeCoverageCountersResolver&& aResolver) {
#ifdef MOZ_CODE_COVERAGE
  CodeCoverageHandler::FlushCounters();
  aResolver(/* unused */ true);
  return IPC_OK();
#else
  MOZ_CRASH("Shouldn't receive this message in non-code coverage builds!");
#endif
}

mozilla::ipc::IPCResult ContentChild::RecvGetMemoryUniqueSetSize(
    GetMemoryUniqueSetSizeResolver&& aResolver) {
  MemoryTelemetry::Get().GetUniqueSetSize(std::move(aResolver));
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvSetInputEventQueueEnabled() {
  nsThreadManager::get().EnableMainThreadEventPrioritization();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvFlushInputEventQueue() {
  nsThreadManager::get().FlushInputEventPrioritization();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvSuspendInputEventQueue() {
  nsThreadManager::get().SuspendInputEventPrioritization();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvResumeInputEventQueue() {
  nsThreadManager::get().ResumeInputEventPrioritization();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvAddDynamicScalars(
    nsTArray<DynamicScalarDefinition>&& aDefs) {
  TelemetryIPC::AddDynamicScalarDefinitions(aDefs);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvSaveRecording(
    const FileDescriptor& aFile) {
  recordreplay::parent::SaveRecording(aFile);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvCrossProcessRedirect(
    const uint32_t& aRegistrarId, nsIURI* aURI,
    const ReplacementChannelConfigInit& aConfig,
    const Maybe<LoadInfoArgs>& aLoadInfo, const uint64_t& aChannelId,
    nsIURI* aOriginalURI, const uint64_t& aIdentifier,
    const uint32_t& aRedirectMode, CrossProcessRedirectResolver&& aResolve) {
  nsCOMPtr<nsILoadInfo> loadInfo;
  nsresult rv =
      mozilla::ipc::LoadInfoArgsToLoadInfo(aLoadInfo, getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    MOZ_DIAGNOSTIC_ASSERT(false, "LoadInfoArgsToLoadInfo failed");
    return IPC_OK();
  }

  nsCOMPtr<nsIChannel> newChannel;
  rv = NS_NewChannelInternal(getter_AddRefs(newChannel), aURI, loadInfo);

  // We are sure this is a HttpChannelChild because the parent
  // is always a HTTP channel.
  RefPtr<HttpChannelChild> httpChild = do_QueryObject(newChannel);
  if (NS_FAILED(rv) || !httpChild) {
    MOZ_DIAGNOSTIC_ASSERT(false, "NS_NewChannelInternal failed");
    return IPC_OK();
  }

  // This is used to report any errors back to the parent by calling
  // CrossProcessRedirectFinished.
  auto scopeExit = MakeScopeExit([&]() {
    rv = httpChild->CrossProcessRedirectFinished(rv);
    nsCOMPtr<nsILoadInfo> loadInfo;
    MOZ_ALWAYS_SUCCEEDS(newChannel->GetLoadInfo(getter_AddRefs(loadInfo)));
    Maybe<LoadInfoArgs> loadInfoArgs;
    MOZ_ALWAYS_SUCCEEDS(
        mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs));
    aResolve(
        Tuple<const nsresult&, const Maybe<LoadInfoArgs>&>(rv, loadInfoArgs));
  });

  rv = httpChild->SetChannelId(aChannelId);
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  rv = httpChild->SetOriginalURI(aOriginalURI);
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  rv = httpChild->SetRedirectMode(aRedirectMode);
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  HttpBaseChannel::ReplacementChannelConfig config(aConfig);
  HttpBaseChannel::ConfigureReplacementChannel(
      newChannel, config,
      HttpBaseChannel::ConfigureReason::DocumentChannelReplacement);

  // connect parent.
  rv = httpChild->ConnectParent(aRegistrarId);  // creates parent channel
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIChildProcessChannelListener> processListener =
      do_GetService("@mozilla.org/network/childProcessChannelListener;1");
  // The listener will call completeRedirectSetup on the channel.
  rv = processListener->OnChannelReady(httpChild, aIdentifier);
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  // scopeExit will call CrossProcessRedirectFinished(rv) here
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvStartDelayedAutoplayMediaComponents(
    BrowsingContext* aContext) {
  MOZ_ASSERT(aContext);
  aContext->StartDelayedAutoplayMediaComponents();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvUpdateMediaAction(
    BrowsingContext* aContext, MediaControlActions aAction) {
  MOZ_ASSERT(aContext);
  nsCOMPtr<nsPIDOMWindowOuter> window = aContext->GetDOMWindow();
  if (window) {
    window->UpdateMediaAction(aAction);
  }
  return IPC_OK();
}

already_AddRefed<nsIEventTarget> ContentChild::GetSpecificMessageEventTarget(
    const Message& aMsg) {
  switch (aMsg.type()) {
    // Javascript
    case PJavaScript::Msg_DropTemporaryStrongReferences__ID:
    case PJavaScript::Msg_DropObject__ID:

    // Navigation
    case PContent::Msg_NotifyVisited__ID:

    // Storage API
    case PContent::Msg_DataStoragePut__ID:
    case PContent::Msg_DataStorageRemove__ID:
    case PContent::Msg_DataStorageClear__ID:

    // Blob and BlobURL
    case PContent::Msg_BlobURLRegistration__ID:
    case PContent::Msg_BlobURLUnregistration__ID:
    case PContent::Msg_InitBlobURLs__ID:
    case PContent::Msg_PIPCBlobInputStreamConstructor__ID:
    case PContent::Msg_StoreAndBroadcastBlobURLRegistration__ID:

      return do_AddRef(SystemGroup::EventTargetFor(TaskCategory::Other));

    // PBrowserChild Construction
    case PContent::Msg_ConstructBrowser__ID: {
      // Deserialize the arguments for this message to get the endpoint and
      // `sameTabGroupAs`. The endpoint is needed to set up the event target for
      // our newly created actor, and sameTabGroupAs is needed to determine if
      // we're going to join an existing TabGroup.
      ManagedEndpoint<PBrowserChild> endpoint;
      ManagedEndpoint<PWindowGlobalChild> windowGlobalEndpoint;
      TabId tabId, sameTabGroupAs;
      PickleIterator iter(aMsg);
      if (NS_WARN_IF(!IPC::ReadParam(&aMsg, &iter, &endpoint))) {
        return nullptr;
      }
      aMsg.IgnoreSentinel(&iter);
      if (NS_WARN_IF(!IPC::ReadParam(&aMsg, &iter, &windowGlobalEndpoint))) {
        return nullptr;
      }
      aMsg.IgnoreSentinel(&iter);
      if (NS_WARN_IF(!IPC::ReadParam(&aMsg, &iter, &tabId))) {
        return nullptr;
      }
      aMsg.IgnoreSentinel(&iter);
      if (NS_WARN_IF(!IPC::ReadParam(&aMsg, &iter, &sameTabGroupAs))) {
        return nullptr;
      }

      // If sameTabGroupAs is non-zero, then the new tab will be in the same
      // TabGroup as a previously created tab. Rather than try to find the
      // previously created tab (whose constructor message may not even have
      // been processed yet, in theory) and look up its event target, we just
      // use the default event target. This means that runnables for this tab
      // will not be labeled. However, this path is only taken for print preview
      // and view source, which are not performance-sensitive.
      if (sameTabGroupAs) {
        return nullptr;
      }

      if (NS_WARN_IF(!endpoint.IsValid())) {
        return nullptr;
      }

      // If the request for a new BrowserChild is coming from the parent
      // process, then there is no opener. Therefore, we create a fresh
      // TabGroup.
      RefPtr<TabGroup> tabGroup = new TabGroup();
      nsCOMPtr<nsIEventTarget> target =
          tabGroup->EventTargetFor(TaskCategory::Other);

      // Set this event target for our newly created entry, and use it for this
      // message.
      SetEventTargetForRoute(*endpoint.ActorId(), target);

      return target.forget();
    }

    default:
      return nullptr;
  }
}

void ContentChild::OnChannelReceivedMessage(const Message& aMsg) {
  if (aMsg.is_sync() && !aMsg.is_reply()) {
    LSObject::OnSyncMessageReceived();
  }

#ifdef NIGHTLY_BUILD
  if (nsContentUtils::IsMessageInputEvent(aMsg)) {
    mPendingInputEvents++;
  }
#endif
}

#ifdef NIGHTLY_BUILD
PContentChild::Result ContentChild::OnMessageReceived(const Message& aMsg) {
  if (nsContentUtils::IsMessageInputEvent(aMsg)) {
    DebugOnly<uint32_t> prevEvts = mPendingInputEvents--;
    MOZ_ASSERT(prevEvts > 0);
  }

  return PContentChild::OnMessageReceived(aMsg);
}
#endif

PContentChild::Result ContentChild::OnMessageReceived(const Message& aMsg,
                                                      Message*& aReply) {
  Result result = PContentChild::OnMessageReceived(aMsg, aReply);

  if (aMsg.is_sync()) {
    // OnMessageReceived shouldn't be called for sync replies.
    MOZ_ASSERT(!aMsg.is_reply());

    LSObject::OnSyncMessageHandled();
  }

  return result;
}

mozilla::ipc::IPCResult ContentChild::RecvAttachBrowsingContext(
    BrowsingContext::IPCInitializer&& aInit) {
  RefPtr<BrowsingContext> child = BrowsingContext::Get(aInit.mId);
  MOZ_RELEASE_ASSERT(!child || child->IsCached());

  if (!child) {
    // Determine the BrowsingContextGroup from our parent or opener fields.
    RefPtr<BrowsingContextGroup> group =
        BrowsingContextGroup::Select(aInit.mParentId, aInit.mOpenerId);
    child = BrowsingContext::CreateFromIPC(std::move(aInit), group, nullptr);
  }

  child->Attach(/* aFromIPC */ true);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvDetachBrowsingContext(
    uint64_t aContextId, DetachBrowsingContextResolver&& aResolve) {
  // NOTE: Immediately resolve the promise, as we've received the message. This
  // will allow the parent process to discard references to this BC.
  aResolve(true);

  // If we can't find a BrowsingContext with the given ID, it's already been
  // collected and we can ignore the request.
  RefPtr<BrowsingContext> context = BrowsingContext::Get(aContextId);
  if (context) {
    context->Detach(/* aFromIPC */ true);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvCacheBrowsingContextChildren(
    BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(aContext);

  aContext->CacheChildren(/* aFromIPC */ true);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRestoreBrowsingContextChildren(
    BrowsingContext* aContext, BrowsingContext::Children&& aChildren) {
  MOZ_DIAGNOSTIC_ASSERT(aContext);

  aContext->RestoreChildren(std::move(aChildren), /* aFromIPC */ true);

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvRegisterBrowsingContextGroup(
    nsTArray<BrowsingContext::IPCInitializer>&& aInits) {
  RefPtr<BrowsingContextGroup> group = new BrowsingContextGroup();
  // Each of the initializers in aInits is sorted in pre-order, so our parent
  // should always be available before the element itself.
  for (auto& init : aInits) {
#ifdef DEBUG
    RefPtr<BrowsingContext> existing = BrowsingContext::Get(init.mId);
    MOZ_ASSERT(!existing, "BrowsingContext must not exist yet!");

    RefPtr<BrowsingContext> parent = init.GetParent();
    MOZ_ASSERT_IF(parent, parent->Group() == group);
#endif

    bool cached = init.mCached;
    RefPtr<BrowsingContext> ctxt =
        BrowsingContext::CreateFromIPC(std::move(init), group, nullptr);

    // If the browsing context is cached don't attach it, but add it
    // to the cache here as well
    if (cached) {
      ctxt->Group()->CacheContext(ctxt);
    } else {
      ctxt->Attach(/* aFromIPC */ true);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvWindowClose(BrowsingContext* aContext,
                                                      bool aTrustedCaller) {
  if (!aContext) {
    MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
            ("ChildIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = aContext->GetDOMWindow();
  if (!window) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ChildIPC: Trying to send a message to a context without a window"));
    return IPC_OK();
  }

  nsGlobalWindowOuter::Cast(window)->CloseOuter(aTrustedCaller);
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvWindowFocus(
    BrowsingContext* aContext) {
  if (!aContext) {
    MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
            ("ChildIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = aContext->GetDOMWindow();
  if (!window) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ChildIPC: Trying to send a message to a context without a window"));
    return IPC_OK();
  }
  nsGlobalWindowOuter::Cast(window)->FocusOuter();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvWindowBlur(
    BrowsingContext* aContext) {
  if (!aContext) {
    MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
            ("ChildIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = aContext->GetDOMWindow();
  if (!window) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ChildIPC: Trying to send a message to a context without a window"));
    return IPC_OK();
  }
  nsGlobalWindowOuter::Cast(window)->BlurOuter();
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvWindowPostMessage(
    BrowsingContext* aContext, const ClonedMessageData& aMessage,
    const PostMessageData& aData) {
  if (!aContext) {
    MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
            ("ChildIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }

  RefPtr<nsGlobalWindowOuter> window =
      nsGlobalWindowOuter::Cast(aContext->GetDOMWindow());
  if (!window) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ChildIPC: Trying to send a message to a context without a window"));
    return IPC_OK();
  }

  nsCOMPtr<nsIPrincipal> providedPrincipal;
  if (!window->GetPrincipalForPostMessage(
          aData.targetOrigin(), aData.targetOriginURI(),
          aData.callerPrincipal(), *aData.subjectPrincipal(),
          getter_AddRefs(providedPrincipal))) {
    return IPC_OK();
  }

  // Create and asynchronously dispatch a runnable which will handle actual DOM
  // event creation and dispatch.
  RefPtr<BrowsingContext> sourceBc = aData.source();
  RefPtr<PostMessageEvent> event = new PostMessageEvent(
      sourceBc, aData.origin(), window, providedPrincipal,
      aData.callerDocumentURI(), aData.isFromPrivateWindow());
  event->UnpackFrom(aMessage);

  window->Dispatch(TaskCategory::Other, event.forget());
  return IPC_OK();
}

mozilla::ipc::IPCResult ContentChild::RecvCommitBrowsingContextTransaction(
    BrowsingContext* aContext, BrowsingContext::Transaction&& aTransaction,
    uint64_t aEpoch) {
  if (!aContext || aContext->IsDiscarded()) {
    MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
            ("ChildIPC: Trying to send a message to dead or detached context"));
    return IPC_OK();
  }

  if (!aTransaction.ValidateEpochs(aContext, aEpoch)) {
    return IPC_FAIL(this, "Invalid BrowsingContext transaction from Parent");
  }

  aTransaction.Apply(aContext);
  return IPC_OK();
}

void ContentChild::HoldBrowsingContextGroup(BrowsingContextGroup* aBCG) {
  mBrowsingContextGroupHolder.AppendElement(aBCG);
}

void ContentChild::ReleaseBrowsingContextGroup(BrowsingContextGroup* aBCG) {
  mBrowsingContextGroupHolder.RemoveElement(aBCG);
}

mozilla::ipc::IPCResult ContentChild::RecvScriptError(
    const nsString& aMessage, const nsString& aSourceName,
    const nsString& aSourceLine, const uint32_t& aLineNumber,
    const uint32_t& aColNumber, const uint32_t& aFlags,
    const nsCString& aCategory, const bool& aFromPrivateWindow,
    const uint64_t& aInnerWindowId, const bool& aFromChromeContext) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL(this, "Failed to get console service"));

  nsCOMPtr<nsIScriptError> scriptError(
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  NS_ENSURE_TRUE(scriptError,
                 IPC_FAIL(this, "Failed to construct nsIScriptError"));

  scriptError->InitWithWindowID(aMessage, aSourceName, aSourceLine, aLineNumber,
                                aColNumber, aFlags, aCategory, aInnerWindowId,
                                aFromChromeContext);
  rv = consoleService->LogMessage(scriptError);
  NS_ENSURE_SUCCESS(rv, IPC_FAIL(this, "Failed to log script error"));

  return IPC_OK();
}

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
mozilla::ipc::IPCResult ContentChild::RecvInitSandboxTesting(
    Endpoint<PSandboxTestingChild>&& aEndpoint) {
  if (!SandboxTestingChild::Initialize(std::move(aEndpoint))) {
    return IPC_FAIL(
        this, "InitSandboxTesting failed to initialise the child process.");
  }
  return IPC_OK();
}
#endif

}  // namespace dom

#if defined(__OpenBSD__) && defined(MOZ_SANDBOX)
#  include <unistd.h>

static LazyLogModule sPledgeLog("SandboxPledge");

bool StartOpenBSDSandbox(GeckoProcessType type) {
  nsAutoCString promisesString;
  nsAutoCString processTypeString;

  switch (type) {
    case GeckoProcessType_Default:
      processTypeString = "main";
      Preferences::GetCString("security.sandbox.pledge.main", promisesString);
      break;

    case GeckoProcessType_Content:
      processTypeString = "content";
      Preferences::GetCString("security.sandbox.pledge.content",
                              promisesString);
      break;

    default:
      MOZ_ASSERT(false, "unknown process type");
      return false;
  };

  if (pledge(promisesString.get(), NULL) == -1) {
    if (errno == EINVAL) {
      MOZ_LOG(sPledgeLog, LogLevel::Error,
              ("pledge promises for %s process is a malformed string: '%s'\n",
               processTypeString.get(), promisesString.get()));
    } else if (errno == EPERM) {
      MOZ_LOG(
          sPledgeLog, LogLevel::Error,
          ("pledge promises for %s process can't elevate privileges: '%s'\n",
           processTypeString.get(), promisesString.get()));
    }
    return false;
  } else {
    MOZ_LOG(sPledgeLog, LogLevel::Debug,
            ("pledged %s process with promises: '%s'\n",
             processTypeString.get(), promisesString.get()));
  }
  return true;
}
#endif

#if !defined(XP_WIN)
bool IsDevelopmentBuild() {
  nsCOMPtr<nsIFile> path = mozilla::Omnijar::GetPath(mozilla::Omnijar::GRE);
  // If the path doesn't exist, we're a dev build.
  return path == nullptr;
}
#endif /* !XP_WIN */

}  // namespace mozilla
