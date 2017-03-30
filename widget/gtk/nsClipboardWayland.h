/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsClipboardWayland_h_
#define __nsClipboardWayland_h_

#include "nsIClipboard.h"
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <nsTArray.h>

class nsRetrievalContextWayland : public nsRetrievalContext
{
public:
    nsRetrievalContextWayland();

    NS_IMETHOD HasDataMatchingFlavors(const char** aFlavorList,
                                      uint32_t aLength,
                                      int32_t aWhichClipboard,
                                      bool *_retval) override;
    NS_IMETHOD GetClipboardContent(const char* aMimeType,
                                   int32_t aWhichClipboard,
                                   nsIInputStream** aResult,
                                   uint32_t* aContentLength) override;

    void SetDataOffer(wl_data_offer *aDataOffer);
    void AddMIMEType(const char *aMimeType);
    bool HasMIMEType(const char *aMimeType);
    // Our version of gtk_selection_data_targets_include_text()
    bool HasMIMETypeText(void);
    void ResetMIMETypeList(void);
    void ConfigureKeyboard(wl_seat_capability caps);

    void InitDataDeviceManager(wl_registry *registry, uint32_t id, uint32_t version);
    void InitSeat(wl_registry *registry, uint32_t id, uint32_t version, void *data);
private:
    virtual ~nsRetrievalContextWayland() override;

    bool                    mInitialized;
    wl_display             *mDisplay;
    wl_seat                *mSeat;
    wl_data_device_manager *mDataDeviceManager;
    wl_data_offer          *mDataOffer;
    wl_keyboard            *mKeyboard;
    nsTArray<char*>         mMIMETypes;
    gchar                  *mTextPlainLocale;
};

#endif /* __nsClipboardWayland_h_ */
