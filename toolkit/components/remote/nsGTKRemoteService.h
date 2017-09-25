/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsGTKRemoteService_h__
#define __nsGTKRemoteService_h__

#if defined(MOZ_WAYLAND) && defined(MOZ_ENABLE_DBUS)
#define ENABLE_REMOTE_DBUS 1
#endif

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "nsInterfaceHashtable.h"
#include "nsXRemoteService.h"
#include "mozilla/Attributes.h"
#ifdef ENABLE_REMOTE_DBUS
#include "mozilla/ipc/DBusConnectionRefPtr.h"
#endif


class nsGTKRemoteService final : public nsXRemoteService
{
public:
  // We will be a static singleton, so don't use the ordinary methods.
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREMOTESERVICE


  nsGTKRemoteService() 
    : mServerWindow(nullptr)
#ifdef ENABLE_REMOTE_DBUS
    , mConnection(nullptr)
#endif    
    { }

#ifdef ENABLE_REMOTE_DBUS
    DBusHandlerResult HandleDBusMessage(DBusConnection *aConnection, DBusMessage *msg);
    void UnregisterDBusInterface(DBusConnection *aConnection);
#endif

private:
  ~nsGTKRemoteService() { }

  void HandleCommandsFor(GtkWidget* aWidget,
                         nsIWeakReference* aWindow);


  static gboolean HandlePropertyChange(GtkWidget *widget,
                                       GdkEventProperty *event,
                                       nsIWeakReference* aThis);


  virtual void SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                              uint32_t aTimestamp) override;

#ifdef ENABLE_REMOTE_DBUS
  void OpenURL(const char *aCommandLine, int aLength);

  DBusHandlerResult OpenURL(DBusMessage *msg);
  DBusHandlerResult Introspect(DBusMessage *msg);

  bool Connect(const char* aAppName, const char* aProfileName);
#endif

  nsInterfaceHashtable<nsPtrHashKey<GtkWidget>, nsIWeakReference> mWindows;
  GtkWidget* mServerWindow;
  bool       mIsX11Display;
#ifdef ENABLE_REMOTE_DBUS
  RefPtr<DBusConnection> mConnection;
#endif
};

#endif // __nsGTKRemoteService_h__
