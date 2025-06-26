/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PageloadEvent_h
#define mozilla_PageloadEvent_h

#include <cstdint>

#include "nsCOMPtr.h"
#include "nsString.h"

class nsIChannel;
class nsIURI;

namespace IPC {
template <typename>
struct ParamTraits;
}

namespace mozilla::glean::perf {
struct PageLoadExtra;
struct PageLoadDomainExtra;
}  // namespace mozilla::glean::perf

// This is a list of metrics that exist in either PageloadExtra or
// PageloadDomainExtra. The only exclusion is the domain field since it
// requires some special handling.
#define FOR_EACH_PAGELOAD_METRIC(_) \
  _(dnsLookupTime, uint32_t)        \
  _(documentFeatures, uint32_t)     \
  _(fcpTime, uint32_t)              \
  _(hasSsd, bool)                   \
  _(httpVer, uint32_t)              \
  _(jsExecTime, uint32_t)           \
  _(delazifyTime, uint32_t)         \
  _(lcpTime, uint32_t)              \
  _(loadTime, uint32_t)             \
  _(loadType, nsCString)            \
  _(redirectCount, uint32_t)        \
  _(redirectTime, uint32_t)         \
  _(responseTime, uint32_t)         \
  _(sameOriginNav, bool)            \
  _(timeToRequestStart, uint32_t)   \
  _(tlsHandshakeTime, uint32_t)     \
  _(trrDomain, nsCString)           \
  _(userFeatures, uint32_t)         \
  _(usingWebdriver, bool)

namespace mozilla::performance::pageload_event {
/*
 *  Features utilized within a document, represented as bitfield in the pageload
 * event.
 */
enum UserFeature : uint32_t { USING_A11Y = 1 << 0 };

enum DocumentFeature : uint32_t { FETCH_PRIORITY_IMAGES = 1 << 0 };

// Type of pageload event that will fire after loading has finished.
// - kNormal:  Default pageload event type which contains non-sensitive
// information.
// - kDomain: Pageload event type which contains minimal metric information and
// the ETLD+1 (i.e. example.com) of the page visited.  Sent with ohttp.
// - kNone: No pageload event is sent.
enum class PageloadEventType { kNormal, kDomain, kNone };

// Randomly decides what type of pageload event to send.
extern PageloadEventType GetPageloadEventType();

// Pageload event data is stored in this struct and converted to the
// glean representation when submitted.

#define DEFINE_METRIC(name, type) mozilla::Maybe<type> name;
#define DEFINE_SETTER(name, type) \
  void set_##name(const type& value) { this->name = mozilla::Some(value); }

class PageloadEventData {
  friend struct IPC::ParamTraits<PageloadEventData>;

  // Define each member.
  FOR_EACH_PAGELOAD_METRIC(DEFINE_METRIC)

  // Define ETLD separately since we want a special setter for it.
  mozilla::Maybe<nsCString> mDomain;

 public:
  // Define a setter for every member.
  FOR_EACH_PAGELOAD_METRIC(DEFINE_SETTER)

  bool MaybeSetPublicRegistrableDomain(nsCOMPtr<nsIURI> aURI,
                                       nsIChannel* aChannel);
  bool HasDomain() { return mDomain.isSome() && !mDomain.value().IsEmpty(); }

  bool HasLoadTime() { return loadTime.isSome(); }

  // Define setters for the individual bit features.
  void SetUserFeature(UserFeature aFeature);
  void SetDocumentFeature(DocumentFeature aFeature);

  mozilla::glean::perf::PageLoadExtra ToPageLoadExtra() const;
  mozilla::glean::perf::PageLoadDomainExtra ToPageLoadDomainExtra() const;
};
#undef DEFINE_METRIC
#undef ASSIGN_METRIC

}  // namespace mozilla::performance::pageload_event

#endif /* mozilla_PageloadEvent_h___ */
