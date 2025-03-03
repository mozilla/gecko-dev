/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This source code was derived from Chromium code, and as such is also subject
 * to the [Chromium license](ipc/chromium/src/LICENSE). */

#include "mozilla/ipc/SharedMemory.h"

#include <windows.h>

#include "base/logging.h"
#include "base/win_util.h"
#include "base/string_util.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/RandomNum.h"
#include "nsDebug.h"
#include "nsString.h"
#ifdef MOZ_MEMORY
#  include "mozmemory_utils.h"
#endif

namespace {
// NtQuerySection is an internal (but believed to be stable) API and the
// structures it uses are defined in nt_internals.h.
// So we have to define them ourselves.
typedef enum _SECTION_INFORMATION_CLASS {
  SectionBasicInformation,
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION {
  PVOID BaseAddress;
  ULONG Attributes;
  LARGE_INTEGER Size;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

typedef ULONG(__stdcall* NtQuerySectionType)(
    HANDLE SectionHandle, SECTION_INFORMATION_CLASS SectionInformationClass,
    PVOID SectionInformation, ULONG SectionInformationLength,
    PULONG ResultLength);

// Checks if the section object is safe to map. At the moment this just means
// it's not an image section.
bool IsSectionSafeToMap(HANDLE handle) {
  static NtQuerySectionType nt_query_section_func =
      reinterpret_cast<NtQuerySectionType>(
          ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "NtQuerySection"));
  DCHECK(nt_query_section_func);

  // The handle must have SECTION_QUERY access for this to succeed.
  SECTION_BASIC_INFORMATION basic_information = {};
  ULONG status =
      nt_query_section_func(handle, SectionBasicInformation, &basic_information,
                            sizeof(basic_information), nullptr);
  if (status) {
    return false;
  }

  return (basic_information.Attributes & SEC_IMAGE) != SEC_IMAGE;
}

}  // namespace

namespace mozilla::ipc {

void SharedMemory::ResetImpl() {};

SharedMemory::Handle SharedMemory::CloneHandle(const Handle& aHandle) {
  HANDLE handle = INVALID_HANDLE_VALUE;
  if (DuplicateHandle(GetCurrentProcess(), aHandle.get(), GetCurrentProcess(),
                      &handle, 0, false, DUPLICATE_SAME_ACCESS)) {
    return SharedMemoryHandle(handle);
  }
  NS_WARNING("DuplicateHandle Failed!");
  return nullptr;
}

void* SharedMemory::FindFreeAddressSpace(size_t size) {
  void* memory = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
  if (memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
  return memory;
}

Maybe<void*> SharedMemory::MapImpl(size_t nBytes, void* fixedAddress) {
  if (mExternalHandle && !IsSectionSafeToMap(mHandle.get())) {
    return Nothing();
  }

  void* mem = MapViewOfFileEx(
      mHandle.get(), mReadOnly ? FILE_MAP_READ : FILE_MAP_READ | FILE_MAP_WRITE,
      0, 0, nBytes, fixedAddress);
  if (mem) {
    MOZ_ASSERT(!fixedAddress || mem == fixedAddress,
               "MapViewOfFileEx returned an expected address");
    return Some(mem);
  }
  return Nothing();
}

void SharedMemory::UnmapImpl(size_t nBytes, void* address) {
  UnmapViewOfFile(address);
}

// Wrapper around CreateFileMappingW for pagefile-backed regions. When out of
// memory, may attempt to stall and retry rather than returning immediately, in
// hopes that the page file is about to be expanded by Windows. (bug 1822383,
// bug 1716727)
//
// This method is largely a copy of the MozVirtualAlloc method from
// mozjemalloc.cpp, which implements this strategy for VirtualAlloc calls,
// except re-purposed to handle CreateFileMapping.
static HANDLE MozCreateFileMappingW(
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect,
    DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName) {
#ifdef MOZ_MEMORY
  constexpr auto IsOOMError = [] {
    return ::GetLastError() == ERROR_COMMITMENT_LIMIT;
  };

  {
    HANDLE handle = ::CreateFileMappingW(
        INVALID_HANDLE_VALUE, lpFileMappingAttributes, flProtect,
        dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
    if (MOZ_LIKELY(handle)) {
      MOZ_DIAGNOSTIC_ASSERT(handle != INVALID_HANDLE_VALUE,
                            "::CreateFileMapping should return NULL, not "
                            "INVALID_HANDLE_VALUE, on failure");
      return handle;
    }

    // We can't do anything for errors other than OOM.
    if (!IsOOMError()) {
      return nullptr;
    }
  }

  // Retry as many times as desired (possibly zero).
  const mozilla::StallSpecs stallSpecs = mozilla::GetAllocatorStallSpecs();

  const auto ret =
      stallSpecs.StallAndRetry(&::Sleep, [&]() -> std::optional<HANDLE> {
        HANDLE handle = ::CreateFileMappingW(
            INVALID_HANDLE_VALUE, lpFileMappingAttributes, flProtect,
            dwMaximumSizeHigh, dwMaximumSizeLow, lpName);

        if (handle) {
          MOZ_DIAGNOSTIC_ASSERT(handle != INVALID_HANDLE_VALUE,
                                "::CreateFileMapping should return NULL, not "
                                "INVALID_HANDLE_VALUE, on failure");
          return handle;
        }

        // Failure for some reason other than OOM.
        if (!IsOOMError()) {
          return nullptr;
        }

        return std::nullopt;
      });

  return ret.value_or(nullptr);
#else
  return ::CreateFileMappingW(INVALID_HANDLE_VALUE, lpFileMappingAttributes,
                              flProtect, dwMaximumSizeHigh, dwMaximumSizeLow,
                              lpName);
#endif
}

bool SharedMemory::CreateImpl(size_t size, bool freezable) {
  // If the shared memory object has no DACL, any process can
  // duplicate its handles with any access rights; e.g., re-add write
  // access to a read-only handle.  To prevent that, we give it an
  // empty DACL, so that no process can do that.
  SECURITY_ATTRIBUTES sa, *psa = nullptr;
  SECURITY_DESCRIPTOR sd;
  ACL dacl;

  if (freezable) {
    psa = &sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    if (NS_WARN_IF(!InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) ||
        NS_WARN_IF(
            !InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) ||
        NS_WARN_IF(!SetSecurityDescriptorDacl(&sd, TRUE, &dacl, FALSE))) {
      return false;
    }
  }

  mHandle.reset(MozCreateFileMappingW(psa, PAGE_READWRITE, 0,
                                      static_cast<DWORD>(size), nullptr));
  return (bool)mHandle;
}

Maybe<SharedMemory::Handle> SharedMemory::ReadOnlyCopyImpl() {
  HANDLE ro_handle;
  if (!::DuplicateHandle(GetCurrentProcess(), mHandle.get(),
                         GetCurrentProcess(), &ro_handle,
                         GENERIC_READ | FILE_MAP_READ, false, 0)) {
    return Nothing();
  }

  return Some(ro_handle);
}

void SharedMemory::SystemProtect(char* aAddr, size_t aSize, int aRights) {
  if (!SystemProtectFallible(aAddr, aSize, aRights)) {
    MOZ_CRASH("can't VirtualProtect()");
  }
}

bool SharedMemory::SystemProtectFallible(char* aAddr, size_t aSize,
                                         int aRights) {
  DWORD flags;
  if ((aRights & RightsRead) && (aRights & RightsWrite))
    flags = PAGE_READWRITE;
  else if (aRights & RightsRead)
    flags = PAGE_READONLY;
  else
    flags = PAGE_NOACCESS;

  DWORD oldflags;
  return VirtualProtect(aAddr, aSize, flags, &oldflags);
}

size_t SharedMemory::SystemPageSize() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
}

}  // namespace mozilla::ipc
