/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/PBrowserOrId.h"
#include "mozilla/dom/PContentChild.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsTHashtable.h"

#include "nsWeakPtr.h"


struct ChromePackage;
class nsIObserver;
struct ResourceMapping;
struct OverrideMapping;
class nsIDomainPolicy;

namespace mozilla {
class RemoteSpellcheckEngineChild;

namespace ipc {
class OptionalURIParams;
class PFileDescriptorSetChild;
class URIParams;
}// namespace ipc

namespace layers {
class PCompositorChild;
} // namespace layers

namespace dom {

class AlertObserver;
class ConsoleListener;
class PStorageChild;
class ClonedMessageData;
class TabChild;

class ContentChild final : public PContentChild
                         , public nsIContentChild
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
    typedef mozilla::ipc::PFileDescriptorSetChild PFileDescriptorSetChild;
    typedef mozilla::ipc::URIParams URIParams;

public:
    ContentChild();
    virtual ~ContentChild();
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
    NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override { return 1; }
    NS_IMETHOD_(MozExternalRefCountType) Release(void) override { return 1; }

    struct AppInfo
    {
        nsCString version;
        nsCString buildID;
        nsCString name;
        nsCString UAName;
        nsCString ID;
        nsCString vendor;
    };

    bool Init(MessageLoop* aIOLoop,
              base::ProcessId aParentPid,
              IPC::Channel* aChannel);
    void InitProcessAttributes();
    void InitXPCOM();

    static ContentChild* GetSingleton() {
        return sSingleton;
    }

    const AppInfo& GetAppInfo() {
        return mAppInfo;
    }
    void SetProcessName(const nsAString& aName, bool aDontOverride = false);
    void GetProcessName(nsAString& aName);
    void GetProcessName(nsACString& aName);
    bool IsAlive();
    static void AppendProcessId(nsACString& aName);

    ContentBridgeParent* GetLastBridge() {
        MOZ_ASSERT(mLastBridge);
        ContentBridgeParent* parent = mLastBridge;
        mLastBridge = nullptr;
        return parent;
    }
    nsRefPtr<ContentBridgeParent> mLastBridge;

    PPluginModuleParent *
    AllocPPluginModuleParent(mozilla::ipc::Transport* transport,
                             base::ProcessId otherProcess) override;

    PContentBridgeParent*
    AllocPContentBridgeParent(mozilla::ipc::Transport* transport,
                              base::ProcessId otherProcess) override;
    PContentBridgeChild*
    AllocPContentBridgeChild(mozilla::ipc::Transport* transport,
                             base::ProcessId otherProcess) override;

    PGMPServiceChild*
    AllocPGMPServiceChild(mozilla::ipc::Transport* transport,
                          base::ProcessId otherProcess) override;

    PCompositorChild*
    AllocPCompositorChild(mozilla::ipc::Transport* aTransport,
                          base::ProcessId aOtherProcess) override;

    PSharedBufferManagerChild*
    AllocPSharedBufferManagerChild(mozilla::ipc::Transport* aTransport,
                                    base::ProcessId aOtherProcess) override;

    PImageBridgeChild*
    AllocPImageBridgeChild(mozilla::ipc::Transport* aTransport,
                           base::ProcessId aOtherProcess) override;

    PProcessHangMonitorChild*
    AllocPProcessHangMonitorChild(Transport* aTransport,
                                  ProcessId aOtherProcess) override;

    virtual bool RecvSetProcessSandbox() override;

    PBackgroundChild*
    AllocPBackgroundChild(Transport* aTransport, ProcessId aOtherProcess)
                          override;

    virtual PBrowserChild* AllocPBrowserChild(const TabId& aTabId,
                                              const IPCTabContext& aContext,
                                              const uint32_t& aChromeFlags,
                                              const ContentParentId& aCpID,
                                              const bool& aIsForApp,
                                              const bool& aIsForBrowser)
                                              override;
    virtual bool DeallocPBrowserChild(PBrowserChild*) override;

