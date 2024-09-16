/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CloseWatcherManager_h
#define mozilla_dom_CloseWatcherManager_h

#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nsTObserverArray.h"

namespace mozilla::dom {

class CloseWatcher;

using CloseWatcherArray =
    nsTObserverArray<nsTObserverArray<RefPtr<CloseWatcher>>>;

class CloseWatcherManager : public nsISupports {
 public:
  CloseWatcherManager() = default;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(CloseWatcherManager)

  void NotifyUserInteraction();

  MOZ_CAN_RUN_SCRIPT bool ProcessCloseRequest();

  void Add(CloseWatcher&);

  void Remove(CloseWatcher&);

  bool Contains(const CloseWatcher&) const;

  bool CanGrow() const;

 protected:
  virtual ~CloseWatcherManager() = default;

  CloseWatcherArray mGroups;
  uint32_t mAllowedNumberOfGroups = 1;
  bool mNextUserInteractionAllowsNewGroup = true;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CloseWatcherManager_h
