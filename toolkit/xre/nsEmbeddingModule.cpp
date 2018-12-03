/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsDialogParamBlock.h"
#include "nsWindowWatcher.h"
#include "nsFind.h"
#include "nsWebBrowserPersist.h"
#include "nsNetCID.h"
#include "nsEmbedCID.h"
#include "nsXREDirProvider.h"

#ifdef NS_PRINTING
#include "nsPrintingPromptService.h"
#include "nsPrintingProxy.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWindowWatcher, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFind)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserPersist)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsXREDirProvider,
                                         nsXREDirProvider::GetSingleton)

#ifdef MOZ_XUL
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDialogParamBlock)
#ifdef NS_PRINTING
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPrintingPromptService,
                                         nsPrintingPromptService::GetSingleton)
#ifdef PROXY_PRINTING
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPrintingProxy,
                                         nsPrintingProxy::GetInstance)
#endif
#endif
#endif

#ifdef MOZ_XUL
NS_DEFINE_NAMED_CID(NS_DIALOGPARAMBLOCK_CID);
#ifdef NS_PRINTING
NS_DEFINE_NAMED_CID(NS_PRINTINGPROMPTSERVICE_CID);
#endif
#endif
NS_DEFINE_NAMED_CID(NS_WINDOWWATCHER_CID);
NS_DEFINE_NAMED_CID(NS_FIND_CID);
NS_DEFINE_NAMED_CID(NS_WEBBROWSERPERSIST_CID);
NS_DEFINE_NAMED_CID(NS_XREDIRPROVIDER_CID);

static const mozilla::Module::CIDEntry kEmbeddingCIDs[] = {
#ifdef MOZ_XUL
    {&kNS_DIALOGPARAMBLOCK_CID, false, nullptr, nsDialogParamBlockConstructor},
#ifdef NS_PRINTING

#ifdef PROXY_PRINTING
    {&kNS_PRINTINGPROMPTSERVICE_CID, false, nullptr,
     nsPrintingPromptServiceConstructor, mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_PRINTINGPROMPTSERVICE_CID, false, nullptr, nsPrintingProxyConstructor,
     mozilla::Module::CONTENT_PROCESS_ONLY},
#else
    {&kNS_PRINTINGPROMPTSERVICE_CID, false, nullptr,
     nsPrintingPromptServiceConstructor},
#endif
#endif
#endif
    {&kNS_WINDOWWATCHER_CID, false, nullptr, nsWindowWatcherConstructor},
    {&kNS_FIND_CID, false, nullptr, nsFindConstructor},
    {&kNS_WEBBROWSERPERSIST_CID, false, nullptr,
     nsWebBrowserPersistConstructor},
    {&kNS_XREDIRPROVIDER_CID, false, nullptr, nsXREDirProviderConstructor},
    {nullptr}};

static const mozilla::Module::ContractIDEntry kEmbeddingContracts[] = {
#ifdef MOZ_XUL
    {NS_DIALOGPARAMBLOCK_CONTRACTID, &kNS_DIALOGPARAMBLOCK_CID},
#ifdef NS_PRINTING
    {NS_PRINTINGPROMPTSERVICE_CONTRACTID, &kNS_PRINTINGPROMPTSERVICE_CID},
#endif
#endif
    {NS_WINDOWWATCHER_CONTRACTID, &kNS_WINDOWWATCHER_CID},
    {NS_FIND_CONTRACTID, &kNS_FIND_CID},
    {NS_WEBBROWSERPERSIST_CONTRACTID, &kNS_WEBBROWSERPERSIST_CID},
    {NS_XREDIRPROVIDER_CONTRACTID, &kNS_XREDIRPROVIDER_CID},
    {nullptr}};

static const mozilla::Module kEmbeddingModule = {
    mozilla::Module::kVersion, kEmbeddingCIDs, kEmbeddingContracts};

NSMODULE_DEFN(embedcomponents) = &kEmbeddingModule;
