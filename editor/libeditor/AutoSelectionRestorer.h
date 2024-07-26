/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AutoSelectionRestorer_h__
#define mozilla_AutoSelectionRestorer_h__

#include "EditorBase.h"

namespace mozilla {

/**
 * Stack based helper class for saving/restoring selection.  Note that this
 * assumes that the nodes involved are still around afterwords!
 */
class AutoSelectionRestorer final {
 public:
  AutoSelectionRestorer() = delete;
  explicit AutoSelectionRestorer(const AutoSelectionRestorer& aOther) = delete;
  AutoSelectionRestorer(AutoSelectionRestorer&& aOther) = delete;

  /**
   * Constructor responsible for remembering all state needed to restore
   * aSelection.
   * XXX This constructor and the destructor should be marked as
   *     `MOZ_CAN_RUN_SCRIPT`, but it's impossible due to this may be used
   *     with `Maybe`.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY explicit AutoSelectionRestorer(
      EditorBase* aEditor);

  /**
   * Destructor restores mSelection to its former state
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY ~AutoSelectionRestorer();

  /**
   * Abort() cancels to restore the selection.
   */
  void Abort();

  bool MaybeRestoreSelectionLater() const { return !!mEditor; }

 protected:
  // The lifetime must be guaranteed by the creator of this instance.
  MOZ_KNOWN_LIVE EditorBase* mEditor = nullptr;
};
}  // namespace mozilla

#endif
