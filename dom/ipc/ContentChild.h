/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/ipc/Blob.h"
#include "nsWeakPtr.h"

struct ChromePackage;
class nsIDOMBlob;
class nsIObserver;
struct ResourceMapping;
struct OverrideMapping;

namespace mozilla {

namespace ipc {
class OptionalURIParams;
class URIParams;
}// namespace ipc

namespace jsipc {
class JavaScriptChild;
}

namespace layers {
class PCompositorChild;
} // namespace layers

namespace dom {

class AlertObserver;
class PrefObserver;
class ConsoleListener;
class PStorageChild;
class ClonedMessageData;

class ContentChild : public PContentChild
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
    typedef mozilla::ipc::URIParams URIParams;

public:
    ContentChild();
    virtual ~ContentChild();
    nsrefcnt AddRef() { return 1; }
    nsrefcnt Release() { return 1; }

    struct AppInfo
    {
        nsCString version;
        nsCString buildID;
        nsCString name;
        nsCString UAName;
    };

    bool Init(MessageLoop* aIOLoop,
              base::ProcessHandle aParentHandle,
              IPC::Channel* aChannel);
    void InitXPCOM();

    static ContentChild* GetSingleton() {
        return sSingleton;
    }

    const AppInfo& GetAppInfo() {
        return mAppInfo;
    }

    void SetProcessName(const nsAString& aName);
    void GetProcessName(nsAString& aName);
    void GetProcessName(nsACString& aName);
    static void AppendProcessId(nsACString& aName);

    PCompositorChild*
    AllocPCompositorChild(mozilla::ipc::Transport* aTransport,
                          base::ProcessId aOtherProcess) MOZ_OVERRIDE;
    PImageBridgeChild*
    AllocPImageBridgeChild(mozilla::ipc::Transport* aTransport,
                           base::ProcessId aOtherProcess) MOZ_OVERRIDE;

    virtual bool RecvSetProcessPrivileges(const ChildPrivileges& aPrivs) MOZ_OVERRIDE;

    virtual PBrowserChild* AllocPBrowserChild(const IPCTabContext &aContext,
                                              const uint32_t &chromeFlags);
    virtual bool DeallocPBrowserChild(PBrowserChild*);

    virtual PDeviceStorageRequestChild* AllocPDeviceStorageRequestChild(const DeviceStorageParams&);
    virtual bool DeallocPDeviceStorageRequestChild(PDeviceStorageRequestChild*);

    virtual PBlobChild* AllocPBlobChild(const BlobConstructorParams& aParams);
    virtual bool DeallocPBlobChild(PBlobChild*);

    virtual PCrashReporterChild*
    AllocPCrashReporterChild(const mozilla::dom::NativeThreadId& id,
                             const uint32_t& processType) MOZ_OVERRIDE;
    virtual bool
    DeallocPCrashReporterChild(PCrashReporterChild*) MOZ_OVERRIDE;

    virtual PHalChild* AllocPHalChild() MOZ_OVERRIDE;
    virtual bool DeallocPHalChild(PHalChild*) MOZ_OVERRIDE;

    virtual PIndexedDBChild* AllocPIndexedDBChild() MOZ_OVERRIDE;
    virtual bool DeallocPIndexedDBChild(PIndexedDBChild* aActor) MOZ_OVERRIDE;

    virtual PMemoryReportRequestChild*
    AllocPMemoryReportRequestChild(const uint32_t& generation) MOZ_OVERRIDE;

    virtual bool
    DeallocPMemoryReportRequestChild(PMemoryReportRequestChild* actor) MOZ_OVERRIDE;

    virtual bool
    RecvPMemoryReportRequestConstructor(PMemoryReportRequestChild* child,
                                        const uint32_t& generation) MOZ_OVERRIDE;

    virtual bool
    RecvAudioChannelNotify() MOZ_OVERRIDE;

    virtual bool
    RecvDumpMemoryInfoToTempDir(const nsString& aIdentifier,
                                const bool& aMinimizeMemoryUsage,
                                const bool& aDumpChildProcesses) MOZ_OVERRIDE;
    virtual bool
    RecvDumpGCAndCCLogsToFile(const nsString& aIdentifier,
                              const bool& aDumpAllTraces,
                              const bool& aDumpChildProcesses) MOZ_OVERRIDE;

