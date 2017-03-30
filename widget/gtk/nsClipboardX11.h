/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardX11_h_
#define __nsClipboardX11_h_

#include "nsIClipboard.h"
#include <gtk/gtk.h>

class nsRetrievalContextX11 : public nsRetrievalContext
{
public:
    enum State { INITIAL, COMPLETED, TIMED_OUT };

    nsRetrievalContextX11();

    NS_IMETHOD HasDataMatchingFlavors(const char** aFlavorList,
                                      uint32_t aLength,
                                      int32_t aWhichClipboard,
                                      bool *_retval) override;
    NS_IMETHOD GetClipboardContent(const char* aMimeType,
                                   int32_t aWhichClipboard,
                                   nsIInputStream** aResult,
                                   uint32_t* aContentLength) override;

    gchar* CopyRetrievedData(const gchar *aData)
    {
      return g_strdup(aData);
    }
    GtkSelectionData* CopyRetrievedData(GtkSelectionData *aData)
    {
      // A negative length indicates that retrieving the data failed.
      return gtk_selection_data_get_length(aData) >= 0 ?
          gtk_selection_data_copy(aData) : nullptr;
    }

    // Call this when data has been retrieved.
    template <class T> void Complete(T *aData)
    {
      if (mState == INITIAL) {
          mState = COMPLETED;
          mData = CopyRetrievedData(aData);
      } else {
          // Already timed out
          MOZ_ASSERT(mState == TIMED_OUT);
      }
    }
private:
    virtual ~nsRetrievalContextX11() override;

    GtkSelectionData* WaitForContents(GtkClipboard *clipboard,
                                      const char *aMimeType);
    /**
     * Spins X event loop until timing out or being completed. Returns
     * null if we time out, otherwise returns the completed data (passing
     * ownership to caller).
     */
    void *Wait();

    State mState;
    void* mData;
};

#endif /* __nsClipboardX11_h_ */
