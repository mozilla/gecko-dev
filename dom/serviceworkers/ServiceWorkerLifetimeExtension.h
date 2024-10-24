/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerlifetimeextension_h
#define mozilla_dom_serviceworkerlifetimeextension_h

#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"

namespace mozilla::dom {

// Do not extend the ServiceWorker's lifetime.  This should only be used for
// special internal cases like sending a termination op.  If you are thinking
// of using this for other purposes, you probably should not be using the
// ServiceWorkerOp mechanism.
struct NoLifetimeExtension {};

// Propagated lifetime extension allows us to ensure that a ServiceWorker
// using ServiceWorker.postMessage to contact another ServiceWorker cannot
// extend the lifetime of the recipient ServiceWorker beyond the lifetime of
// the sender.
struct PropagatedLifetimeExtension {
  // We propagate the lifetime as a timestamp-as-deadline rather than a
  // duration because a duration is effectively frozen in time until it is
  // applied, providing potential for abuse due to the inherently async nature
  // of the events involved.
  //
  // It is possible for this value to be in the past by the time it is
  // processed.  It is also possible for this value to be null because of
  // async delays between the transmission of a message from one ServiceWorker
  // (in the content process) and the time it is received in the parent
  // process and/or because the sending ServiceWorker has reached its deadline
  // but is in its "grace period".  We do not attempt to normalize these cases
  // into `NoLifetimeExtension`.
  TimeStamp mDeadline;
};

// For functional events that are initiated by window clients or very specific
// APIs like the Push API where care has been taken to ensure that Service
// Workers can only run without having a tab open under very specific
// circumstances that have been extensively discussed with the standards,
// privacy, and security teams.
struct FullLifetimeExtension {};

/**
 * Conveys how events dispatched at a Service Worker global should impact its
 * lifetime.
 */
struct ServiceWorkerLifetimeExtension
    : public Variant<NoLifetimeExtension, PropagatedLifetimeExtension,
                     FullLifetimeExtension> {
 public:
  explicit ServiceWorkerLifetimeExtension(NoLifetimeExtension aExt)
      : Variant(AsVariant(std::move(aExt))) {}
  explicit ServiceWorkerLifetimeExtension(PropagatedLifetimeExtension aExt)
      : Variant(AsVariant(std::move(aExt))) {}
  explicit ServiceWorkerLifetimeExtension(FullLifetimeExtension aExt)
      : Variant(AsVariant(std::move(aExt))) {}

  // Check whether this lifetime extends at least the provided number of
  // seconds into the future.  This is for use in situations where we might
  // freshly spawn a new ServiceWorker like `SpawnWorkerIfNeeded`.  This helps
  // compensate for the fixed costs to spawning a ServiceWorker as well as the
  // assumption that a ServiceWorker needs at least a minimum amount of run time
  // to accomplish something.  Note that a spawned ServiceWorker will still
  // potentially be able to leverage the
  // "dom.serviceWorkers.idle_extended_timeout" grace period, which with current
  // pref values means an extra 30 seconds of potential execution time.  (But
  // the grace period never counts for propagated deadline purposes.)
  bool LifetimeExtendsIntoTheFuture(uint32_t aRequiredSecs = 5) const;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworkerlifetimeextension_h
