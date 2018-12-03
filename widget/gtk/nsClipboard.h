/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboard_h_
#define __nsClipboard_h_

#include "nsIClipboard.h"
#include "nsIObserver.h"
#include "nsIBinaryOutputStream.h"
#include <gtk/gtk.h>

class nsRetrievalContext {
 public:
  // Get actual clipboard content (GetClipboardData/GetClipboardText)
  // which has to be released by ReleaseClipboardData().
  virtual const char* GetClipboardData(const char* aMimeType,
                                       int32_t aWhichClipboard,
                                       uint32_t* aContentLength) = 0;
  virtual const char* GetClipboardText(int32_t aWhichClipboard) = 0;
  virtual void ReleaseClipboardData(const char* aClipboardData) = 0;

  // Get data mime types which can be obtained from clipboard.
  // The returned array has to be released by g_free().
  virtual GdkAtom* GetTargets(int32_t aWhichClipboard, int* aTargetNum) = 0;

  virtual bool HasSelectionSupport(void) = 0;

  virtual ~nsRetrievalContext(){};
};

class nsClipboard : public nsIClipboard, public nsIObserver {
 public:
  nsClipboard();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSICLIPBOARD

  // Make sure we are initialized, called from the factory
  // constructor
  nsresult Init(void);

  // Someone requested the selection
  void SelectionGetEvent(GtkClipboard* aGtkClipboard,
                         GtkSelectionData* aSelectionData);
  void SelectionClearEvent(GtkClipboard* aGtkClipboard);

 private:
  virtual ~nsClipboard();

  // Save global clipboard content to gtk
  nsresult Store(void);

  // Get our hands on the correct transferable, given a specific
  // clipboard
  nsITransferable* GetTransferable(int32_t aWhichClipboard);

  // Send clipboard data by nsITransferable
  void SetTransferableData(nsITransferable* aTransferable, nsCString& aFlavor,
                           const char* aClipboardData,
                           uint32_t aClipboardDataLength);

  // Hang on to our owners and transferables so we can transfer data
  // when asked.
  nsCOMPtr<nsIClipboardOwner> mSelectionOwner;
  nsCOMPtr<nsIClipboardOwner> mGlobalOwner;
  nsCOMPtr<nsITransferable> mSelectionTransferable;
  nsCOMPtr<nsITransferable> mGlobalTransferable;
  nsAutoPtr<nsRetrievalContext> mContext;
};

extern const int kClipboardTimeout;

GdkAtom GetSelectionAtom(int32_t aWhichClipboard);

#endif /* __nsClipboard_h_ */
