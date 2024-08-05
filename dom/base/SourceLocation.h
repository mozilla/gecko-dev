/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SourceLocation_h
#define mozilla_SourceLocation_h

#include "nsString.h"
#include "mozilla/Variant.h"
#include "nsCOMPtr.h"

struct JSContext;
class nsIURI;

namespace mozilla {

struct SourceLocation {
  mozilla::Variant<nsCString, nsCOMPtr<nsIURI>> mResource{VoidCString()};
  uint32_t mLine = 0;
  uint32_t mColumn = 1;

  SourceLocation();
  explicit SourceLocation(nsCString&&, uint32_t aLine = 0, uint32_t aCol = 1);
  explicit SourceLocation(nsCOMPtr<nsIURI>&&, uint32_t aLine = 0,
                          uint32_t aCol = 1);

  ~SourceLocation();

  bool IsEmpty() const {
    return mResource.is<nsCString>() ? mResource.as<nsCString>().IsEmpty()
                                     : !mResource.as<nsCOMPtr<nsIURI>>();
  }
  explicit operator bool() const { return !IsEmpty(); }
};

struct JSCallingLocation : SourceLocation {
  const nsCString& FileName() const { return mResource.as<nsCString>(); }

  static JSCallingLocation Get();
  static JSCallingLocation Get(JSContext*);

  class MOZ_STACK_CLASS AutoFallback {
   public:
    explicit AutoFallback(const JSCallingLocation* aFallback)
        : mOldFallback(GetFallback()) {
      SetFallback(aFallback);
    }
    ~AutoFallback() { SetFallback(mOldFallback); }

   private:
    const JSCallingLocation* mOldFallback;
  };

 private:
  static const JSCallingLocation* GetFallback();
  static void SetFallback(const JSCallingLocation*);
};

}  // namespace mozilla

#endif
