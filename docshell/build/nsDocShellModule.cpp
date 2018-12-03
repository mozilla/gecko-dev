/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsDocShellCID.h"

#include "mozilla/dom/BrowsingContext.h"

#include "nsDocShell.h"
#include "nsDefaultURIFixup.h"
#include "nsWebNavigationInfo.h"
#include "nsAboutRedirector.h"
#include "nsCDefaultURIFixup.h"

// uriloader
#include "nsURILoader.h"
#include "nsDocLoader.h"
#include "nsOSHelperAppService.h"
#include "nsOSPermissionRequest.h"
#include "nsExternalProtocolHandler.h"
#include "nsPrefetchService.h"
#include "nsOfflineCacheUpdate.h"
#include "nsLocalHandlerApp.h"
#include "ContentHandlerService.h"
#ifdef MOZ_ENABLE_DBUS
#include "nsDBusHandlerApp.h"
#endif
#if defined(MOZ_WIDGET_ANDROID)
#include "nsExternalURLHandlerService.h"
#endif

// session history
#include "nsSHEntry.h"
#include "nsSHEntryShared.h"
#include "nsSHistory.h"

using mozilla::dom::ContentHandlerService;

static bool gInitialized = false;

// The one time initialization for this module
static nsresult Initialize() {
  MOZ_ASSERT(!gInitialized, "docshell module already initialized");
  if (gInitialized) {
    return NS_OK;
  }
  gInitialized = true;

  mozilla::dom::BrowsingContext::Init();
  nsresult rv = nsSHistory::Startup();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static void Shutdown() {
  nsSHistory::Shutdown();
  nsSHEntryShared::Shutdown();
  gInitialized = false;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDefaultURIFixup)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWebNavigationInfo, Init)

// uriloader
NS_GENERIC_FACTORY_CONSTRUCTOR(nsURILoader)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDocLoader, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsOSHelperAppService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExternalProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrefetchService, Init)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(
    nsOfflineCacheUpdateService, nsOfflineCacheUpdateService::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(PlatformLocalHandlerApp_t)
#ifdef MOZ_ENABLE_DBUS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDBusHandlerApp)
#endif
#if defined(MOZ_WIDGET_ANDROID)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExternalURLHandlerService)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(ContentHandlerService, Init)

// OS access permissions
NS_GENERIC_FACTORY_CONSTRUCTOR(nsOSPermissionRequest)

// session history
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHEntry)

NS_DEFINE_NAMED_CID(NS_DEFAULTURIFIXUP_CID);
NS_DEFINE_NAMED_CID(NS_WEBNAVIGATION_INFO_CID);
NS_DEFINE_NAMED_CID(NS_ABOUT_REDIRECTOR_MODULE_CID);
NS_DEFINE_NAMED_CID(NS_URI_LOADER_CID);
NS_DEFINE_NAMED_CID(NS_DOCUMENTLOADER_SERVICE_CID);
NS_DEFINE_NAMED_CID(NS_EXTERNALHELPERAPPSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_EXTERNALPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_PREFETCHSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_OFFLINECACHEUPDATESERVICE_CID);
NS_DEFINE_NAMED_CID(NS_LOCALHANDLERAPP_CID);
NS_DEFINE_NAMED_CID(NS_OSPERMISSIONREQUEST_CID);
#ifdef MOZ_ENABLE_DBUS
NS_DEFINE_NAMED_CID(NS_DBUSHANDLERAPP_CID);
#endif
#if defined(MOZ_WIDGET_ANDROID)
NS_DEFINE_NAMED_CID(NS_EXTERNALURLHANDLERSERVICE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_SHENTRY_CID);
NS_DEFINE_NAMED_CID(NS_CONTENTHANDLERSERVICE_CID);

