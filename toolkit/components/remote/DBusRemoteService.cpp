/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=8:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DBusRemoteService.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsIWeakReference.h"
#include "nsIWidget.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"
#include "nsPrintfCString.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "mozilla/ipc/DBusMessageRefPtr.h"
#include "mozilla/ipc/DBusPendingCallRefPtr.h"

#define MOZILLA_TARGET              "org.mozilla"
#define MOZILLA_REMOTE_OBJECT       "/org/mozilla/Firefox/Remote"

const char* introspect_xml =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\";>\n"
"<node>\n"
"	<interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"		<method name=\"Introspect\">\n"
"			<arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"		</method>\n"
"	</interface>\n"
"	<interface name=\"org.mozilla.firefox\">\n"
"		<method name=\"Open\">\n"
"			<arg name=\"url\" direction=\"out\" type=\"s\"/>\n"
"		</method>\n"
"	</interface>\n"
"</node>\n";

static void unregister(DBusConnection *connection, void *user_data)
{
}

static DBusHandlerResult introspect(DBusConnection *conn, DBusMessage *msg)
{
	DBusMessage *reply;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_append_args(reply,
			DBUS_TYPE_STRING, &introspect_xml,
			DBUS_TYPE_INVALID);

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult Open(DBusConnection *conn, DBusMessage *msg)
{
	DBusMessage *reply;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_append_args(reply,
			DBUS_TYPE_STRING, &introspect_xml,
			DBUS_TYPE_INVALID);

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult message_handler(DBusConnection *conn,
				DBusMessage *msg, void *user_data)
{
	const char *method = dbus_message_get_member(msg);
	const char *iface = dbus_message_get_interface(msg);

	if ((strcmp("Introspect", method) == 0) &&
		 (strcmp("org.freedesktop.DBus.Introspectable", iface) == 0)) {
		return introspect(conn, msg);
  }

	if ((strcmp("Open", method) == 0) && 
    (strcmp("org.mozilla.firefox", iface) == 0)) {
    return Open(conn, msg);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusObjectPathVTable remoteHandlersTable = {
	.unregister_function	= unregister,
	.message_function	= message_handler,
};

bool
DBusRemoteService::Connect(const char* aAppName, const char* aProfileName)
{
  mConnection = already_AddRefed<DBusConnection>(
    dbus_bus_get(DBUS_BUS_SESSION, nullptr));
  if (!mConnection)
    return NS_ERROR_FAILURE;

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
    return NS_ERROR_FAILURE;
  }

  if (!dbus_connection_register_object_path(mConnection, MOZILLA_REMOTE_OBJECT,
                                            &remoteHandlersTable, nullptr)) {
    dbus_connection_unref(mConnection);
    mConnection = nullptr;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

DBusRemoteService::Disconnect()
{
  if (mConnection) {
    dbus_connection_unref(mConnection);
    mConnection = nullptr;
  }
}
