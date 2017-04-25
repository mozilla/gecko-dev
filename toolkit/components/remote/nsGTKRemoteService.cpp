/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGTKRemoteService.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsIWeakReference.h"
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"
#include "nsPrintfCString.h"

#include "nsCOMPtr.h"

#include "nsGTKToolkit.h"

#ifdef ENABLE_REMOTE_DBUS
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#endif

NS_IMPL_ISUPPORTS(nsGTKRemoteService,
                  nsIRemoteService,
                  nsIObserver)

NS_IMETHODIMP
nsGTKRemoteService::Startup(const char* aAppName, const char* aProfileName)
{
  NS_ASSERTION(aAppName, "Don't pass a null appname!");
  sRemoteImplementation = this;

  if (mServerWindow) return NS_ERROR_ALREADY_INITIALIZED;

  mServerWindow = gtk_invisible_new();
  gtk_widget_realize(mServerWindow);

  mIsX11Display = GDK_IS_X11_DISPLAY(gdk_display_get_default());
#ifdef ENABLE_REMOTE_DBUS
  if (!mIsX11Display) {
    if (!Connect(aAppName, aProfileName))
      return NS_ERROR_FAILURE;
  } else
#endif
  {
    XRemoteBaseStartup(aAppName, aProfileName);

    HandleCommandsFor(mServerWindow, nullptr);

    for (auto iter = mWindows.Iter(); !iter.Done(); iter.Next()) {
      HandleCommandsFor(iter.Key(), iter.UserData());
    }
  }

  return NS_OK;
}

static nsIWidget* GetMainWidget(nsPIDOMWindowInner* aWindow)
{
  // get the native window for this instance
  nsCOMPtr<nsIBaseWindow> baseWindow
    (do_QueryInterface(aWindow->GetDocShell()));
  NS_ENSURE_TRUE(baseWindow, nullptr);

  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  return mainWidget;
}

