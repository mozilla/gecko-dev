/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotacommon_h__
#define mozilla_dom_quota_quotacommon_h__

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsTArray.h"

#define BEGIN_QUOTA_NAMESPACE \
  namespace mozilla { namespace dom { namespace quota {
#define END_QUOTA_NAMESPACE \
  } /* namespace quota */ } /* namespace dom */ } /* namespace mozilla */
#define USING_QUOTA_NAMESPACE \
  using namespace mozilla::dom::quota;

#define DSSTORE_FILE_NAME ".DS_Store"

#define QM_WARNING(...)                                                        \
  do {                                                                         \
    nsPrintfCString str(__VA_ARGS__);                                          \
    mozilla::dom::quota::ReportInternalError(__FILE__, __LINE__, str.get());   \
    NS_WARNING(str.get());                                                     \
  } while (0)

class nsIEventTarget;

BEGIN_QUOTA_NAMESPACE

class BackgroundThreadObject
{
protected:
  nsCOMPtr<nsIEventTarget> mOwningThread;

public:
  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  nsIEventTarget*
  OwningThread() const;

protected:
  BackgroundThreadObject();

  explicit BackgroundThreadObject(nsIEventTarget* aOwningThread);
};

void
AssertIsOnIOThread();

void
AssertCurrentThreadOwnsQuotaMutex();

bool
IsOnIOThread();

void
ReportInternalError(const char* aFile, uint32_t aLine, const char* aStr);

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_quotacommon_h__
