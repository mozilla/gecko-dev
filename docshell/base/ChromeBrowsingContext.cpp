/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeBrowsingContext.h"

#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace dom {

ChromeBrowsingContext::ChromeBrowsingContext(BrowsingContext* aParent,
                                             const nsAString& aName,
                                             uint64_t aBrowsingContextId,
                                             uint64_t aProcessId,
                                             BrowsingContext::Type aType)
  : BrowsingContext(aParent, aName, aBrowsingContextId, aType)
  , mProcessId(aProcessId)
{
  // You are only ever allowed to create ChromeBrowsingContexts in the
  // parent process.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
}

// TODO(farre): ChromeBrowsingContext::CleanupContexts starts from the
// list of root BrowsingContexts. This isn't enough when separate
// BrowsingContext nodes of a BrowsingContext tree, not in a crashing
// child process, are from that process and thus needs to be
// cleaned. [Bug 1472108]
/* static */ void
ChromeBrowsingContext::CleanupContexts(uint64_t aProcessId)
{
  nsTArray<RefPtr<BrowsingContext>> roots;
  BrowsingContext::GetRootBrowsingContexts(roots);

  for (RefPtr<BrowsingContext> context : roots) {
    if (Cast(context)->IsOwnedByProcess(aProcessId)) {
      context->Detach();
    }
  }
}

/* static */ already_AddRefed<ChromeBrowsingContext>
ChromeBrowsingContext::Get(uint64_t aId)
{
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return BrowsingContext::Get(aId).downcast<ChromeBrowsingContext>();
}

/* static */ ChromeBrowsingContext*
ChromeBrowsingContext::Cast(BrowsingContext* aContext)
{
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<ChromeBrowsingContext*>(aContext);
}

/* static */ const ChromeBrowsingContext*
ChromeBrowsingContext::Cast(const BrowsingContext* aContext)
{
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<const ChromeBrowsingContext*>(aContext);
}

} // namespace dom
} // namespace mozilla
