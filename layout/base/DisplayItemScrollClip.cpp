/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayItemScrollClip.h"

#include "DisplayItemClip.h"

namespace mozilla {

/* static */ bool
DisplayItemScrollClip::IsAncestor(const DisplayItemScrollClip* aAncestor,
                                  const DisplayItemScrollClip* aDescendant)
{
  if (!aAncestor) {
    // null means root.
    return true;
  }

  for (const DisplayItemScrollClip* sc = aDescendant; sc; sc = sc->mParent) {
    if (sc == aAncestor) {
      return true;
    }
  }

  return false;
}

bool
DisplayItemScrollClip::HasRoundedCorners() const
{
  for (const DisplayItemScrollClip* scrollClip = this;
       scrollClip; scrollClip = scrollClip->mParent) {
    if (scrollClip->mClip->GetRoundedRectCount() > 0) {
      return true;
    }
  }
  return false;
}

/* static */ nsCString
DisplayItemScrollClip::ToString(const DisplayItemScrollClip* aScrollClip)
{
  nsAutoCString str;
  for (const DisplayItemScrollClip* scrollClip = aScrollClip;
       scrollClip; scrollClip = scrollClip->mParent) {
    str.AppendPrintf("<%s>%s", scrollClip->mClip ? scrollClip->mClip->ToString().get() : "null",
                     scrollClip->mIsAsyncScrollable ? " [async-scrollable]" : "");
    if (scrollClip->mParent) {
      str.Append(", ");
    }
  }
  return str;
}

} // namespace mozilla
