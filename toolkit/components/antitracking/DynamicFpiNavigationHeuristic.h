/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dynamicfpinavigationheuristic_h
#define mozilla_dynamicfpinavigationheuristic_h

#include "mozilla/WeakPtr.h"
#include "nsIEffectiveTLDService.h"
#include "nsIPrincipal.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

namespace mozilla {

namespace dom {
class BrowsingContextWebProgress;
class CanonicalBrowsingContext;
}  // namespace dom

class DynamicFpiNavigationHeuristic {
 public:
  // Given a browsing context and a document principal that is about to be
  // opened, grant storage access to any origin that was interacted during the
  // ongoing "extended navigation" (as defined by bounce tracking) if a page
  // from the same-site-host is further back in the history of this tab.
  //
  // More simply, this is a tightened down version of the redirect heuristic
  // that looks for something that is a lot like a redirect auth flow where you
  // interact with the intervening page.
  static void MaybeGrantStorageAccess(
      dom::CanonicalBrowsingContext* aBrowsingContext,
      nsIChannel* aChannel);
};

}  // namespace mozilla

#endif  // mozilla_dynamicfpinavigationheuristic_h