    virtual PDeviceStorageRequestChild* AllocPDeviceStorageRequestChild(const DeviceStorageParams&)
                                                                        override;
    virtual bool DeallocPDeviceStorageRequestChild(PDeviceStorageRequestChild*)
                                                   override;

    virtual PFileSystemRequestChild* AllocPFileSystemRequestChild(const FileSystemParams&)
                                                                  override;
    virtual bool DeallocPFileSystemRequestChild(PFileSystemRequestChild*) override;

    virtual PBlobChild* AllocPBlobChild(const BlobConstructorParams& aParams)
                                        override;
    virtual bool DeallocPBlobChild(PBlobChild* aActor) override;

    virtual PCrashReporterChild*
    AllocPCrashReporterChild(const mozilla::dom::NativeThreadId& id,
                             const uint32_t& processType) override;
    virtual bool
    DeallocPCrashReporterChild(PCrashReporterChild*) override;

    virtual PHalChild* AllocPHalChild() override;
    virtual bool DeallocPHalChild(PHalChild*) override;

    PIccChild*
    SendPIccConstructor(PIccChild* aActor, const uint32_t& aServiceId);
    virtual PIccChild*
    AllocPIccChild(const uint32_t& aClientId) override;
    virtual bool
    DeallocPIccChild(PIccChild* aActor) override;

    virtual PMemoryReportRequestChild*
    AllocPMemoryReportRequestChild(const uint32_t& aGeneration,
                                   const bool& aAnonymize,
                                   const bool& aMinimizeMemoryUsage,
                                   const MaybeFileDesc& aDMDFile) override;
    virtual bool
    DeallocPMemoryReportRequestChild(PMemoryReportRequestChild* actor) override;

    virtual bool
    RecvPMemoryReportRequestConstructor(PMemoryReportRequestChild* aChild,
                                        const uint32_t& aGeneration,
                                        const bool& aAnonymize,
                                        const bool &aMinimizeMemoryUsage,
                                        const MaybeFileDesc &aDMDFile) override;

    virtual PCycleCollectWithLogsChild*
    AllocPCycleCollectWithLogsChild(const bool& aDumpAllTraces,
                                    const FileDescriptor& aGCLog,
                                    const FileDescriptor& aCCLog) override;
    virtual bool
    DeallocPCycleCollectWithLogsChild(PCycleCollectWithLogsChild* aActor) override;
    virtual bool
    RecvPCycleCollectWithLogsConstructor(PCycleCollectWithLogsChild* aChild,
                                         const bool& aDumpAllTraces,
                                         const FileDescriptor& aGCLog,
                                         const FileDescriptor& aCCLog) override;

    virtual bool
    RecvAudioChannelNotify() override;

    virtual bool
    RecvDataStoreNotify(const uint32_t& aAppId, const nsString& aName,
                        const nsString& aManifestURL) override;

    virtual PTestShellChild* AllocPTestShellChild() override;
    virtual bool DeallocPTestShellChild(PTestShellChild*) override;
    virtual bool RecvPTestShellConstructor(PTestShellChild*) override;
    jsipc::CPOWManager* GetCPOWManager() override;

    PMobileConnectionChild*
    SendPMobileConnectionConstructor(PMobileConnectionChild* aActor,
                                     const uint32_t& aClientId);
    virtual PMobileConnectionChild*
    AllocPMobileConnectionChild(const uint32_t& aClientId) override;
    virtual bool
    DeallocPMobileConnectionChild(PMobileConnectionChild* aActor) override;

    virtual PNeckoChild* AllocPNeckoChild() override;
    virtual bool DeallocPNeckoChild(PNeckoChild*) override;

    virtual PPrintingChild* AllocPPrintingChild() override;
    virtual bool DeallocPPrintingChild(PPrintingChild*) override;

    virtual PScreenManagerChild*
    AllocPScreenManagerChild(uint32_t* aNumberOfScreens,
                             float* aSystemDefaultScale,
                             bool* aSuccess) override;
    virtual bool DeallocPScreenManagerChild(PScreenManagerChild*) override;

