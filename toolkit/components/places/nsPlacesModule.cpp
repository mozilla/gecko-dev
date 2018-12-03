/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "nsAnnoProtocolHandler.h"
#include "nsAnnotationService.h"
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsFaviconService.h"
#include "History.h"
#include "nsDocShellCID.h"

using namespace mozilla::places;

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsNavHistory,
                                         nsNavHistory::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsAnnotationService,
                                         nsAnnotationService::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsNavBookmarks,
                                         nsNavBookmarks::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsFaviconService,
                                         nsFaviconService::GetSingleton)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(History, History::GetSingleton)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAnnoProtocolHandler)
NS_DEFINE_NAMED_CID(NS_NAVHISTORYSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ANNOTATIONSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ANNOPROTOCOLHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_NAVBOOKMARKSSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_FAVICONSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_HISTORYSERVICE_CID);

const mozilla::Module::CIDEntry kPlacesCIDs[] = {
    {&kNS_NAVHISTORYSERVICE_CID, false, nullptr, nsNavHistoryConstructor},
    {&kNS_ANNOTATIONSERVICE_CID, false, nullptr,
     nsAnnotationServiceConstructor},
    {&kNS_ANNOPROTOCOLHANDLER_CID, false, nullptr,
     nsAnnoProtocolHandlerConstructor},
    {&kNS_NAVBOOKMARKSSERVICE_CID, false, nullptr, nsNavBookmarksConstructor},
    {&kNS_FAVICONSERVICE_CID, false, nullptr, nsFaviconServiceConstructor},
    {&kNS_HISTORYSERVICE_CID, false, nullptr, HistoryConstructor},
    {nullptr}};

const mozilla::Module::ContractIDEntry kPlacesContracts[] = {
    {NS_NAVHISTORYSERVICE_CONTRACTID, &kNS_NAVHISTORYSERVICE_CID},
    {NS_ANNOTATIONSERVICE_CONTRACTID, &kNS_ANNOTATIONSERVICE_CID},
    {NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-anno",
     &kNS_ANNOPROTOCOLHANDLER_CID},
    {NS_NAVBOOKMARKSSERVICE_CONTRACTID, &kNS_NAVBOOKMARKSSERVICE_CID},
    {NS_FAVICONSERVICE_CONTRACTID, &kNS_FAVICONSERVICE_CID},
    {NS_IHISTORY_CONTRACTID, &kNS_HISTORYSERVICE_CID},
    {nullptr}};

const mozilla::Module::CategoryEntry kPlacesCategories[] = {
    {"vacuum-participant", "Places", NS_NAVHISTORYSERVICE_CONTRACTID},
    {nullptr}};

const mozilla::Module kPlacesModule = {mozilla::Module::kVersion, kPlacesCIDs,
                                       kPlacesContracts, kPlacesCategories};

NSMODULE_DEFN(nsPlacesModule) = &kPlacesModule;
