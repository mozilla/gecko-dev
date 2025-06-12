/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_MediaActorUtils_h
#define include_dom_media_ipc_MediaActorUtils_h

#include "nsISupportsImpl.h"

// This refcounting specialization allows the implementing class to supply a
// method to call when there is only one reference left. This allows for media
// IPDL actors to be refcounted normally, and when the last reference is the
// IPDL actor, we can choose to self destroy.
#define MEDIA_INLINE_DECL_THREADSAFE_REFCOUNTING_META(_class, _decl, _destroy, \
                                                      _last_ref, ...)          \
 public:                                                                       \
  _decl(MozExternalRefCountType) AddRef(void) __VA_ARGS__ {                    \
    MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(_class)                                 \
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                       \
    nsrefcnt count = ++mRefCnt;                                                \
    NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                        \
    return (nsrefcnt)count;                                                    \
  }                                                                            \
  _decl(MozExternalRefCountType) Release(void) __VA_ARGS__ {                   \
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                           \
    nsrefcnt count = --mRefCnt;                                                \
    NS_LOG_RELEASE(this, count, #_class);                                      \
    if (count == 0) {                                                          \
      _destroy;                                                                \
      return 0;                                                                \
    }                                                                          \
    if (count == 1) {                                                          \
      _last_ref;                                                               \
    }                                                                          \
    return count;                                                              \
  }                                                                            \
  using HasThreadSafeRefCnt = std::true_type;                                  \
                                                                               \
 protected:                                                                    \
  ::mozilla::ThreadSafeAutoRefCnt mRefCnt;                                     \
                                                                               \
 public:

#endif  // include_dom_media_ipc_MediaActorUtils_h
