/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "PaymentActionResponse.h"
#include "PaymentRequestData.h"
#include "PaymentRequestService.h"

using mozilla::dom::BasicCardMethodChangeDetails;
using mozilla::dom::BasicCardResponseData;
using mozilla::dom::GeneralMethodChangeDetails;
using mozilla::dom::GeneralResponseData;
using mozilla::dom::PaymentAbortActionResponse;
using mozilla::dom::PaymentCanMakeActionResponse;
using mozilla::dom::PaymentCompleteActionResponse;
using mozilla::dom::PaymentRequestService;
using mozilla::dom::PaymentShowActionResponse;
using mozilla::dom::payments::PaymentAddress;

NS_GENERIC_FACTORY_CONSTRUCTOR(GeneralResponseData)
NS_GENERIC_FACTORY_CONSTRUCTOR(BasicCardResponseData)
NS_GENERIC_FACTORY_CONSTRUCTOR(PaymentCanMakeActionResponse)
NS_GENERIC_FACTORY_CONSTRUCTOR(PaymentAbortActionResponse)
NS_GENERIC_FACTORY_CONSTRUCTOR(PaymentShowActionResponse)
NS_GENERIC_FACTORY_CONSTRUCTOR(PaymentCompleteActionResponse)
NS_GENERIC_FACTORY_CONSTRUCTOR(GeneralMethodChangeDetails)
NS_GENERIC_FACTORY_CONSTRUCTOR(BasicCardMethodChangeDetails)
NS_GENERIC_FACTORY_CONSTRUCTOR(PaymentAddress)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(PaymentRequestService,
                                         PaymentRequestService::GetSingleton)

NS_DEFINE_NAMED_CID(NS_GENERAL_RESPONSE_DATA_CID);
NS_DEFINE_NAMED_CID(NS_BASICCARD_RESPONSE_DATA_CID);
NS_DEFINE_NAMED_CID(NS_PAYMENT_CANMAKE_ACTION_RESPONSE_CID);
NS_DEFINE_NAMED_CID(NS_PAYMENT_ABORT_ACTION_RESPONSE_CID);
NS_DEFINE_NAMED_CID(NS_PAYMENT_SHOW_ACTION_RESPONSE_CID);
NS_DEFINE_NAMED_CID(NS_PAYMENT_COMPLETE_ACTION_RESPONSE_CID);
NS_DEFINE_NAMED_CID(NS_GENERAL_CHANGE_DETAILS_CID);
NS_DEFINE_NAMED_CID(NS_BASICCARD_CHANGE_DETAILS_CID);
NS_DEFINE_NAMED_CID(NS_PAYMENT_ADDRESS_CID);
NS_DEFINE_NAMED_CID(NS_PAYMENT_REQUEST_SERVICE_CID);

static const mozilla::Module::CIDEntry kPaymentRequestCIDs[] = {
    {&kNS_GENERAL_RESPONSE_DATA_CID, false, nullptr,
     GeneralResponseDataConstructor},
    {&kNS_BASICCARD_RESPONSE_DATA_CID, false, nullptr,
     BasicCardResponseDataConstructor},
    {&kNS_PAYMENT_CANMAKE_ACTION_RESPONSE_CID, false, nullptr,
     PaymentCanMakeActionResponseConstructor},
    {&kNS_PAYMENT_ABORT_ACTION_RESPONSE_CID, false, nullptr,
     PaymentAbortActionResponseConstructor},
    {&kNS_PAYMENT_SHOW_ACTION_RESPONSE_CID, false, nullptr,
     PaymentShowActionResponseConstructor},
    {&kNS_PAYMENT_COMPLETE_ACTION_RESPONSE_CID, false, nullptr,
     PaymentCompleteActionResponseConstructor},
    {&kNS_GENERAL_CHANGE_DETAILS_CID, false, nullptr,
     GeneralMethodChangeDetailsConstructor},
    {&kNS_BASICCARD_CHANGE_DETAILS_CID, false, nullptr,
     BasicCardMethodChangeDetailsConstructor},
    {&kNS_PAYMENT_ADDRESS_CID, false, nullptr, PaymentAddressConstructor},
    {&kNS_PAYMENT_REQUEST_SERVICE_CID, true, nullptr,
     PaymentRequestServiceConstructor},
    {nullptr}};