    virtual PPSMContentDownloaderChild* AllocPPSMContentDownloaderChild(
            const uint32_t& aCertType) override;
    virtual bool DeallocPPSMContentDownloaderChild(PPSMContentDownloaderChild* aDownloader) override;

    virtual PExternalHelperAppChild *AllocPExternalHelperAppChild(
            const OptionalURIParams& uri,
            const nsCString& aMimeContentType,
            const nsCString& aContentDisposition,
            const uint32_t& aContentDispositionHint,
            const nsString& aContentDispositionFilename,
            const bool& aForceSave,
            const int64_t& aContentLength,
            const OptionalURIParams& aReferrer,
            PBrowserChild* aBrowser) override;
    virtual bool DeallocPExternalHelperAppChild(PExternalHelperAppChild *aService) override;

    virtual PCellBroadcastChild* AllocPCellBroadcastChild() override;
    PCellBroadcastChild* SendPCellBroadcastConstructor(PCellBroadcastChild* aActor);
    virtual bool DeallocPCellBroadcastChild(PCellBroadcastChild* aActor) override;

    virtual PSmsChild* AllocPSmsChild() override;
    virtual bool DeallocPSmsChild(PSmsChild*) override;

    virtual PTelephonyChild* AllocPTelephonyChild() override;
    virtual bool DeallocPTelephonyChild(PTelephonyChild*) override;

    virtual PVoicemailChild* AllocPVoicemailChild() override;
    PVoicemailChild* SendPVoicemailConstructor(PVoicemailChild* aActor);
    virtual bool DeallocPVoicemailChild(PVoicemailChild*) override;

    virtual PMediaChild* AllocPMediaChild() override;
    virtual bool DeallocPMediaChild(PMediaChild* aActor) override;

    virtual PStorageChild* AllocPStorageChild() override;
    virtual bool DeallocPStorageChild(PStorageChild* aActor) override;

    virtual PBluetoothChild* AllocPBluetoothChild() override;
    virtual bool DeallocPBluetoothChild(PBluetoothChild* aActor) override;

    virtual PFMRadioChild* AllocPFMRadioChild() override;
    virtual bool DeallocPFMRadioChild(PFMRadioChild* aActor) override;

    virtual PAsmJSCacheEntryChild* AllocPAsmJSCacheEntryChild(
                                 const asmjscache::OpenMode& aOpenMode,
                                 const asmjscache::WriteParams& aWriteParams,
                                 const IPC::Principal& aPrincipal) override;
    virtual bool DeallocPAsmJSCacheEntryChild(
                                    PAsmJSCacheEntryChild* aActor) override;

    virtual PSpeechSynthesisChild* AllocPSpeechSynthesisChild() override;
    virtual bool DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor) override;

    virtual bool RecvRegisterChrome(InfallibleTArray<ChromePackage>&& packages,
                                    InfallibleTArray<ResourceMapping>&& resources,
                                    InfallibleTArray<OverrideMapping>&& overrides,
                                    const nsCString& locale,
                                    const bool& reset) override;
    virtual bool RecvRegisterChromeItem(const ChromeRegistryItem& item) override;

    virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() override;
    virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) override;
    virtual PRemoteSpellcheckEngineChild* AllocPRemoteSpellcheckEngineChild() override;
    virtual bool DeallocPRemoteSpellcheckEngineChild(PRemoteSpellcheckEngineChild*) override;

    virtual bool RecvSetOffline(const bool& offline) override;
    virtual bool RecvSetConnectivity(const bool& connectivity) override;

    virtual bool RecvSpeakerManagerNotify() override;

    virtual bool RecvBidiKeyboardNotify(const bool& isLangRTL) override;

    virtual bool RecvUpdateServiceWorkerRegistrations() override;

    virtual bool RecvNotifyVisited(const URIParams& aURI) override;
    // auto remove when alertfinished is received.
    nsresult AddRemoteAlertObserver(const nsString& aData, nsIObserver* aObserver);

    virtual bool RecvSystemMemoryAvailable(const uint64_t& aGetterId,
                                           const uint32_t& aMemoryAvailable) override;

    virtual bool RecvPreferenceUpdate(const PrefSetting& aPref) override;

    virtual bool RecvNotifyAlertsObserver(const nsCString& aType,
                                          const nsString& aData) override;

    virtual bool RecvLoadProcessScript(const nsString& aURL) override;

    virtual bool RecvAsyncMessage(const nsString& aMsg,
                                  const ClonedMessageData& aData,
                                  InfallibleTArray<CpowEntry>&& aCpows,
                                  const IPC::Principal& aPrincipal) override;

    virtual bool RecvGeolocationUpdate(const GeoPosition& somewhere) override;

    virtual bool RecvGeolocationError(const uint16_t& errorCode) override;

    virtual bool RecvUpdateDictionaryList(InfallibleTArray<nsString>&& aDictionaries) override;

    virtual bool RecvAddPermission(const IPC::Permission& permission) override;

    virtual bool RecvScreenSizeChanged(const gfxIntSize &size) override;

    virtual bool RecvFlushMemory(const nsString& reason) override;

    virtual bool RecvActivateA11y() override;

    virtual bool RecvGarbageCollect() override;
    virtual bool RecvCycleCollect() override;

    virtual bool RecvAppInfo(const nsCString& version, const nsCString& buildID,
                             const nsCString& name, const nsCString& UAName,
                             const nsCString& ID, const nsCString& vendor) override;
    virtual bool RecvAppInit() override;

    virtual bool RecvLastPrivateDocShellDestroyed() override;

    virtual bool RecvVolumes(InfallibleTArray<VolumeInfo>&& aVolumes) override;
    virtual bool RecvFilePathUpdate(const nsString& aStorageType,
                                    const nsString& aStorageName,
                                    const nsString& aPath,
                                    const nsCString& aReason) override;
    virtual bool RecvFileSystemUpdate(const nsString& aFsName,
                                      const nsString& aVolumeName,
                                      const int32_t& aState,
                                      const int32_t& aMountGeneration,
                                      const bool& aIsMediaPresent,
                                      const bool& aIsSharing,
                                      const bool& aIsFormatting,
                                      const bool& aIsFake,
                                      const bool& aIsUnmounting,
                                      const bool& aIsRemovable,
                                      const bool& aIsHotSwappable) override;
    virtual bool RecvVolumeRemoved(const nsString& aFsName) override;

    virtual bool RecvNuwaFork() override;

    virtual bool
    RecvNotifyProcessPriorityChanged(const hal::ProcessPriority& aPriority) override;
    virtual bool RecvMinimizeMemoryUsage() override;

    virtual bool RecvLoadAndRegisterSheet(const URIParams& aURI,
                                          const uint32_t& aType) override;
    virtual bool RecvUnregisterSheet(const URIParams& aURI, const uint32_t& aType) override;

    virtual bool RecvNotifyPhoneStateChange(const nsString& state) override;

    void AddIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);
    void RemoveIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);
    virtual bool RecvNotifyIdleObserver(const uint64_t& aObserver,
                                        const nsCString& aTopic,
                                        const nsString& aData) override;

    virtual bool RecvOnAppThemeChanged() override;

    virtual bool RecvAssociatePluginId(const uint32_t& aPluginId,
                                       const base::ProcessId& aProcessId) override;
    virtual bool RecvLoadPluginResult(const uint32_t& aPluginId,
                                      const bool& aResult) override;
    virtual bool RecvUpdateWindow(const uintptr_t& aChildId) override;

    virtual bool RecvStartProfiler(const uint32_t& aEntries,
                                   const double& aInterval,
                                   nsTArray<nsCString>&& aFeatures,
                                   nsTArray<nsCString>&& aThreadNameFilters) override;
    virtual bool RecvStopProfiler() override;
    virtual bool RecvGatherProfile() override;
    virtual bool RecvDomainSetChanged(const uint32_t& aSetType, const uint32_t& aChangeType,
                                      const OptionalURIParams& aDomain) override;
    virtual bool RecvShutdown() override;

    virtual bool
    RecvInvokeDragSession(nsTArray<IPCDataTransfer>&& aTransfers,
                          const uint32_t& aAction) override;
    virtual bool RecvEndDragSession(const bool& aDoneDrag,
                                    const bool& aUserCancelled) override;
