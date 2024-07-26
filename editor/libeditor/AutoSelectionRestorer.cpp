/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoSelectionRestorer.h"

namespace mozilla {

AutoSelectionRestorer::AutoSelectionRestorer(EditorBase* aEditor) {
  if (!aEditor) {
    return;
  }
  if (aEditor->ArePreservingSelection()) {
    // We already have initialized mParentData::mSavedSelection, so this must
    // be nested call.
    return;
  }
  MOZ_ASSERT(aEditor->IsEditActionDataAvailable());
  mEditor = aEditor;
  mEditor->PreserveSelectionAcrossActions();
}

AutoSelectionRestorer::~AutoSelectionRestorer() {
  if (!mEditor || !mEditor->ArePreservingSelection()) {
    return;
  }
  DebugOnly<nsresult> rvIgnored = mEditor->RestorePreservedSelection();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EditorBase::RestorePreservedSelection() failed, but ignored");
}

void AutoSelectionRestorer::Abort() {
  if (mEditor) {
    mEditor->StopPreservingSelection();
  }
}

}  // namespace mozilla
