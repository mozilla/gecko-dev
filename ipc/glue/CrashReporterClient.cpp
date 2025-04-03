/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashReporterClient.h"
#include "nsISupportsImpl.h"

#if defined(XP_LINUX)
#  include "mozilla/toolkit/crashreporter/rust_minidump_writer_linux_ffi_generated.h"
#endif  // defined(XP_LINUX)

namespace mozilla::ipc {

StaticMutex CrashReporterClient::sLock;
StaticRefPtr<CrashReporterClient> CrashReporterClient::sClientSingleton;

CrashReporterClient::CrashReporterClient() {
  MOZ_COUNT_CTOR(CrashReporterClient);
}

CrashReporterClient::~CrashReporterClient() {
  MOZ_COUNT_DTOR(CrashReporterClient);
}

/* static */
void CrashReporterClient::InitSingleton() {
  {
    StaticMutexAutoLock lock(sLock);

    MOZ_ASSERT(!sClientSingleton);
    sClientSingleton = new CrashReporterClient();
  }
}

/*static*/
CrashReporter::CrashReporterInitArgs CrashReporterClient::CreateInitArgs() {
  CrashReporter::CrashReporterInitArgs initArgs;
  initArgs.threadId() = CrashReporter::CurrentThreadId();

#if defined(XP_LINUX)
  DirectAuxvDumpInfo auxvInfo = {};
  CrashReporter::GetCurrentProcessAuxvInfo(&auxvInfo);
  initArgs.auxvInfo().programHeaderCount() = auxvInfo.program_header_count;
  initArgs.auxvInfo().programHeaderAddress() = auxvInfo.program_header_address;
  initArgs.auxvInfo().linuxGateAddress() = auxvInfo.linux_gate_address;
  initArgs.auxvInfo().entryAddress() = auxvInfo.entry_address;
#endif  // defined(XP_LINUX)

  return initArgs;
}

/* static */
void CrashReporterClient::DestroySingleton() {
  StaticMutexAutoLock lock(sLock);
  sClientSingleton = nullptr;
}

/* static */
RefPtr<CrashReporterClient> CrashReporterClient::GetSingleton() {
  StaticMutexAutoLock lock(sLock);
  return sClientSingleton;
}

}  // namespace mozilla::ipc
