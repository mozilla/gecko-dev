/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HoldDropJSObjects.h"

#include "mozilla/Assertions.h"
#include "mozilla/CycleCollectedJSRuntime.h"

namespace mozilla {
namespace cyclecollector {

void HoldJSObjectsImpl(void* aHolder, nsScriptObjectTracer* aTracer,
                       JS::Zone* aZone) {
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  rt->AddJSHolder(aHolder, aTracer, aZone);
}

void HoldJSObjectsWithKeyImpl(void* aHolder, nsScriptObjectTracer* aTracer,
                              JSHolderKey* aKey) {
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  rt->AddJSHolderWithKey(aHolder, aTracer, aKey);
}

void HoldJSObjectsImpl(nsISupports* aHolder) {
  nsXPCOMCycleCollectionParticipant* participant = nullptr;
  CallQueryInterface(aHolder, &participant);
  MOZ_ASSERT(participant, "Failed to QI to nsXPCOMCycleCollectionParticipant!");
  MOZ_ASSERT(
      participant->CheckForRightISupports(aHolder),
      "The result of QIing a JS holder should be the same as ToSupports");

  HoldJSObjectsImpl(aHolder, participant);
}

void HoldJSObjectsWithKeyImpl(nsISupports* aHolder, JSHolderKey* aKey) {
  nsXPCOMCycleCollectionParticipant* participant = nullptr;
  CallQueryInterface(aHolder, &participant);
  MOZ_ASSERT(participant, "Failed to QI to nsXPCOMCycleCollectionParticipant!");
  MOZ_ASSERT(
      participant->CheckForRightISupports(aHolder),
      "The result of QIing a JS holder should be the same as ToSupports");

  HoldJSObjectsWithKeyImpl(aHolder, participant, aKey);
}

void DropJSObjectsImpl(void* aHolder) {
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  rt->RemoveJSHolder(aHolder);
}

void DropJSObjectsWithKeyImpl(void* aHolder, JSHolderKey* aKey) {
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  rt->RemoveJSHolderWithKey(aHolder, aKey);
}

void DropJSObjectsImpl(nsISupports* aHolder) {
#ifdef DEBUG
  nsXPCOMCycleCollectionParticipant* participant = nullptr;
  CallQueryInterface(aHolder, &participant);
  MOZ_ASSERT(participant, "Failed to QI to nsXPCOMCycleCollectionParticipant!");
  MOZ_ASSERT(
      participant->CheckForRightISupports(aHolder),
      "The result of QIing a JS holder should be the same as ToSupports");
#endif
  DropJSObjectsImpl(static_cast<void*>(aHolder));
}

void DropJSObjectsWithKeyImpl(nsISupports* aHolder, JSHolderKey* aKey) {
#ifdef DEBUG
  nsXPCOMCycleCollectionParticipant* participant = nullptr;
  CallQueryInterface(aHolder, &participant);
  MOZ_ASSERT(participant, "Failed to QI to nsXPCOMCycleCollectionParticipant!");
  MOZ_ASSERT(
      participant->CheckForRightISupports(aHolder),
      "The result of QIing a JS holder should be the same as ToSupports");
#endif
  DropJSObjectsWithKeyImpl(static_cast<void*>(aHolder), aKey);
}

}  // namespace cyclecollector

}  // namespace mozilla
