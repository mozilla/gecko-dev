/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVirtualFileSystemRequestManager.h"
#include "nsVirtualFileSystemData.h"
#include "mozilla/dom/FileSystemProviderAbortEvent.h"
#include "mozilla/dom/FileSystemProviderCloseFileEvent.h"
#include "mozilla/dom/FileSystemProviderGetMetadataEvent.h"
#include "mozilla/dom/FileSystemProviderOpenFileEvent.h"
#include "mozilla/dom/FileSystemProviderReadDirectoryEvent.h"
#include "mozilla/dom/FileSystemProviderReadFileEvent.h"
#include "mozilla/ModuleUtils.h"

using mozilla::dom::virtualfilesystem::nsEntryMetadata;
using mozilla::dom::AbortRequestedOptions;
using mozilla::dom::CloseFileRequestedOptions;
using mozilla::dom::OpenFileRequestedOptions;
using mozilla::dom::GetMetadataRequestedOptions;
using mozilla::dom::ReadDirectoryRequestedOptions;
using mozilla::dom::ReadFileRequestedOptions;

using mozilla::dom::virtualfilesystem::nsVirtualFileSystemRequestManager;

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEntryMetadata)
NS_DEFINE_NAMED_CID(ENTRYMETADATA_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(AbortRequestedOptions)
NS_DEFINE_NAMED_CID(VIRTUALFILESYSTEMABORTREQUESTOPTION_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(CloseFileRequestedOptions)
NS_DEFINE_NAMED_CID(VIRTUALFILESYSTEMCLOSEFILEREQUESTOPTION_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(OpenFileRequestedOptions)
NS_DEFINE_NAMED_CID(VIRTUALFILESYSTEMOPENFILEREQUESTOPTION_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(GetMetadataRequestedOptions)
NS_DEFINE_NAMED_CID(VIRTUALFILESYSTEMGETMETADATAREQUESTOPTION_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(ReadDirectoryRequestedOptions)
NS_DEFINE_NAMED_CID(VIRTUALFILESYSTEMREADDIRECTORYREQUESTOPTION_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(ReadFileRequestedOptions)
NS_DEFINE_NAMED_CID(VIRTUALFILESYSTEMREADFILEREQUESTOPTION_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsVirtualFileSystemRequestManager)
NS_DEFINE_NAMED_CID(VIRTUALFILESYSTEMREQUESTMANAGER_CID);

static const mozilla::Module::CIDEntry kVirtualFileSystemModuleCIDs[] = {
  { &kENTRYMETADATA_CID, false, nullptr, nsEntryMetadataConstructor },
  { &kVIRTUALFILESYSTEMABORTREQUESTOPTION_CID, false, nullptr, AbortRequestedOptionsConstructor },
  { &kVIRTUALFILESYSTEMCLOSEFILEREQUESTOPTION_CID, false, nullptr, CloseFileRequestedOptionsConstructor },
  { &kVIRTUALFILESYSTEMOPENFILEREQUESTOPTION_CID, false, nullptr, OpenFileRequestedOptionsConstructor },
  { &kVIRTUALFILESYSTEMGETMETADATAREQUESTOPTION_CID, false, nullptr, GetMetadataRequestedOptionsConstructor },
  { &kVIRTUALFILESYSTEMREADDIRECTORYREQUESTOPTION_CID, false, nullptr, ReadDirectoryRequestedOptionsConstructor },
  { &kVIRTUALFILESYSTEMREADFILEREQUESTOPTION_CID, false, nullptr, ReadFileRequestedOptionsConstructor },
  { &kVIRTUALFILESYSTEMREQUESTMANAGER_CID, false, nullptr, nsVirtualFileSystemRequestManagerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kVirtualFileSystemModuleContracts[] = {
  { ENTRY_METADATA_CONTRACT_ID, &kENTRYMETADATA_CID },
  { VIRTUAL_FILE_SYSTEM_ABORT_REQUEST_OPTION_CONTRACT_ID, &kVIRTUALFILESYSTEMABORTREQUESTOPTION_CID },
  { VIRTUAL_FILE_SYSTEM_CLOSEFILE_REQUEST_OPTION_CONTRACT_ID, &kVIRTUALFILESYSTEMCLOSEFILEREQUESTOPTION_CID },
  { VIRTUAL_FILE_SYSTEM_OPENFILE_REQUEST_OPTION_CONTRACT_ID, &kVIRTUALFILESYSTEMOPENFILEREQUESTOPTION_CID },
  { VIRTUAL_FILE_SYSTEM_GETMETADATA_REQUEST_OPTION_CONTRACT_ID, &kVIRTUALFILESYSTEMGETMETADATAREQUESTOPTION_CID },
  { VIRTUAL_FILE_SYSTEM_READDIRECTORY_REQUEST_OPTION_CONTRACT_ID, &kVIRTUALFILESYSTEMREADDIRECTORYREQUESTOPTION_CID },
  { VIRTUAL_FILE_SYSTEM_READFILE_REQUEST_OPTION_CONTRACT_ID, &kVIRTUALFILESYSTEMREADFILEREQUESTOPTION_CID },
  { VIRTUAL_FILE_SYSTEM_REQUEST_MANAGER_CONTRACT_ID, &kVIRTUALFILESYSTEMREQUESTMANAGER_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kVirtualFileSystemModuleCategories[] = {
  { nullptr }
};

static const mozilla::Module kVirtualFileSystemModule = {
  mozilla::Module::kVersion,
  kVirtualFileSystemModuleCIDs,
  kVirtualFileSystemModuleContracts,
  kVirtualFileSystemModuleCategories
};

NSMODULE_DEFN(VirtualFileSystemModule) = &kVirtualFileSystemModule;
