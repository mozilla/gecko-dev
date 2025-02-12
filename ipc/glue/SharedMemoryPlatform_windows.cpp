/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This source code was derived from Chromium code, and as such is also subject
 * to the [Chromium license](ipc/chromium/src/LICENSE). */

#include "SharedMemoryPlatform.h"

#include <windows.h>

#include "nsDebug.h"
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
bool IsSectionSafeToMap(HANDLE aHandle) {
  static NtQuerySectionType nt_query_section_func =
      reinterpret_cast<NtQuerySectionType>(
          ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "NtQuerySection"));
  DCHECK(nt_query_section_func);

  // The handle must have SECTION_QUERY access for this to succeed.
  SECTION_BASIC_INFORMATION basic_information = {};
  ULONG status = nt_query_section_func(aHandle, SectionBasicInformation,
                                       &basic_information,
                                       sizeof(basic_information), nullptr);
  if (status) {
    return false;
  }

  return (basic_information.Attributes & SEC_IMAGE) != SEC_IMAGE;
}

// Wrapper around CreateFileMappingW for pagefile-backed regions. When out of
// memory, may attempt to stall and retry rather than returning immediately, in
// hopes that the page file is about to be expanded by Windows. (bug 1822383,
// bug 1716727)
//
// This method is largely a copy of the MozVirtualAlloc method from
// mozjemalloc.cpp, which implements this strategy for VirtualAlloc calls,
// except re-purposed to handle CreateFileMapping.
HANDLE MozCreateFileMappingW(LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                             DWORD flProtect, DWORD dwMaximumSizeHigh,
                             DWORD dwMaximumSizeLow, LPCWSTR lpName) {
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

}  // namespace

namespace mozilla::ipc::shared_memory {

static Maybe<PlatformHandle> CreateImpl(size_t aSize, bool aFreezable) {
  // If the shared memory object has no DACL, any process can
  // duplicate its handles with any access rights; e.g., re-add write
  // access to a read-only handle.  To prevent that, we give it an
  // empty DACL, so that no process can do that.
  SECURITY_ATTRIBUTES sa, *psa = nullptr;
  SECURITY_DESCRIPTOR sd;
  ACL dacl;

  if (aFreezable) {
    psa = &sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    if (NS_WARN_IF(!::InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) ||
        NS_WARN_IF(!::InitializeSecurityDescriptor(
            &sd, SECURITY_DESCRIPTOR_REVISION)) ||
        NS_WARN_IF(!::SetSecurityDescriptorDacl(&sd, TRUE, &dacl, FALSE))) {
      return Nothing();
    }
  }

  auto handle = MozCreateFileMappingW(psa, PAGE_READWRITE, 0,
                                      static_cast<DWORD>(aSize), nullptr);
  if (!handle) {
    return Nothing();
  } else {
    return Some(handle);
  }
}

bool Platform::Create(Handle& aHandle, size_t aSize) {
  if (auto ph = CreateImpl(aSize, false)) {
    aHandle.mHandle = std::move(*ph);
    aHandle.mSize = aSize;
    return true;
  }
  return false;
}

bool Platform::CreateFreezable(FreezableHandle& aHandle, size_t aSize) {
  if (auto ph = CreateImpl(aSize, true)) {
    aHandle.mHandle = std::move(*ph);
    aHandle.mSize = aSize;
    return true;
  }
  return false;
}

PlatformHandle Platform::CloneHandle(const PlatformHandle& aHandle) {
  HANDLE h = INVALID_HANDLE_VALUE;
  if (::DuplicateHandle(::GetCurrentProcess(), aHandle.get(),
                        ::GetCurrentProcess(), &h, 0, false,
                        DUPLICATE_SAME_ACCESS)) {
    return PlatformHandle(h);
  }
  NS_WARNING("DuplicateHandle Failed!");
  return nullptr;
}

bool Platform::Freeze(FreezableHandle& aHandle) {
  HANDLE ro_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), aHandle.mHandle.get(),
                         ::GetCurrentProcess(), &ro_handle,
                         GENERIC_READ | FILE_MAP_READ, false, 0)) {
    return false;
  }

  aHandle.mHandle.reset(ro_handle);
  return true;
}

Maybe<void*> Platform::Map(const HandleBase& aHandle, void* aFixedAddress,
                           bool aReadOnly) {
  void* mem = ::MapViewOfFileEx(
      aHandle.mHandle.get(),
      aReadOnly ? FILE_MAP_READ : FILE_MAP_READ | FILE_MAP_WRITE, 0, 0,
      aHandle.Size(), aFixedAddress);
  if (mem) {
    MOZ_ASSERT(!aFixedAddress || mem == aFixedAddress,
               "MapViewOfFileEx returned an expected address");
    return Some(mem);
  }
  return Nothing();
}

void Platform::Unmap(void* aMemory, size_t aSize) {
  ::UnmapViewOfFile(aMemory);
}

bool Platform::Protect(char* aAddr, size_t aSize, Access aAccess) {
  DWORD flags;
  if ((aAccess & AccessReadWrite) == AccessReadWrite)
    flags = PAGE_READWRITE;
  else if (aAccess & AccessRead)
    flags = PAGE_READONLY;
  else
    flags = PAGE_NOACCESS;

  DWORD oldflags;
  return ::VirtualProtect(aAddr, aSize, flags, &oldflags);
}

void* Platform::FindFreeAddressSpace(size_t aSize) {
  void* memory = ::VirtualAlloc(NULL, aSize, MEM_RESERVE, PAGE_NOACCESS);
  if (memory) {
    ::VirtualFree(memory, 0, MEM_RELEASE);
  }
  return memory;
}

size_t Platform::PageSize() {
  SYSTEM_INFO si;
  ::GetSystemInfo(&si);
  return si.dwPageSize;
}

bool Platform::IsSafeToMap(const PlatformHandle& aHandle) {
  return IsSectionSafeToMap(aHandle.get());
}

}  // namespace mozilla::ipc::shared_memory