    virtual PTestShellChild* AllocPTestShellChild() MOZ_OVERRIDE;
    virtual bool DeallocPTestShellChild(PTestShellChild*) MOZ_OVERRIDE;
    virtual bool RecvPTestShellConstructor(PTestShellChild*) MOZ_OVERRIDE;
    jsipc::JavaScriptChild *GetCPOWManager();

    virtual PNeckoChild* AllocPNeckoChild() MOZ_OVERRIDE;
    virtual bool DeallocPNeckoChild(PNeckoChild*) MOZ_OVERRIDE;

    virtual PExternalHelperAppChild *AllocPExternalHelperAppChild(
            const OptionalURIParams& uri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const bool& aForceSave,
            const int64_t& aContentLength,
            const OptionalURIParams& aReferrer,
            PBrowserChild* aBrowser) MOZ_OVERRIDE;
    virtual bool DeallocPExternalHelperAppChild(PExternalHelperAppChild *aService) MOZ_OVERRIDE;

    virtual PSmsChild* AllocPSmsChild() MOZ_OVERRIDE;
    virtual bool DeallocPSmsChild(PSmsChild*) MOZ_OVERRIDE;

    virtual PTelephonyChild* AllocPTelephonyChild() MOZ_OVERRIDE;
    virtual bool DeallocPTelephonyChild(PTelephonyChild*) MOZ_OVERRIDE;

    virtual PStorageChild* AllocPStorageChild() MOZ_OVERRIDE;
    virtual bool DeallocPStorageChild(PStorageChild* aActor) MOZ_OVERRIDE;

    virtual PBluetoothChild* AllocPBluetoothChild() MOZ_OVERRIDE;
    virtual bool DeallocPBluetoothChild(PBluetoothChild* aActor) MOZ_OVERRIDE;

    virtual PFMRadioChild* AllocPFMRadioChild() MOZ_OVERRIDE;
    virtual bool DeallocPFMRadioChild(PFMRadioChild* aActor) MOZ_OVERRIDE;

    virtual PAsmJSCacheEntryChild* AllocPAsmJSCacheEntryChild(
                                 const asmjscache::OpenMode& aOpenMode,
                                 const asmjscache::WriteParams& aWriteParams,
                                 const IPC::Principal& aPrincipal) MOZ_OVERRIDE;
    virtual bool DeallocPAsmJSCacheEntryChild(
                                    PAsmJSCacheEntryChild* aActor) MOZ_OVERRIDE;

    virtual PSpeechSynthesisChild* AllocPSpeechSynthesisChild() MOZ_OVERRIDE;
    virtual bool DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor) MOZ_OVERRIDE;

    virtual bool RecvRegisterChrome(const InfallibleTArray<ChromePackage>& packages,
                                    const InfallibleTArray<ResourceMapping>& resources,
                                    const InfallibleTArray<OverrideMapping>& overrides,
                                    const nsCString& locale) MOZ_OVERRIDE;

    virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() MOZ_OVERRIDE;
    virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) MOZ_OVERRIDE;

    virtual bool RecvSetOffline(const bool& offline) MOZ_OVERRIDE;

    virtual bool RecvSpeakerManagerNotify() MOZ_OVERRIDE;

    virtual bool RecvNotifyVisited(const URIParams& aURI) MOZ_OVERRIDE;
    // auto remove when alertfinished is received.
    nsresult AddRemoteAlertObserver(const nsString& aData, nsIObserver* aObserver);

    virtual bool RecvPreferenceUpdate(const PrefSetting& aPref) MOZ_OVERRIDE;

    virtual bool RecvNotifyAlertsObserver(const nsCString& aType,
                                          const nsString& aData) MOZ_OVERRIDE;

    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  const IPC::Principal& aPrincipal) MOZ_OVERRIDE;

    virtual bool RecvGeolocationUpdate(const GeoPosition& somewhere) MOZ_OVERRIDE;

    virtual bool RecvAddPermission(const IPC::Permission& permission) MOZ_OVERRIDE;

    virtual bool RecvScreenSizeChanged(const gfxIntSize &size) MOZ_OVERRIDE;

    virtual bool RecvFlushMemory(const nsString& reason) MOZ_OVERRIDE;

    virtual bool RecvActivateA11y() MOZ_OVERRIDE;

    virtual bool RecvGarbageCollect() MOZ_OVERRIDE;
    virtual bool RecvCycleCollect() MOZ_OVERRIDE;

    virtual bool RecvAppInfo(const nsCString& version, const nsCString& buildID,
                             const nsCString& name, const nsCString& UAName) MOZ_OVERRIDE;

    virtual bool RecvLastPrivateDocShellDestroyed() MOZ_OVERRIDE;

    virtual bool RecvFilePathUpdate(const nsString& aStorageType,
                                    const nsString& aStorageName,
                                    const nsString& aPath,
                                    const nsCString& aReason) MOZ_OVERRIDE;
    virtual bool RecvFileSystemUpdate(const nsString& aFsName,
                                      const nsString& aVolumeName,
                                      const int32_t& aState,
                                      const int32_t& aMountGeneration,
                                      const bool& aIsMediaPresent,
                                      const bool& aIsSharing,
                                      const bool& aIsFormatting) MOZ_OVERRIDE;

    virtual bool RecvNuwaFork() MOZ_OVERRIDE;

    virtual bool
    RecvNotifyProcessPriorityChanged(const hal::ProcessPriority& aPriority) MOZ_OVERRIDE;
    virtual bool RecvMinimizeMemoryUsage() MOZ_OVERRIDE;
    virtual bool RecvCancelMinimizeMemoryUsage() MOZ_OVERRIDE;

    virtual bool RecvLoadAndRegisterSheet(const URIParams& aURI,
                                          const uint32_t& aType) MOZ_OVERRIDE;
    virtual bool RecvUnregisterSheet(const URIParams& aURI, const uint32_t& aType) MOZ_OVERRIDE;

    virtual bool RecvNotifyPhoneStateChange(const nsString& state) MOZ_OVERRIDE;

    void AddIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);
    void RemoveIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);
    virtual bool RecvNotifyIdleObserver(const uint64_t& aObserver,
                                        const nsCString& aTopic,
                                        const nsString& aData) MOZ_OVERRIDE;
#ifdef ANDROID
    gfxIntSize GetScreenSize() { return mScreenSize; }
#endif

    // Get the directory for IndexedDB files. We query the parent for this and
    // cache the value
    nsString &GetIndexedDBPath();

    uint64_t GetID() { return mID; }

    bool IsForApp() { return mIsForApp; }
    bool IsForBrowser() { return mIsForBrowser; }

    BlobChild* GetOrCreateActorForBlob(nsIDOMBlob* aBlob);

protected:
    virtual bool RecvPBrowserConstructor(PBrowserChild* actor,
                                         const IPCTabContext& context,
                                         const uint32_t& chromeFlags) MOZ_OVERRIDE;

private:
    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    virtual void ProcessingError(Result what) MOZ_OVERRIDE;

    /**
     * Exit *now*.  Do not shut down XPCOM, do not pass Go, do not run
     * static destructors, do not collect $200.
     */
    MOZ_NORETURN void QuickExit();

    InfallibleTArray<nsAutoPtr<AlertObserver> > mAlertObservers;
    nsRefPtr<ConsoleListener> mConsoleListener;

    /**
     * An ID unique to the process containing our corresponding
     * content parent.
     *
     * We expect our content parent to set this ID immediately after opening a
     * channel to us.
     */
    uint64_t mID;

    AppInfo mAppInfo;

#ifdef ANDROID
    gfxIntSize mScreenSize;
#endif

    bool mIsForApp;
    bool mIsForBrowser;
    nsString mProcessName;
    nsWeakPtr mMemoryMinimizerRunnable;

    static ContentChild* sSingleton;

    DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

} // namespace dom
} // namespace mozilla

#endif
