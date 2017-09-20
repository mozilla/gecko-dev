/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "nsClipboardX11.h"
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

// For manipulation of the X event queue
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include "X11UndefineNone.h"

using namespace mozilla;

static GdkFilterReturn
selection_request_filter(GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
    XEvent *xevent = static_cast<XEvent*>(gdk_xevent);
    if (xevent->xany.type == SelectionRequest) {
        if (xevent->xselectionrequest.requestor == X11None)
            return GDK_FILTER_REMOVE;

        GdkDisplay *display = gdk_x11_lookup_xdisplay(
                xevent->xselectionrequest.display);
        if (!display)
            return GDK_FILTER_REMOVE;

        GdkWindow *window = gdk_x11_window_foreign_new_for_display(display,
                xevent->xselectionrequest.requestor);
        if (!window)
            return GDK_FILTER_REMOVE;

        g_object_unref(window);
    }
    return GDK_FILTER_CONTINUE;
}

nsRetrievalContextX11::nsRetrievalContextX11(void)
: mState(INITIAL),
  mData(nullptr)
{
    // A custom event filter to workaround attempting to dereference a null
    // selection requestor in GTK3 versions before 3.11.3. See bug 1178799.
#if (MOZ_WIDGET_GTK == 3) && defined(MOZ_X11)
    if (gtk_check_version(3, 11, 3))
        gdk_window_add_filter(nullptr, selection_request_filter, nullptr);
#endif
}

nsRetrievalContextX11::~nsRetrievalContextX11(void)
{
    gdk_window_remove_filter(nullptr, selection_request_filter, nullptr);
}

static void
DispatchSelectionNotifyEvent(GtkWidget *widget, XEvent *xevent)
{
    GdkEvent event;
    event.selection.type = GDK_SELECTION_NOTIFY;
    event.selection.window = gtk_widget_get_window(widget);
    event.selection.selection = gdk_x11_xatom_to_atom(xevent->xselection.selection);
    event.selection.target = gdk_x11_xatom_to_atom(xevent->xselection.target);
    event.selection.property = gdk_x11_xatom_to_atom(xevent->xselection.property);
    event.selection.time = xevent->xselection.time;

    gtk_widget_event(widget, &event);
}

static void
DispatchPropertyNotifyEvent(GtkWidget *widget, XEvent *xevent)
{
    GdkWindow *window = gtk_widget_get_window(widget);
    if ((gdk_window_get_events(window)) & GDK_PROPERTY_CHANGE_MASK) {
        GdkEvent event;
        event.property.type = GDK_PROPERTY_NOTIFY;
        event.property.window = window;
        event.property.atom = gdk_x11_xatom_to_atom(xevent->xproperty.atom);
        event.property.time = xevent->xproperty.time;
        event.property.state = xevent->xproperty.state;

        gtk_widget_event(widget, &event);
    }
}

struct checkEventContext
{
    GtkWidget *cbWidget;
    Atom       selAtom;
};

static Bool
checkEventProc(Display *display, XEvent *event, XPointer arg)
{
    checkEventContext *context = (checkEventContext *) arg;

    if (event->xany.type == SelectionNotify ||
        (event->xany.type == PropertyNotify &&
         event->xproperty.atom == context->selAtom)) {

        GdkWindow *cbWindow =
            gdk_x11_window_lookup_for_display(gdk_x11_lookup_xdisplay(display),
                                              event->xany.window);
        if (cbWindow) {
            GtkWidget *cbWidget = nullptr;
            gdk_window_get_user_data(cbWindow, (gpointer *)&cbWidget);
            if (cbWidget && GTK_IS_WIDGET(cbWidget)) {
                context->cbWidget = cbWidget;
                return True;
            }
        }
    }

    return False;
}

