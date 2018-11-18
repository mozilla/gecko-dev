/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "nsLayoutStatics.h"
#include "nscore.h"

#include "DateTimeFormat.h"
#include "nsAttrValue.h"
#include "nsColorNames.h"
#include "nsComputedDOMStyle.h"
#include "nsContentDLF.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "mozilla/css/ErrorReporter.h"
#include "nsCSSKeywords.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRendering.h"
#include "nsGenericHTMLFrameElement.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/EventListenerManager.h"
#include "nsFrame.h"
#include "nsGlobalWindow.h"
#include "nsGkAtoms.h"
#include "nsImageFrame.h"
#include "nsLayoutStylesheetCache.h"
#include "nsRange.h"
#include "nsRegion.h"
#include "nsRepeatService.h"
#include "nsFloatManager.h"
#include "nsSprocketLayout.h"
#include "nsStackLayout.h"
#include "nsTextControlFrame.h"
#include "nsXBLService.h"
#include "txMozillaXSLTProcessor.h"
#include "nsTreeSanitizer.h"
#include "nsCellMap.h"
#include "nsTextFrame.h"
#include "nsCCUncollectableMarker.h"
#include "nsTextFragment.h"
#include "nsMediaFeatures.h"
#include "nsCORSListenerProxy.h"
#include "nsHTMLDNSPrefetch.h"
#include "nsHtml5Module.h"
#include "nsHTMLTags.h"
#include "mozilla/dom/FallbackEncoding.h"
#include "nsFocusManager.h"
#include "nsListControlFrame.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "SVGElementFactory.h"
#include "nsSVGUtils.h"
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#include "Navigator.h"
#include "StorageObserver.h"
#include "CacheObserver.h"
#include "DisplayItemClip.h"
#include "ActiveLayerTracker.h"
#include "FrameLayerBuilder.h"
#include "AnimationCommon.h"
#include "LayerAnimationInfo.h"

#include "AudioChannelService.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/WebCryptoThreadPool.h"

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#include "nsXULContentUtils.h"
#include "nsXULPrototypeCache.h"
#include "nsXULTooltipListener.h"

#include "nsMenuBarListener.h"
#endif

#include "CubebUtils.h"
#include "WebAudioUtils.h"

#include "nsError.h"

#include "nsJSEnvironment.h"
#include "nsContentSink.h"
#include "nsFrameMessageManager.h"
#include "nsDOMMutationObserver.h"
#include "nsHyphenationManager.h"
#include "nsWindowMemoryReporter.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ProcessPriorityManager.h"
#include "nsPermissionManager.h"
#include "nsCookieService.h"
#include "nsApplicationCacheService.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "TouchManager.h"
#include "DecoderDoctorLogger.h"
#include "MediaDecoder.h"
#include "mozilla/ClearSiteData.h"
#include "mozilla/Fuzzyfox.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StaticPresData.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"
#include "mozilla/dom/ipc/IPCBlobInputStreamStorage.h"
#include "mozilla/dom/U2FTokenManager.h"
#include "mozilla/dom/PointerEventHandler.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "nsThreadManager.h"
#include "mozilla/css/ImageLoader.h"

using namespace mozilla;
using namespace mozilla::net;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

nsrefcnt nsLayoutStatics::sLayoutStaticRefcnt = 0;

nsresult
nsLayoutStatics::Initialize()
{
  NS_ASSERTION(sLayoutStaticRefcnt == 0,
               "nsLayoutStatics isn't zero!");

  sLayoutStaticRefcnt = 1;
  NS_LOG_ADDREF(&sLayoutStaticRefcnt, sLayoutStaticRefcnt,
                "nsLayoutStatics", 1);

  nsresult rv;

  ContentParent::StartUp();

  nsCSSKeywords::AddRefTable();
  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();

#ifdef DEBUG
  nsCSSPseudoElements::AssertAtoms();
  nsCSSAnonBoxes::AssertAtoms();
#endif

  StartupJSEnvironment();
  nsJSContext::EnsureStatics();

  nsGlobalWindowInner::Init();
  nsGlobalWindowOuter::Init();
  Navigator::Init();
  nsXBLService::Init();

  rv = nsContentUtils::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsContentUtils");
    return rv;
  }

  rv = nsAttrValue::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsAttrValue");
    return rv;
  }

  rv = nsTextFragment::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsTextFragment");
    return rv;
  }

  nsCellMap::Init();

  mozilla::SharedFontList::Initialize();
  StaticPresData::Init();
  nsCSSRendering::Init();
  css::ImageLoader::Init();

  rv = nsHTMLDNSPrefetch::Initialize();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize HTML DNS prefetch");
    return rv;
  }

  nsMathMLOperators::AddRefTable();

#ifdef DEBUG
  nsFrame::DisplayReflowStartup();
#endif
  Attr::Initialize();

  rv = txMozillaXSLTProcessor::Startup();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize txMozillaXSLTProcessor");
    return rv;
  }

  rv = StorageObserver::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize StorageObserver");
    return rv;
  }

  rv = nsCCUncollectableMarker::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsCCUncollectableMarker");
    return rv;
  }

