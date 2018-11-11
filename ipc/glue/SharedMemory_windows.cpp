/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>

#include "mozilla/ipc/SharedMemory.h"

namespace mozilla {
namespace ipc {

void
SharedMemory::SystemProtect(char* aAddr, size_t aSize, int aRights)
{
  DWORD flags;
  if ((aRights & RightsRead) && (aRights & RightsWrite))
    flags = PAGE_READWRITE;
  else if (aRights & RightsRead)
    flags = PAGE_READONLY;
  else
    flags = PAGE_NOACCESS;

  DWORD oldflags;
  if (!VirtualProtect(aAddr, aSize, flags, &oldflags))
    NS_RUNTIMEABORT("can't VirtualProtect()");
}

size_t
SharedMemory::SystemPageSize()
{
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
}

} // namespace ipc
} // namespace mozilla
