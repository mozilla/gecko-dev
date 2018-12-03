/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinHeaderOnlyUtils_h
#define mozilla_WinHeaderOnlyUtils_h

#include <windows.h>
#include <winerror.h>
#include <winnt.h>
#include <winternl.h>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsHelpers.h"

/**
 * This header is intended for self-contained, header-only, utility code for
 * Win32. It may be used outside of xul.dll, in places such as firefox.exe or
 * mozglue.dll. If your code creates dependencies on Mozilla libraries, you
 * should put it elsewhere.
 */

#if _WIN32_WINNT < _WIN32_WINNT_WIN8
typedef struct _FILE_ID_INFO {
  ULONGLONG VolumeSerialNumber;
  FILE_ID_128 FileId;
} FILE_ID_INFO;

#define FileIdInfo ((FILE_INFO_BY_HANDLE_CLASS)18)

#endif  // _WIN32_WINNT < _WIN32_WINNT_WIN8

#if !defined(STATUS_SUCCESS)
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif  // !defined(STATUS_SUCCESS)

namespace mozilla {

class WindowsError final {
 private:
  // HRESULT and NTSTATUS are both typedefs of LONG, so we cannot use
  // overloading to properly differentiate between the two. Instead we'll use
  // static functions to convert the various error types to HRESULTs before
  // instantiating.
  explicit WindowsError(HRESULT aHResult) : mHResult(aHResult) {}

 public:
  using UniqueString = UniquePtr<WCHAR[], LocalFreeDeleter>;

  static WindowsError FromNtStatus(NTSTATUS aNtStatus) {
    if (aNtStatus == STATUS_SUCCESS) {
      // Special case: we don't want to set FACILITY_NT_BIT
      // (HRESULT_FROM_NT does not handle this case, unlike HRESULT_FROM_WIN32)
      return WindowsError(S_OK);
    }

    return WindowsError(HRESULT_FROM_NT(aNtStatus));
  }

  static WindowsError FromHResult(HRESULT aHResult) {
    return WindowsError(aHResult);
  }

  static WindowsError FromWin32Error(DWORD aWin32Err) {
    return WindowsError(HRESULT_FROM_WIN32(aWin32Err));
  }

  static WindowsError FromLastError() {
    return FromWin32Error(::GetLastError());
  }

  static WindowsError CreateSuccess() { return WindowsError(S_OK); }

  static WindowsError CreateGeneric() {
    return FromWin32Error(ERROR_UNIDENTIFIED_ERROR);
  }

  bool IsSuccess() const { return SUCCEEDED(mHResult); }

  bool IsFailure() const { return FAILED(mHResult); }

  bool IsAvailableAsWin32Error() const {
    return IsAvailableAsNtStatus() ||
           HRESULT_FACILITY(mHResult) == FACILITY_WIN32;
  }

  bool IsAvailableAsNtStatus() const {
    return mHResult == S_OK || (mHResult & FACILITY_NT_BIT);
  }

  bool IsAvailableAsHResult() const { return true; }

  UniqueString AsString() const {
    LPWSTR rawMsgBuf = nullptr;
    DWORD result = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, mHResult, 0, reinterpret_cast<LPWSTR>(&rawMsgBuf), 0, nullptr);
    if (!result) {
      return nullptr;
    }

    return UniqueString(rawMsgBuf);
  }

  HRESULT AsHResult() const { return mHResult; }

  // Not all HRESULTs are convertible to Win32 Errors, so we use Maybe
  Maybe<DWORD> AsWin32Error() const {
    if (mHResult == S_OK) {
      return Some(static_cast<DWORD>(ERROR_SUCCESS));
    }

    if (HRESULT_FACILITY(mHResult) == FACILITY_WIN32) {
      // This is the inverse of HRESULT_FROM_WIN32
      return Some(static_cast<DWORD>(HRESULT_CODE(mHResult)));
    }

    // The NTSTATUS facility is a special case and thus does not utilize the
    // HRESULT_FACILITY and HRESULT_CODE macros.
    if (mHResult & FACILITY_NT_BIT) {
      return Some(NtStatusToWin32Error(
          static_cast<NTSTATUS>(mHResult & ~FACILITY_NT_BIT)));
    }

    return Nothing();
  }

  // Not all HRESULTs are convertible to NTSTATUS, so we use Maybe
  Maybe<NTSTATUS> AsNtStatus() const {
    if (mHResult == S_OK) {
      return Some(STATUS_SUCCESS);
    }

    // The NTSTATUS facility is a special case and thus does not utilize the
    // HRESULT_FACILITY and HRESULT_CODE macros.
    if (mHResult & FACILITY_NT_BIT) {
      return Some(static_cast<NTSTATUS>(mHResult & ~FACILITY_NT_BIT));
    }

    return Nothing();
  }

  static DWORD NtStatusToWin32Error(NTSTATUS aNtStatus) {
    static const DynamicallyLinkedFunctionPtr<decltype(&RtlNtStatusToDosError)>
        pRtlNtStatusToDosError(L"ntdll.dll", "RtlNtStatusToDosError");

    MOZ_ASSERT(!!pRtlNtStatusToDosError);
    if (!pRtlNtStatusToDosError) {
      return ERROR_UNIDENTIFIED_ERROR;
    }

    return pRtlNtStatusToDosError(aNtStatus);
  }

 private:
  // We store the error code as an HRESULT because they can encode both Win32
  // error codes and NTSTATUS codes.
  HRESULT mHResult;
};

template <typename T>
using WindowsErrorResult = Result<T, WindowsError>;

// How long to wait for a created process to become available for input,
// to prevent that process's windows being forced to the background.
// This is used across update, restart, and the launcher.
const DWORD kWaitForInputIdleTimeoutMS = 10 * 1000;

