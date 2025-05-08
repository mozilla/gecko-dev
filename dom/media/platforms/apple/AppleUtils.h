/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility functions to help with Apple API calls.

#ifndef mozilla_AppleUtils_h
#define mozilla_AppleUtils_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include <CoreFoundation/CFBase.h>      // For CFRelease()
#include <CoreVideo/CVBuffer.h>         // For CVBufferRelease()
#include <VideoToolbox/VideoToolbox.h>  // For VTCompressionSessionRef

#if TARGET_OS_IPHONE
inline bool OSSupportsSVC() {
  // TODO
  return false;
}
#else
#  include "nsCocoaFeatures.h"
inline bool OSSupportsSVC() {
  return nsCocoaFeatures::IsAtLeastVersion(11, 3, 0);
}
#endif

namespace mozilla {

template <typename T>
struct AutoTypeRefTraits;

enum class AutoTypePolicy { Retain, NoRetain };
template <typename T, typename Traits = AutoTypeRefTraits<T>>
class AutoTypeRef {
 public:
  explicit AutoTypeRef(T aObj = Traits::InvalidValue(),
                       AutoTypePolicy aPolicy = AutoTypePolicy::NoRetain)
      : mObj(aObj) {
    if (mObj != Traits::InvalidValue()) {
      if (aPolicy == AutoTypePolicy::Retain) {
        mObj = Traits::Retain(mObj);
      }
    }
  }

  ~AutoTypeRef() { ReleaseIfNeeded(); }

  // Copy constructor
  AutoTypeRef(const AutoTypeRef<T, Traits>& aOther) : mObj(aOther.mObj) {
    if (mObj != Traits::InvalidValue()) {
      mObj = Traits::Retain(mObj);
    }
  }

  // Copy assignment
  AutoTypeRef<T, Traits>& operator=(const AutoTypeRef<T, Traits>& aOther) {
    ReleaseIfNeeded();
    mObj = aOther.mObj;
    if (mObj != Traits::InvalidValue()) {
      mObj = Traits::Retain(mObj);
    }
    return *this;
  }

  // Move constructor
  AutoTypeRef(AutoTypeRef<T, Traits>&& aOther) : mObj(aOther.Take()) {}

  // Move assignment
  AutoTypeRef<T, Traits>& operator=(const AutoTypeRef<T, Traits>&& aOther) {
    Reset(aOther.Take(), AutoTypePolicy::NoRetain);
    return *this;
  }

  explicit operator bool() const { return mObj != Traits::InvalidValue(); }

  operator T() { return mObj; }

  T& Ref() { return mObj; }

  T* Receive() {
    MOZ_ASSERT(mObj == Traits::InvalidValue(),
               "Receive() should only be called for uninitialized objects");
    return &mObj;
  }

  void Reset(T aObj = Traits::InvalidValue(),
             AutoTypePolicy aPolicy = AutoTypePolicy::NoRetain) {
    ReleaseIfNeeded();
    mObj = aObj;
    if (mObj != Traits::InvalidValue()) {
      if (aPolicy == AutoTypePolicy::Retain) {
        mObj = Traits::Retain(mObj);
      } else {
        mObj = aObj;
      }
    }
  }

 private:
  T Take() {
    T obj = mObj;
    mObj = Traits::InvalidValue();
    return obj;
  }

  void ReleaseIfNeeded() {
    if (mObj != Traits::InvalidValue()) {
      Traits::Release(mObj);
      mObj = Traits::InvalidValue();
    }
  }
  T mObj;
};

template <typename CFT>
struct CFTypeRefTraits {
  static CFT InvalidValue() { return nullptr; }
  static CFT Retain(CFT aObject) {
    CFRetain(aObject);
    return aObject;
  }
  static void Release(CFT aObject) { CFRelease(aObject); }
};

template <typename CVB>
struct CVBufferRefTraits {
  static CVB InvalidValue() { return nullptr; }
  static CVB Retain(CVB aObject) {
    CVBufferRetain(aObject);
    return aObject;
  }
  static void Release(CVB aObject) { CVBufferRelease(aObject); }
};

template <typename CFT>
using AutoCFTypeRef = AutoTypeRef<CFT, CFTypeRefTraits<CFT>>;
template <typename CVB>
using AutoCVBufferRef = AutoTypeRef<CVB, CVBufferRefTraits<CVB>>;

class MOZ_RAII SessionPropertyManager {
 public:
  explicit SessionPropertyManager(
      const AutoCFTypeRef<VTCompressionSessionRef>& aSession);
  explicit SessionPropertyManager(const VTCompressionSessionRef& aSession);
  ~SessionPropertyManager() = default;

  bool IsSupported(CFStringRef aKey);

  OSStatus Set(CFStringRef aKey, int32_t aValue);
  OSStatus Set(CFStringRef aKey, int64_t aValue);
  OSStatus Set(CFStringRef aKey, float aValue);
  OSStatus Set(CFStringRef aKey, bool aValue);
  OSStatus Set(CFStringRef aKey, CFStringRef value);
  OSStatus Copy(CFStringRef aKey, bool& aValue);

 private:
  template <typename V>
  OSStatus Set(CFStringRef aKey, V aValue, CFNumberType aType);

  AutoCFTypeRef<VTCompressionSessionRef> mSession;
  AutoCFTypeRef<CFDictionaryRef> mSupportedKeys;
};

}  // namespace mozilla

#endif  // mozilla_AppleUtils_h
