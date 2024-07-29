/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AutoCopyListener_h
#define mozilla_AutoCopyListener_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/StaticPrefs_clipboard.h"
#include "nsIClipboard.h"

namespace mozilla {

class AutoCopyListener final {
 public:
  /**
   * OnSelectionChange() is called when a Selection whose NotifyAutoCopy() was
   * called is changed.
   *
   * @param aDocument           The document of the Selection.  May be nullptr.
   * @param aSelection          The selection.
   * @param aReason             The reasons of the change.
   *                            See nsISelectionListener::*_REASON.
   */
  static void OnSelectionChange(dom::Document* aDocument,
                                dom::Selection& aSelection, int16_t aReason);

  /**
   * IsEnabled() returns true if the auto-copy feature is enabled.
   */
  static bool IsEnabled() {
#ifdef XP_MACOSX
    return true;
#else
    return StaticPrefs::clipboard_autocopy();
#endif
  }

 private:
#ifdef XP_MACOSX
  // On macOS, cache the current selection to send to service menu of macOS.
  static const nsIClipboard::ClipboardType sClipboardID =
      nsIClipboard::kSelectionCache;
#else
  // Make the normal Selection notifies auto-copy listener of its changes.
  static const nsIClipboard::ClipboardType sClipboardID =
      nsIClipboard::kSelectionClipboard;
#endif
};

}  // namespace mozilla

#endif  // #ifndef mozilla_AutoCopyListener_h