NS_IMETHODIMP
nsGTKRemoteService::RegisterWindow(mozIDOMWindow* aWindow)
{
  nsIWidget* mainWidget = GetMainWidget(nsPIDOMWindowInner::From(aWindow));
  NS_ENSURE_TRUE(mainWidget, NS_ERROR_FAILURE);

  GtkWidget* widget =
    (GtkWidget*) mainWidget->GetNativeData(NS_NATIVE_SHELLWIDGET);
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWeakReference> weak = do_GetWeakReference(aWindow);
  NS_ENSURE_TRUE(weak, NS_ERROR_FAILURE);

  mWindows.Put(widget, weak);

  // If Startup() has already been called, immediately register this window.
  if (mServerWindow && mIsX11Display) {
    HandleCommandsFor(widget, weak);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGTKRemoteService::Shutdown()
{
  if (!mServerWindow)
    return NS_ERROR_NOT_INITIALIZED;

  gtk_widget_destroy(mServerWindow);
  mServerWindow = nullptr;
  return NS_OK;
}

// Set desktop startup ID to the passed ID, if there is one, so that any created
// windows get created with the right window manager metadata, and any windows
// that get new tabs and are activated also get the right WM metadata.
// The timestamp will be used if there is no desktop startup ID, or if we're
// raising an existing window rather than showing a new window for the first time.
void
nsGTKRemoteService::SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                                   uint32_t aTimestamp) {
  nsGTKToolkit* toolkit = nsGTKToolkit::GetToolkit();
  if (!toolkit)
    return;

  if (!aDesktopStartupID.IsEmpty()) {
    toolkit->SetDesktopStartupID(aDesktopStartupID);
  }

  toolkit->SetFocusTimestamp(aTimestamp);
}


void
nsGTKRemoteService::HandleCommandsFor(GtkWidget* widget,
                                      nsIWeakReference* aWindow)
{
  g_signal_connect(G_OBJECT(widget), "property_notify_event",
                   G_CALLBACK(HandlePropertyChange), aWindow);

  gtk_widget_add_events(widget, GDK_PROPERTY_CHANGE_MASK);

#if (MOZ_WIDGET_GTK == 2)
  Window window = GDK_WINDOW_XWINDOW(widget->window);
#else
  Window window = gdk_x11_window_get_xid(gtk_widget_get_window(widget));
#endif
  nsXRemoteService::HandleCommandsFor(window);

}

gboolean
nsGTKRemoteService::HandlePropertyChange(GtkWidget *aWidget,
                                         GdkEventProperty *pevent,
                                         nsIWeakReference *aThis)
{
  if (pevent->state == GDK_PROPERTY_NEW_VALUE) {
    Atom changedAtom = gdk_x11_atom_to_xatom(pevent->atom);

#if (MOZ_WIDGET_GTK == 2)
    XID window = GDK_WINDOW_XWINDOW(pevent->window);
#else
    XID window = gdk_x11_window_get_xid(gtk_widget_get_window(aWidget));
#endif
    return HandleNewProperty(window,
                             GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                             pevent->time, changedAtom, aThis);
  }
  return FALSE;
}

#ifdef ENABLE_REMOTE_DBUS

void nsGTKRemoteService::OpenURL(const char *aCommandLine)
{
  char* buffer = strdup(aCommandLine);
  HandleCommandLine(buffer, nullptr, 0);
  free(buffer);
}

#define MOZILLA_REMOTE_OBJECT       "/org/mozilla/Firefox/Remote"

const char* introspect_xml =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\";>\n"
"<node>\n"
" <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"   <method name=\"Introspect\">\n"
"     <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"   </method>\n"
" </interface>\n"
" <interface name=\"org.mozilla.firefox\">\n"
"   <method name=\"OpenURL\">\n"
"     <arg name=\"url\" direction=\"in\" type=\"s\"/>\n"
"   </method>\n"
" </interface>\n"
"</node>\n";

DBusHandlerResult
nsGTKRemoteService::Introspect(DBusMessage *msg)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(msg);
  if (!reply)
    return DBUS_HANDLER_RESULT_NEED_MEMORY;

  dbus_message_append_args(reply,
      DBUS_TYPE_STRING, &introspect_xml,
      DBUS_TYPE_INVALID);

  dbus_connection_send(mConnection, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult
nsGTKRemoteService::OpenURL(DBusMessage *msg)
{
  DBusMessage *reply = nullptr;
  const char  *commandLine;

  if (!dbus_message_get_args(msg, nullptr, DBUS_TYPE_STRING, &commandLine,
                             DBUS_TYPE_INVALID)) {
    reply = dbus_message_new_error(msg, "org.mozilla.firefox.Error",
                                   "Wrong argument");
  } else {
    OpenURL(commandLine);
    reply = dbus_message_new_method_return(msg);
  }
  
  dbus_connection_send(mConnection, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;  
}

DBusHandlerResult
nsGTKRemoteService::HandleDBusMessage(DBusConnection *aConnection, DBusMessage *msg)
{
  NS_ASSERTION(mConnection == aConnection, "Wrong D-Bus connection.");
  
  const char *method = dbus_message_get_member(msg);
  const char *iface = dbus_message_get_interface(msg);

  if ((strcmp("Introspect", method) == 0) &&
     (strcmp("org.freedesktop.DBus.Introspectable", iface) == 0)) {
    return Introspect(msg);
  }

  if ((strcmp("OpenURL", method) == 0) && 
    (strcmp("org.mozilla.firefox", iface) == 0)) {
    return OpenURL(msg);
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void
nsGTKRemoteService::UnregisterDBusInterface(DBusConnection *aConnection)
{
  NS_ASSERTION(mConnection == aConnection, "Wrong D-Bus connection.");
  // Not implemented
}

static DBusHandlerResult
message_handler(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
  auto interface = static_cast<nsGTKRemoteService*>(user_data);
  return interface->HandleDBusMessage(conn, msg);
}

static void
unregister(DBusConnection *conn, void *user_data)
{
  auto interface = static_cast<nsGTKRemoteService*>(user_data);
  interface->UnregisterDBusInterface(conn);
}

static DBusObjectPathVTable remoteHandlersTable = {
  .unregister_function  = unregister,
  .message_function = message_handler,
};

bool
nsGTKRemoteService::Connect(const char* aAppName, const char* aProfileName)
{
  mConnection = already_AddRefed<DBusConnection>(
    dbus_bus_get(DBUS_BUS_SESSION, nullptr));
  if (!mConnection)
    return false;

  dbus_connection_set_exit_on_disconnect(mConnection, false);

  nsAutoCString interfaceName;
  interfaceName = nsPrintfCString("org.mozilla.%s.%s", aAppName, aProfileName);

  int ret = dbus_bus_request_name(mConnection, interfaceName.get(),
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, nullptr);
  // The interface is already owned - there is another application/profile
  // instance already running.
  if (ret == -1) {
    dbus_connection_unref(mConnection);
    mConnection = nullptr;
    return false;
  }

  if (!dbus_connection_register_object_path(mConnection, MOZILLA_REMOTE_OBJECT,
                                            &remoteHandlersTable, this)) {
    dbus_connection_unref(mConnection);
    mConnection = nullptr;
    return false;
  }

  return true;
}

void
nsGTKRemoteService::Disconnect()
{
  if (mConnection) {
    dbus_connection_unref(mConnection);
    mConnection = nullptr;
  }
}
#endif

// {C0773E90-5799-4eff-AD03-3EBCD85624AC}
#define NS_REMOTESERVICE_CID \
  { 0xc0773e90, 0x5799, 0x4eff, { 0xad, 0x3, 0x3e, 0xbc, 0xd8, 0x56, 0x24, 0xac } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsGTKRemoteService)
NS_DEFINE_NAMED_CID(NS_REMOTESERVICE_CID);

static const mozilla::Module::CIDEntry kRemoteCIDs[] = {
  { &kNS_REMOTESERVICE_CID, false, nullptr, nsGTKRemoteServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kRemoteContracts[] = {
  { "@mozilla.org/toolkit/remote-service;1", &kNS_REMOTESERVICE_CID },
  { nullptr }
};

static const mozilla::Module kRemoteModule = {
  mozilla::Module::kVersion,
  kRemoteCIDs,
  kRemoteContracts
};

NSMODULE_DEFN(RemoteServiceModule) = &kRemoteModule;