/**
 * Wait for a child GUI process to become "idle." Idle means that the process
 * has created its message queue and has begun waiting for user input.
 *
 * Note that this must only be used when the child process is going to display
 * GUI! Otherwise you're going to be waiting for a very long time ;-)
 *
 * @return true if we successfully waited for input idle;
 *         false if we timed out or failed to wait.
 */
inline bool WaitForInputIdle(HANDLE aProcess,
                             DWORD aTimeoutMs = kWaitForInputIdleTimeoutMS) {
  const DWORD kSleepTimeMs = 10;
  const DWORD waitStart = aTimeoutMs == INFINITE ? 0 : ::GetTickCount();
  DWORD elapsed = 0;

  while (true) {
    if (aTimeoutMs != INFINITE) {
      elapsed = ::GetTickCount() - waitStart;
    }

    if (elapsed >= aTimeoutMs) {
      return false;
    }

    DWORD waitResult = ::WaitForInputIdle(aProcess, aTimeoutMs - elapsed);
    if (!waitResult) {
      return true;
    }

    if (waitResult == WAIT_FAILED &&
        ::GetLastError() == ERROR_NOT_GUI_PROCESS) {
      ::Sleep(kSleepTimeMs);
      continue;
    }

    return false;
  }
}

enum class PathType {
  eNtPath,
  eDosPath,
};

class FileUniqueId final {
 public:
  explicit FileUniqueId(const wchar_t* aPath, PathType aPathType) : mId() {
    if (!aPath) {
      return;
    }

    nsAutoHandle file;

    switch (aPathType) {
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled PathType");
        return;

      case PathType::eNtPath: {
        UNICODE_STRING unicodeString;
        ::RtlInitUnicodeString(&unicodeString, aPath);
        OBJECT_ATTRIBUTES objectAttributes;
        InitializeObjectAttributes(&objectAttributes, &unicodeString,
                                   OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        IO_STATUS_BLOCK ioStatus = {};
        HANDLE ntHandle;
        NTSTATUS status = ::NtOpenFile(
            &ntHandle, SYNCHRONIZE | FILE_READ_ATTRIBUTES, &objectAttributes,
            &ioStatus, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT);
        // We don't need to check |ntHandle| for INVALID_HANDLE_VALUE here,
        // as that value is set by the Win32 layer.
        if (!NT_SUCCESS(status)) {
          mError = Some(WindowsError::FromNtStatus(status));
          return;
        }

        file.own(ntHandle);
      }

      break;

      case PathType::eDosPath: {
        file.own(::CreateFileW(
            aPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        if (file == INVALID_HANDLE_VALUE) {
          mError = Some(WindowsError::FromLastError());
          return;
        }

        break;
      }
    }

    GetId(file);
  }

  explicit FileUniqueId(const nsAutoHandle& aFile) : mId() { GetId(aFile); }

  FileUniqueId(const FileUniqueId& aOther)
      : mId(aOther.mId), mError(aOther.mError) {}

  ~FileUniqueId() = default;

  explicit operator bool() const {
    FILE_ID_INFO zeros = {};
    return !mError && memcmp(&mId, &zeros, sizeof(FILE_ID_INFO));
  }

  Maybe<WindowsError> GetError() const { return mError; }

  FileUniqueId& operator=(const FileUniqueId& aOther) {
    mId = aOther.mId;
    mError = aOther.mError;
    return *this;
  }

  FileUniqueId(FileUniqueId&& aOther) = delete;
  FileUniqueId& operator=(FileUniqueId&& aOther) = delete;

  bool operator==(const FileUniqueId& aOther) const {
    return !mError && !aOther.mError &&
           !memcmp(&mId, &aOther.mId, sizeof(FILE_ID_INFO));
  }

  bool operator!=(const FileUniqueId& aOther) const {
    return !((*this) == aOther);
  }

 private:
  void GetId(const nsAutoHandle& aFile) {
    if (IsWin8OrLater()) {
      if (::GetFileInformationByHandleEx(aFile.get(), FileIdInfo, &mId,
                                         sizeof(mId))) {
        return;
      }
      // Only NTFS and ReFS support FileIdInfo. So we have to fallback if
      // GetFileInformationByHandleEx failed.
    }

    BY_HANDLE_FILE_INFORMATION info = {};
    if (!::GetFileInformationByHandle(aFile.get(), &info)) {
      mError = Some(WindowsError::FromLastError());
      return;
    }

    mId.VolumeSerialNumber = info.dwVolumeSerialNumber;
    memcpy(&mId.FileId.Identifier[0], &info.nFileIndexLow, sizeof(DWORD));
    memcpy(&mId.FileId.Identifier[sizeof(DWORD)], &info.nFileIndexHigh,
           sizeof(DWORD));
  }

 private:
  FILE_ID_INFO mId;
  Maybe<WindowsError> mError;
};

inline WindowsErrorResult<bool> DoPathsPointToIdenticalFile(
    const wchar_t* aPath1, const wchar_t* aPath2,
    PathType aPathType1 = PathType::eDosPath,
    PathType aPathType2 = PathType::eDosPath) {
  FileUniqueId id1(aPath1, aPathType1);
  if (!id1) {
    Maybe<WindowsError> error = id1.GetError();
    return Err(error.valueOr(WindowsError::CreateGeneric()));
  }

  FileUniqueId id2(aPath2, aPathType2);
  if (!id2) {
    Maybe<WindowsError> error = id2.GetError();
    return Err(error.valueOr(WindowsError::CreateGeneric()));
  }

  return id1 == id2;
}

}  // namespace mozilla

#endif  // mozilla_WinHeaderOnlyUtils_h
