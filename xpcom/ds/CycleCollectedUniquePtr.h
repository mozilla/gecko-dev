/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedUniquePtr_h__
#define mozilla_CycleCollectedUniquePtr_h__

#include "mozilla/UniquePtr.h"
#include "nsCycleCollectionContainerParticipant.h"

namespace mozilla {
template <typename T>
inline void ImplCycleCollectionUnlink(mozilla::UniquePtr<T>& aField) {
  aField.reset();
}

template <typename Container, typename Callback,
          EnableCycleCollectionIf<Container, mozilla::UniquePtr> = nullptr>
inline void ImplCycleCollectionContainer(Container&& aField,
                                         Callback&& aCallback) {
  if (aField) {
    aCallback(*aField);
  }
}
}  // namespace mozilla

#endif  // mozilla_CycleCollectedUniquePtr_h__