static const mozilla::Module::ContractIDEntry kPaymentRequestContracts[] = {
    {NS_GENERAL_RESPONSE_DATA_CONTRACT_ID, &kNS_GENERAL_RESPONSE_DATA_CID},
    {NS_BASICCARD_RESPONSE_DATA_CONTRACT_ID, &kNS_BASICCARD_RESPONSE_DATA_CID},
    {NS_PAYMENT_CANMAKE_ACTION_RESPONSE_CONTRACT_ID,
     &kNS_PAYMENT_CANMAKE_ACTION_RESPONSE_CID},
    {NS_PAYMENT_ABORT_ACTION_RESPONSE_CONTRACT_ID,
     &kNS_PAYMENT_ABORT_ACTION_RESPONSE_CID},
    {NS_PAYMENT_SHOW_ACTION_RESPONSE_CONTRACT_ID,
     &kNS_PAYMENT_SHOW_ACTION_RESPONSE_CID},
    {NS_PAYMENT_COMPLETE_ACTION_RESPONSE_CONTRACT_ID,
     &kNS_PAYMENT_COMPLETE_ACTION_RESPONSE_CID},
    {NS_GENERAL_CHANGE_DETAILS_CONTRACT_ID, &kNS_GENERAL_CHANGE_DETAILS_CID},
    {NS_BASICCARD_CHANGE_DETAILS_CONTRACT_ID,
     &kNS_BASICCARD_CHANGE_DETAILS_CID},
    {NS_PAYMENT_ADDRESS_CONTRACT_ID, &kNS_PAYMENT_ADDRESS_CID},
    {NS_PAYMENT_REQUEST_SERVICE_CONTRACT_ID, &kNS_PAYMENT_REQUEST_SERVICE_CID},
    {nullptr}};

static const mozilla::Module::CategoryEntry kPaymentRequestCategories[] = {
    {"payment-request", "GeneralResponseData",
     NS_GENERAL_RESPONSE_DATA_CONTRACT_ID},
    {"payment-request", "BasicCardResponseData",
     NS_BASICCARD_RESPONSE_DATA_CONTRACT_ID},
    {"payment-request", "PaymentCanMakeActionResponse",
     NS_PAYMENT_CANMAKE_ACTION_RESPONSE_CONTRACT_ID},
    {"payment-request", "PaymentAbortActionResponse",
     NS_PAYMENT_ABORT_ACTION_RESPONSE_CONTRACT_ID},
    {"payment-request", "PaymentShowActionResponse",
     NS_PAYMENT_SHOW_ACTION_RESPONSE_CONTRACT_ID},
    {"payment-request", "PaymentCompleteActionResponse",
     NS_PAYMENT_COMPLETE_ACTION_RESPONSE_CONTRACT_ID},
    {"payment-request", "GeneralMethodChangeDetails",
     NS_GENERAL_CHANGE_DETAILS_CONTRACT_ID},
    {"payment-request", "BasicCardMethodChangeDetails",
     NS_BASICCARD_CHANGE_DETAILS_CONTRACT_ID},
    {"payment-request", "PaymentAddress", NS_PAYMENT_ADDRESS_CONTRACT_ID},
    {"payment-request", "PaymentRequestService",
     NS_PAYMENT_REQUEST_SERVICE_CONTRACT_ID},
    {nullptr}};

static const mozilla::Module kPaymentRequestModule = {
    mozilla::Module::kVersion, kPaymentRequestCIDs, kPaymentRequestContracts,
    kPaymentRequestCategories};

NSMODULE_DEFN(PaymentRequestModule) = &kPaymentRequestModule;