#ifdef MOZ_XUL
  rv = nsXULPopupManager::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsXULPopupManager");
    return rv;
  }
#endif

  rv = nsFocusManager::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsFocusManager");
    return rv;
  }

  DecoderDoctorLogger::Init();
  MediaManager::StartupInit();
  CubebUtils::InitLibrary();

  nsContentSink::InitializeStatics();
  nsHtml5Module::InitializeStatics();
  mozilla::dom::FallbackEncoding::Initialize();
  nsLayoutUtils::Initialize();
  PointerEventHandler::InitializeStatics();
  TouchManager::InitializeStatics();

  nsCORSListenerProxy::Startup();

  nsWindowMemoryReporter::Init();

  SVGElementFactory::Init();
  nsSVGUtils::Init();

  ProcessPriorityManager::Init();

  nsPermissionManager::ClearOriginDataObserverInit();
  nsCookieService::AppClearDataObserverInit();
  nsApplicationCacheService::AppClearDataObserverInit();

  HTMLVideoElement::Init();
  nsGenericHTMLFrameElement::InitStatics();

#ifdef MOZ_XUL
  nsMenuBarListener::InitializeStatics();
#endif

  CacheObserver::Init();

  IMEStateManager::Init();

  ServiceWorkerRegistrar::Initialize();

  MediaDecoder::InitStatics();

  PromiseDebugging::Init();

  mozilla::dom::WebCryptoThreadPool::Initialize();

  if (XRE_IsParentProcess() || XRE_IsContentProcess()) {
    InitializeServo();
  }

  // This must be initialized on the main-thread.
  mozilla::dom::IPCBlobInputStreamStorage::Initialize();

  mozilla::dom::U2FTokenManager::Initialize();

  if (XRE_IsParentProcess()) {
    // On content process we initialize DOMPrefs when PContentChild is fully
    // initialized.
    mozilla::dom::DOMPrefs::Initialize();
  }

  nsThreadManager::InitializeShutdownObserver();

  mozilla::Fuzzyfox::Start();

  ClearSiteData::Initialize();

  return NS_OK;
}

void
nsLayoutStatics::Shutdown()
{
  // Don't need to shutdown nsWindowMemoryReporter, that will be done by the
  // memory reporter manager.

  if (XRE_IsParentProcess() || XRE_IsContentProcess()) {
    ShutdownServo();
    URLExtraData::ReleaseDummy();
  }

  nsMessageManagerScriptExecutor::Shutdown();
  nsFocusManager::Shutdown();
#ifdef MOZ_XUL
  nsXULPopupManager::Shutdown();
#endif
  StorageObserver::Shutdown();
  txMozillaXSLTProcessor::Shutdown();
  Attr::Shutdown();
  EventListenerManager::Shutdown();
  IMEStateManager::Shutdown();
  nsMediaFeatures::Shutdown();
  nsHTMLDNSPrefetch::Shutdown();
  nsCSSRendering::Shutdown();
  StaticPresData::Shutdown();
#ifdef DEBUG
  nsFrame::DisplayReflowShutdown();
#endif
  nsCellMap::Shutdown();
  ActiveLayerTracker::Shutdown();

  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsRepeatService::Shutdown();
  nsStackLayout::Shutdown();
  nsBox::Shutdown();

#ifdef MOZ_XUL
  nsXULContentUtils::Finish();
  nsXULPrototypeCache::ReleaseGlobals();
  nsSprocketLayout::Shutdown();
#endif

  SVGElementFactory::Shutdown();
  nsMathMLOperators::ReleaseTable();

  nsFloatManager::Shutdown();
  nsImageFrame::ReleaseGlobals();

  mozilla::css::ErrorReporter::ReleaseGlobals();

  nsTextFragment::Shutdown();

  nsAttrValue::Shutdown();
  nsContentUtils::Shutdown();
  nsLayoutStylesheetCache::Shutdown();

  ShutdownJSEnvironment();
  nsGlobalWindowInner::ShutDown();
  nsGlobalWindowOuter::ShutDown();
  nsListControlFrame::Shutdown();
  nsXBLService::Shutdown();
  FrameLayerBuilder::Shutdown();

  CubebUtils::ShutdownLibrary();
  WebAudioUtils::Shutdown();

  nsCORSListenerProxy::Shutdown();

  PointerEventHandler::ReleaseStatics();

  TouchManager::ReleaseStatics();

  nsTreeSanitizer::ReleaseStatics();

  nsHtml5Module::ReleaseStatics();

  mozilla::dom::FallbackEncoding::Shutdown();

  mozilla::EventDispatcher::Shutdown();

  HTMLInputElement::DestroyUploadLastDir();

  nsLayoutUtils::Shutdown();
  mozilla::SharedFontList::Shutdown();

  nsHyphenationManager::Shutdown();
  nsDOMMutationObserver::Shutdown();

  DateTimeFormat::Shutdown();

  ContentParent::ShutDown();

  DisplayItemClip::Shutdown();

  CacheObserver::Shutdown();

  PromiseDebugging::Shutdown();

  BlobURLProtocolHandler::RemoveDataEntries();

  css::ImageLoader::Shutdown();
}