const mozilla::Module::CIDEntry kDocShellCIDs[] = {
    // clang-format off
  { &kNS_DEFAULTURIFIXUP_CID, false, nullptr, nsDefaultURIFixupConstructor },
  { &kNS_WEBNAVIGATION_INFO_CID, false, nullptr, nsWebNavigationInfoConstructor },
  { &kNS_ABOUT_REDIRECTOR_MODULE_CID, false, nullptr, nsAboutRedirector::Create },
  { &kNS_URI_LOADER_CID, false, nullptr, nsURILoaderConstructor },
  { &kNS_DOCUMENTLOADER_SERVICE_CID, false, nullptr, nsDocLoaderConstructor },
  { &kNS_EXTERNALHELPERAPPSERVICE_CID, false, nullptr, nsOSHelperAppServiceConstructor },
  { &kNS_OSPERMISSIONREQUEST_CID, false, nullptr, nsOSPermissionRequestConstructor },
  { &kNS_CONTENTHANDLERSERVICE_CID, false, nullptr, ContentHandlerServiceConstructor,
    mozilla::Module::CONTENT_PROCESS_ONLY },
  { &kNS_EXTERNALPROTOCOLHANDLER_CID, false, nullptr, nsExternalProtocolHandlerConstructor },
  { &kNS_PREFETCHSERVICE_CID, false, nullptr, nsPrefetchServiceConstructor },
  { &kNS_OFFLINECACHEUPDATESERVICE_CID, false, nullptr, nsOfflineCacheUpdateServiceConstructor },
  { &kNS_LOCALHANDLERAPP_CID, false, nullptr, PlatformLocalHandlerApp_tConstructor },
#ifdef MOZ_ENABLE_DBUS
  { &kNS_DBUSHANDLERAPP_CID, false, nullptr, nsDBusHandlerAppConstructor },
#endif
#if defined(MOZ_WIDGET_ANDROID)
  { &kNS_EXTERNALURLHANDLERSERVICE_CID, false, nullptr, nsExternalURLHandlerServiceConstructor },
#endif
  { &kNS_SHENTRY_CID, false, nullptr, nsSHEntryConstructor },
  { nullptr }
    // clang-format on
};

const mozilla::Module::ContractIDEntry kDocShellContracts[] = {
    // clang-format off
  { NS_URIFIXUP_CONTRACTID, &kNS_DEFAULTURIFIXUP_CID },
  { NS_WEBNAVIGATION_INFO_CONTRACTID, &kNS_WEBNAVIGATION_INFO_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "about", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "addons", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "buildconfig", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "checkerboard", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "config", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
#ifdef MOZ_CRASHREPORTER
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "crashes", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
#endif
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "crashparent", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "crashcontent", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "credits", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "license", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "logo", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "memory", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "mozilla", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "neterror", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "networking", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "performance", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "plugins", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "serviceworkers", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
#ifndef ANDROID
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "profiles", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
#endif
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "srcdoc", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "support", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "telemetry", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "webrtc", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "printpreview", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_ABOUT_MODULE_CONTRACTID_PREFIX "url-classifier", &kNS_ABOUT_REDIRECTOR_MODULE_CID },
  { NS_URI_LOADER_CONTRACTID, &kNS_URI_LOADER_CID },
  { NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &kNS_DOCUMENTLOADER_SERVICE_CID },
  { NS_HANDLERSERVICE_CONTRACTID, &kNS_CONTENTHANDLERSERVICE_CID, mozilla::Module::CONTENT_PROCESS_ONLY },
  { NS_EXTERNALHELPERAPPSERVICE_CONTRACTID, &kNS_EXTERNALHELPERAPPSERVICE_CID },
  { NS_EXTERNALPROTOCOLSERVICE_CONTRACTID, &kNS_EXTERNALHELPERAPPSERVICE_CID },
  { NS_MIMESERVICE_CONTRACTID, &kNS_EXTERNALHELPERAPPSERVICE_CID },
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX"default", &kNS_EXTERNALPROTOCOLHANDLER_CID },
  { NS_PREFETCHSERVICE_CONTRACTID, &kNS_PREFETCHSERVICE_CID },
  { NS_OFFLINECACHEUPDATESERVICE_CONTRACTID, &kNS_OFFLINECACHEUPDATESERVICE_CID },
  { NS_LOCALHANDLERAPP_CONTRACTID, &kNS_LOCALHANDLERAPP_CID },
#ifdef MOZ_ENABLE_DBUS
  { NS_DBUSHANDLERAPP_CONTRACTID, &kNS_DBUSHANDLERAPP_CID },
#endif
#if defined(MOZ_WIDGET_ANDROID)
  { NS_EXTERNALURLHANDLERSERVICE_CONTRACTID, &kNS_EXTERNALURLHANDLERSERVICE_CID },
#endif
  { NS_SHENTRY_CONTRACTID, &kNS_SHENTRY_CID },
  { NS_OSPERMISSIONREQUEST_CONTRACTID, &kNS_OSPERMISSIONREQUEST_CID, mozilla::Module::MAIN_PROCESS_ONLY },
  { nullptr }
    // clang-format on
};

static const mozilla::Module kDocShellModule = {mozilla::Module::kVersion,
                                                kDocShellCIDs,
                                                kDocShellContracts,
                                                nullptr,
                                                nullptr,
                                                Initialize,
                                                Shutdown};

NSMODULE_DEFN(docshell_provider) = &kDocShellModule;
