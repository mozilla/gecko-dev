/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "nsClipboardWayland.h"
#include "nsIStorageStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsSupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsIServiceManager.h"
#include "nsImageToPixbuf.h"
#include "nsStringStream.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"

#include "imgIContainer.h"

#include <gtk/gtk.h>
#include <poll.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <errno.h>

void
nsRetrievalContextWayland::ResetMIMETypeList(void)
{
  int length = mMIMETypes.Length();
  for (int i = 0; i < length; i++) {
      free(mMIMETypes[i]);
  }
  mMIMETypes.Clear();
}

void
nsRetrievalContextWayland::AddMIMEType(const char *aMimeType)
{
    mMIMETypes.AppendElement(strdup(aMimeType));
}

bool
nsRetrievalContextWayland::HasMIMEType(const char *aMimeType)
{
    int length = mMIMETypes.Length();
    for (int i = 0; i < length; i++) {
        if(strcmp(mMIMETypes[i], aMimeType) == 0)
            return true;
    }
    return false;
}

bool
nsRetrievalContextWayland::HasMIMETypeText(void)
{
    // Taken from gtk_targets_include_text()
    int length = mMIMETypes.Length();
    for (int i = 0; i < length; i++) {
        if(strcmp(mMIMETypes[i], "UTF8_STRING") == 0 ||
           strcmp(mMIMETypes[i], "TEXT") == 0 ||
           strcmp(mMIMETypes[i], "COMPOUND_TEXT") == 0 ||
           strcmp(mMIMETypes[i], "text/plain") == 0 ||
           strcmp(mMIMETypes[i], "text/plain;charset=utf-8") == 0 ||
           strcmp(mMIMETypes[i], "mTextPlainLocale") == 0)
        {
            return true;
        }
    }
    return false;
}

void
nsRetrievalContextWayland::SetDataOffer(wl_data_offer *aDataOffer)
{
    if(mDataOffer) {
        wl_data_offer_destroy(mDataOffer);
    }
    mDataOffer = aDataOffer;
}

static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);
    context->SetDataOffer(offer);
}

static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *wl_data_offer,
                  const char           *type)
{
  nsRetrievalContextWayland *context =
      static_cast<nsRetrievalContextWayland*>(data);
  context->AddMIMEType(type);
}

static void
data_offer_source_actions(void *data,
                          struct wl_data_offer *wl_data_offer,
                          uint32_t source_actions)
{
}

static void
data_offer_action(void *data,
                  struct wl_data_offer *wl_data_offer,
                  uint32_t dnd_action)
{
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_offer,
    data_offer_source_actions,
    data_offer_action
};

static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *offer)
{
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);

    // We have a new fresh clipboard content
    context->ResetMIMETypeList();
    wl_data_offer_add_listener (offer, &data_offer_listener, data);
}

static void
data_device_enter (void                  *data,
                   struct wl_data_device *data_device,
                   uint32_t               time,
                   struct wl_surface     *surface,
                   int32_t                x,
                   int32_t                y,
                   struct wl_data_offer  *offer)
{
}

static void
data_device_leave (void                  *data,
                   struct wl_data_device *data_device)
{
}

static void
data_device_motion (void                  *data,
                    struct wl_data_device *data_device,
                    uint32_t               time,
                    int32_t                x,
                    int32_t                y)
{
}

static void
data_device_drop (void                  *data,
                  struct wl_data_device *data_device)
{
}

static const struct wl_data_device_listener data_device_listener = {
    data_device_data_offer,
    data_device_enter,
    data_device_leave,
    data_device_motion,
    data_device_drop,
    data_device_selection
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface,
                      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface)
{
    // We lost focus so our clipboard data are outdated
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);

    context->ResetMIMETypeList();
    context->SetDataOffer(nullptr);
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                    uint32_t serial, uint32_t time, uint32_t key,
                    uint32_t state)
{
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, uint32_t mods_depressed,
                          uint32_t mods_latched, uint32_t mods_locked,
                          uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
};

void
nsRetrievalContextWayland::ConfigureKeyboard(wl_seat_capability caps)
{
  if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
      mKeyboard = wl_seat_get_keyboard(mSeat);
      wl_keyboard_add_listener(mKeyboard, &keyboard_listener, this);
  } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
      wl_keyboard_destroy(mKeyboard);
      mKeyboard = nullptr;
  }
}

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         unsigned int caps)
{
    nsRetrievalContextWayland *context =
        static_cast<nsRetrievalContextWayland*>(data);
    context->ConfigureKeyboard((wl_seat_capability)caps);
}

static const struct wl_seat_listener seat_listener = {
      seat_handle_capabilities,
};

void
nsRetrievalContextWayland::InitDataDeviceManager(wl_registry *registry,
                                                 uint32_t id,
                                                 uint32_t version)
{
  int data_device_manager_version = MIN (version, 3);
  mDataDeviceManager = (wl_data_device_manager *)wl_registry_bind(registry, id,
      &wl_data_device_manager_interface, data_device_manager_version);
}

void nsRetrievalContextWayland::InitSeat(wl_registry *registry,
                                         uint32_t id, uint32_t version,
                                         void *data)
{
  mSeat = (wl_seat*)wl_registry_bind(registry, id, &wl_seat_interface, 1);
  wl_seat_add_listener(mSeat, &seat_listener, data);
}

