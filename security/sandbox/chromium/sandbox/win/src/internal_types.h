// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_INTERNAL_TYPES_H_
#define SANDBOX_WIN_SRC_INTERNAL_TYPES_H_

namespace sandbox {

const wchar_t kNtdllName[] = L"ntdll.dll";
const wchar_t kKerneldllName[] = L"kernel32.dll";
const wchar_t kKernelBasedllName[] = L"kernelbase.dll";

// Defines the supported C++ types encoding to numeric id. Like a poor's man
// RTTI. Note that true C++ RTTI will not work because the types are not
// polymorphic anyway.
enum ArgType {
  INVALID_TYPE = 0,
  WCHAR_TYPE,
  UINT32_TYPE,
  UNISTR_TYPE,
  VOIDPTR_TYPE,
  INPTR_TYPE,
  INOUTPTR_TYPE,
  LAST_TYPE
};

// Encapsulates a pointer to a buffer and the size of the buffer.
class CountedBuffer {
 public:
  CountedBuffer(void* buffer, uint32 size) : size_(size), buffer_(buffer) {}

  uint32 Size() const {
    return size_;
  }

  void* Buffer() const {
    return buffer_;
  }

 private:
  uint32 size_;
  void* buffer_;
};

// Helper class to convert void-pointer packed ints for both
// 32 and 64 bit builds. This construct is non-portable.
class IPCInt {
 public:
  explicit IPCInt(void* buffer) {
    buffer_.vp = buffer;
  }

  explicit IPCInt(unsigned __int32 i32) {
    buffer_.vp = NULL;
    buffer_.i32 = i32;
  }

  unsigned __int32 As32Bit() const {
    return buffer_.i32;
  }

  void* AsVoidPtr() const {
    return buffer_.vp;
  }

 private:
  union U {
    void* vp;
    unsigned __int32 i32;
  } buffer_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_INTERNAL_TYPES_H_
