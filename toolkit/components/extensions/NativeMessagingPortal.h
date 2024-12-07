/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_NativeMessagingPortal_h
#define mozilla_extensions_NativeMessagingPortal_h

#include "nsINativeMessagingPortal.h"

#include <gio/gio.h>

#include "mozilla/GRefPtr.h"
#include "mozilla/UniquePtr.h"

#include <deque>
#include <unordered_map>

namespace mozilla::extensions {

enum class SessionState { Active, Closing, Error };

class NativeMessagingPortal : public nsINativeMessagingPortal {
 public:
  NS_DECL_NSINATIVEMESSAGINGPORTAL
  NS_DECL_ISUPPORTS

  static already_AddRefed<NativeMessagingPortal> GetSingleton();

 private:
  NativeMessagingPortal();
  virtual ~NativeMessagingPortal();

  RefPtr<GDBusProxy> mProxy;
  bool mInitialized = false;
  RefPtr<GCancellable> mCancellable;

  struct DelayedCall;
  std::deque<UniquePtr<DelayedCall>> mPending;

  using SessionsMap = std::unordered_map<std::string, SessionState>;
  SessionsMap mSessions;

  // Callbacks
  static void OnProxyReady(GObject* source, GAsyncResult* result,
                           gpointer user_data);
  void MaybeDelayedIsAvailable(dom::Promise&, GVariant*);
  void MaybeDelayedCreateSession(dom::Promise&, GVariant*);
  static void OnCreateSessionDone(GObject* source, GAsyncResult* result,
                                  gpointer user_data);
  static void OnCloseSessionProxyReady(GObject* source, GAsyncResult* result,
                                       gpointer user_data);
  static void OnCloseSessionDone(GObject* source, GAsyncResult* result,
                                 gpointer user_data);
  static void OnSessionClosedSignal(GDBusConnection* bus,
                                    const gchar* sender_name,
                                    const gchar* object_path,
                                    const gchar* interface_name,
                                    const gchar* signal_name,
                                    GVariant* parameters, gpointer user_data);
  static void OnGetManifestDone(GObject* source, GAsyncResult* result,
                                gpointer user_data);
  static void OnStartDone(GObject* source, GAsyncResult* result,
                          gpointer user_data);
  static void OnStartRequestResponseSignal(
      GDBusConnection* bus, const gchar* sender_name, const gchar* object_path,
      const gchar* interface_name, const gchar* signal_name,
      GVariant* parameters, gpointer user_data);
  static void OnGetPipesDone(GObject* source, GAsyncResult* result,
                             gpointer user_data);
};

}  // namespace mozilla::extensions

#endif  // mozilla_extensions_NativeMessagingPortal_h