static void
gdk_registry_handle_global(void               *data,
                           struct wl_registry *registry,
                           uint32_t            id,
                           const char         *interface,
                           uint32_t            version)
{
  nsRetrievalContextWayland *context =
      static_cast<nsRetrievalContextWayland*>(data);

  if (strcmp (interface, "wl_data_device_manager") == 0) {
    context->InitDataDeviceManager(registry, id, version);
  } else if (strcmp(interface, "wl_seat") == 0) {
    context->InitSeat(registry, id, version, data);
  }
}

static void
gdk_registry_handle_global_remove(void               *data,
                                 struct wl_registry *registry,
                                 uint32_t            id)
{
}

static const struct wl_registry_listener clipboard_registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

nsRetrievalContextWayland::nsRetrievalContextWayland(void)
  : mInitialized(false),
    mDataDeviceManager(nullptr),
    mDataOffer(nullptr)
{
    const gchar* charset;
    g_get_charset(&charset);
    mTextPlainLocale = g_strdup_printf("text/plain;charset=%s", charset);

    mDisplay = gdk_wayland_display_get_wl_display(gdk_display_get_default());
    wl_registry_add_listener(wl_display_get_registry(mDisplay),
                             &clipboard_registry_listener, this);
    wl_display_roundtrip(mDisplay);
    wl_display_roundtrip(mDisplay);

    // We don't have Wayland support here so just give up
    if (!mDataDeviceManager || !mSeat)
        return;

    wl_data_device *dataDevice =
        wl_data_device_manager_get_data_device(mDataDeviceManager, mSeat);
    wl_data_device_add_listener(dataDevice, &data_device_listener, this);
    // We have to call wl_display_roundtrip() twice otherwise data_offer_listener
    // may not be processed because it's called from data_device_data_offer
    // callback.
    wl_display_roundtrip(mDisplay);
    wl_display_roundtrip(mDisplay);

    mInitialized = true;
}

nsRetrievalContextWayland::~nsRetrievalContextWayland(void)
{
    g_free(mTextPlainLocale);
}

NS_IMETHODIMP
nsRetrievalContextWayland::HasDataMatchingFlavors(const char** aFlavorList,
    uint32_t aLength, int32_t aWhichClipboard, bool *_retval)
{
    if (!aFlavorList || !_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = false;

    // Walk through the provided types and try to match it to a
    // provided type.
    for (uint32_t i = 0; i < aLength; i++) {
        // We special case text/unicode here.
        if (!strcmp(aFlavorList[i], kUnicodeMime) &&
            HasMIMETypeText()) {
            *_retval = true;
            break;
        }
        if (HasMIMEType(aFlavorList[i])) {
            *_retval = true;
            break;
        }
        // X clipboard supports image/jpeg, but we want to emulate support
        // for image/jpg as well
        if (!strcmp(aFlavorList[i], kJPGImageMime) &&
            HasMIMEType(kJPEGImageMime)) {
            *_retval = true;
            break;
        }
    }

    return NS_OK;
}

nsresult
nsRetrievalContextWayland::GetClipboardContent(const char* aMimeType,
                                               int32_t aWhichClipboard,
                                               nsIInputStream** aResult,
                                               uint32_t* aContentLength)
{
    NS_ASSERTION(mDataOffer, "Requested data without valid data offer!");

    if (!mDataOffer) {
        // TODO
        // Something went wrong. We're requested to provide clipboard data
        // but we haven't got any from wayland. Looks like rhbz#1455915.
        // Return NS_ERROR_FAILURE to avoid crash.
        return NS_ERROR_FAILURE;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
        return NS_ERROR_FAILURE;

    wl_data_offer_receive(mDataOffer, aMimeType, pipe_fd[1]);
    close(pipe_fd[1]);
    wl_display_flush(mDisplay);

    nsresult rv;
    nsCOMPtr<nsIStorageStream> storageStream;
    nsCOMPtr<nsIBinaryOutputStream> stream;
    int length;

    struct pollfd fds;
    fds.fd = pipe_fd[0];
    fds.events = POLLIN;

    // Choose some reasonable timeout here
    int ret = poll(&fds, 1, kClipboardTimeout*1000);
    if (!ret || ret == -1) {
        close(pipe_fd[0]);
        return NS_ERROR_FAILURE;
    }

    #define BUFFER_SIZE 4096

    NS_NewStorageStream(BUFFER_SIZE, UINT32_MAX, getter_AddRefs(storageStream));
    nsCOMPtr<nsIOutputStream> outputStream;
    rv = storageStream->GetOutputStream(0, getter_AddRefs(outputStream));
    if (NS_FAILED(rv)) {
        close(pipe_fd[0]);
        return NS_ERROR_FAILURE;
    }

    do {
        char buffer[BUFFER_SIZE];
        length = read(pipe_fd[0], buffer, sizeof(buffer));
        if (length == 0 || length == -1)
            break;

        uint32_t ret;
        rv = outputStream->Write(buffer, length, &ret);
    } while(NS_SUCCEEDED(rv) && length == BUFFER_SIZE);

    outputStream->Close();
    close(pipe_fd[0]);

    rv = storageStream->GetLength(aContentLength);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = storageStream->NewInputStream(0, aResult);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}