void *
nsRetrievalContextX11::Wait()
{
    if (mState == COMPLETED) { // the request completed synchronously
        void *data = mData;
        mData = nullptr;
        return data;
    }

    GdkDisplay *gdkDisplay = gdk_display_get_default();
    if (GDK_IS_X11_DISPLAY(gdkDisplay)) {
        Display *xDisplay = GDK_DISPLAY_XDISPLAY(gdkDisplay);
        checkEventContext context;
        context.cbWidget = nullptr;
        context.selAtom = gdk_x11_atom_to_xatom(gdk_atom_intern("GDK_SELECTION",
                                                                FALSE));

        // Send X events which are relevant to the ongoing selection retrieval
        // to the clipboard widget.  Wait until either the operation completes, or
        // we hit our timeout.  All other X events remain queued.

        int select_result;

        int cnumber = ConnectionNumber(xDisplay);
        fd_set select_set;
        FD_ZERO(&select_set);
        FD_SET(cnumber, &select_set);
        ++cnumber;
        TimeStamp start = TimeStamp::Now();

        do {
            XEvent xevent;

            while (XCheckIfEvent(xDisplay, &xevent, checkEventProc,
                                 (XPointer) &context)) {

                if (xevent.xany.type == SelectionNotify)
                    DispatchSelectionNotifyEvent(context.cbWidget, &xevent);
                else
                    DispatchPropertyNotifyEvent(context.cbWidget, &xevent);

                if (mState == COMPLETED) {
                    void *data = mData;
                    mData = nullptr;
                    return data;
                }
            }

            TimeStamp now = TimeStamp::Now();
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = std::max<int32_t>(0,
                kClipboardTimeout - (now - start).ToMicroseconds());
            select_result = select(cnumber, &select_set, nullptr, nullptr, &tv);
        } while (select_result == 1 ||
                 (select_result == -1 && errno == EINTR));
    }
#ifdef DEBUG_CLIPBOARD
    printf("exceeded clipboard timeout\n");
#endif
    mState = TIMED_OUT;
    return nullptr;
}

static void
clipboard_contents_received(GtkClipboard     *clipboard,
                            GtkSelectionData *selection_data,
                            gpointer          data)
{
    nsRetrievalContextX11 *context = static_cast<nsRetrievalContextX11*>(data);
    context->Complete(selection_data);
}

GtkSelectionData*
nsRetrievalContextX11::WaitForContents(GtkClipboard *clipboard, const char *aMimeType)
{
    mState = INITIAL;
    NS_ASSERTION(!mData, "Leaking clipboard content!");

    gtk_clipboard_request_contents(clipboard,
                                   gdk_atom_intern(aMimeType, FALSE),
                                   clipboard_contents_received,
                                   this);
    return static_cast<GtkSelectionData*>(Wait());
}

NS_IMETHODIMP
nsRetrievalContextX11::HasDataMatchingFlavors(const char** aFlavorList,
                                              uint32_t aLength,
                                              int32_t aWhichClipboard,
                                              bool *_retval)
{
    if (!aFlavorList || !_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = false;

    GtkClipboard *clipboard = 
        gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));

    GtkSelectionData *selection_data =
        WaitForContents(clipboard, "TARGETS");
    if (!selection_data)
        return NS_OK;

    gint n_targets = 0;
    GdkAtom *targets = nullptr;

    if (!gtk_selection_data_get_targets(selection_data,
                                        &targets, &n_targets) ||
        !n_targets)
        return NS_OK;

    // Walk through the provided types and try to match it to a
    // provided type.
    for (uint32_t i = 0; i < aLength && !*_retval; i++) {
        // We special case text/unicode here.
        if (!strcmp(aFlavorList[i], kUnicodeMime) &&
            gtk_selection_data_targets_include_text(selection_data)) {
            *_retval = true;
            break;
        }

        for (int32_t j = 0; j < n_targets; j++) {
            gchar *atom_name = gdk_atom_name(targets[j]);
            if (!atom_name)
                continue;

            if (!strcmp(atom_name, aFlavorList[i]))
                *_retval = true;

            // X clipboard supports image/jpeg, but we want to emulate support
            // for image/jpg as well
            if (!strcmp(aFlavorList[i], kJPGImageMime) && !strcmp(atom_name, kJPEGImageMime))
                *_retval = true;

            g_free(atom_name);

            if (*_retval)
                break;
        }
    }
    gtk_selection_data_free(selection_data);
    g_free(targets);

    return NS_OK;
}

nsresult
nsRetrievalContextX11::GetClipboardContent(const char* aMimeType,
                                           int32_t aWhichClipboard,
                                           nsIInputStream** aResult,
                                           uint32_t* aContentLength)
{
    GtkClipboard *clipboard;
    clipboard = gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));

    GtkSelectionData *selectionData = WaitForContents(clipboard, aMimeType);
    if (!selectionData)
        return NS_ERROR_FAILURE;

    *aContentLength = gtk_selection_data_get_length(selectionData);
    NS_NewByteInputStream(aResult,
                          (const char*)gtk_selection_data_get_data(selectionData),
                          *aContentLength, NS_ASSIGNMENT_COPY);
    gtk_selection_data_free(selectionData);
    return NS_OK;
}