#ifdef ANDROID
    gfxIntSize GetScreenSize() { return mScreenSize; }
#endif

    // Get the directory for IndexedDB files. We query the parent for this and
    // cache the value
    nsString &GetIndexedDBPath();

    ContentParentId GetID() { return mID; }

    bool IsForApp() { return mIsForApp; }
    bool IsForBrowser() { return mIsForBrowser; }

    virtual PBlobChild*
    SendPBlobConstructor(PBlobChild* actor,
                         const BlobConstructorParams& params) override;

    virtual PFileDescriptorSetChild*
    AllocPFileDescriptorSetChild(const FileDescriptor&) override;

    virtual bool
    DeallocPFileDescriptorSetChild(PFileDescriptorSetChild*) override;

    virtual bool SendPBrowserConstructor(PBrowserChild* actor,
                                         const TabId& aTabId,
                                         const IPCTabContext& context,
                                         const uint32_t& chromeFlags,
                                         const ContentParentId& aCpID,
                                         const bool& aIsForApp,
                                         const bool& aIsForBrowser) override;

    virtual bool RecvPBrowserConstructor(PBrowserChild* aCctor,
                                         const TabId& aTabId,
                                         const IPCTabContext& aContext,
                                         const uint32_t& aChromeFlags,
                                         const ContentParentId& aCpID,
                                         const bool& aIsForApp,
                                         const bool& aIsForBrowser) override;

    void GetAvailableDictionaries(InfallibleTArray<nsString>& aDictionaries);

    PBrowserOrId
    GetBrowserOrId(TabChild* aTabChild);

    virtual POfflineCacheUpdateChild* AllocPOfflineCacheUpdateChild(
            const URIParams& manifestURI,
            const URIParams& documentURI,
            const bool& stickDocument,
            const TabId& aTabId) override;
    virtual bool
    DeallocPOfflineCacheUpdateChild(POfflineCacheUpdateChild* offlineCacheUpdate) override;

    virtual PWebrtcGlobalChild* AllocPWebrtcGlobalChild() override;
    virtual bool DeallocPWebrtcGlobalChild(PWebrtcGlobalChild *aActor) override;

    virtual PContentPermissionRequestChild*
    AllocPContentPermissionRequestChild(const InfallibleTArray<PermissionRequest>& aRequests,
                                        const IPC::Principal& aPrincipal,
                                        const TabId& aTabId) override;
    virtual bool
    DeallocPContentPermissionRequestChild(PContentPermissionRequestChild* actor) override;

    virtual bool RecvGamepadUpdate(const GamepadChangeEvent& aGamepadEvent) override;

private:
    virtual void ActorDestroy(ActorDestroyReason why) override;

    virtual void ProcessingError(Result aCode, const char* aReason) override;

    /**
     * Exit *now*.  Do not shut down XPCOM, do not pass Go, do not run
     * static destructors, do not collect $200.
     */
    MOZ_NORETURN void QuickExit();

    InfallibleTArray<nsAutoPtr<AlertObserver> > mAlertObservers;
    nsRefPtr<ConsoleListener> mConsoleListener;

    nsTHashtable<nsPtrHashKey<nsIObserver>> mIdleObservers;

    InfallibleTArray<nsString> mAvailableDictionaries;

    /**
     * An ID unique to the process containing our corresponding
     * content parent.
     *
     * We expect our content parent to set this ID immediately after opening a
     * channel to us.
     */
    ContentParentId mID;

    AppInfo mAppInfo;

#ifdef ANDROID
    gfxIntSize mScreenSize;
#endif

    bool mIsForApp;
    bool mIsForBrowser;
    bool mCanOverrideProcessName;
    bool mIsAlive;
    nsString mProcessName;

    static ContentChild* sSingleton;

    nsCOMPtr<nsIDomainPolicy> mPolicy;

    DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

uint64_t
NextWindowID();

} // namespace dom
} // namespace mozilla

#endif
