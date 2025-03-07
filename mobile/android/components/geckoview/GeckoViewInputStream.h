/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoViewInputStream_h__
#define GeckoViewInputStream_h__

#include "mozilla/java/GeckoViewInputStreamWrappers.h"
#include "mozilla/java/ContentInputStreamWrappers.h"
#include "nsIAndroidContentInputStream.h"
#include "nsIInputStream.h"

class GeckoViewInputStream : public nsIAndroidContentInputStream {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIANDROIDCONTENTINPUTSTREAM

  GeckoViewInputStream() = default;

  bool IsClosed() const;

 protected:
  explicit GeckoViewInputStream(
      mozilla::java::GeckoViewInputStream::LocalRef aInstance)
      : mInstance(aInstance) {};

  virtual ~GeckoViewInputStream() = default;

 private:
  mozilla::java::GeckoViewInputStream::GlobalRef mInstance;
  bool mClosed{false};
};

class GeckoViewContentInputStream final : public GeckoViewInputStream {
 public:
  enum class Allow {
    All,
    PDFOnly,
  };
  static nsresult GetInstance(const nsAutoCString& aUri, Allow aAllow,
                              nsIInputStream** aInstance);
  static bool isReadable(const nsAutoCString& aUri);

 private:
  explicit GeckoViewContentInputStream(const nsAutoCString& aUri, bool aPDFOnly)
      : GeckoViewInputStream(mozilla::java::ContentInputStream::GetInstance(
            mozilla::jni::StringParam(aUri), aPDFOnly)) {}
};

#endif  // !GeckoViewInputStream_h__
