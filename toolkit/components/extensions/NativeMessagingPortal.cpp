/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeMessagingPortal.h"

#include <gio/gunixfdlist.h>
#include <glib.h>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/Logging.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "mozilla/dom/Promise.h"

#include "prlink.h"

#include <string.h>

static mozilla::LazyLogModule gNativeMessagingPortalLog(
    "NativeMessagingPortal");

#ifdef MOZ_LOGGING
#  define LOG_NMP(...) \
    MOZ_LOG(gNativeMessagingPortalLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#  define LOG_NMP(args)
#endif

#define GET_FUNC(func, lib) \
  func##_fn = (decltype(func##_fn))PR_FindFunctionSymbol(lib, #func)

static gint _g_unix_fd_list_get(GUnixFDList* list, gint index_,
                                GError** error) {
  static PRLibrary* gioLib = nullptr;
  static bool gioInitialized = false;
  static gint (*g_unix_fd_list_get_fn)(GUnixFDList* list, gint index_,
                                       GError** error) = nullptr;

  if (!gioInitialized) {
    gioInitialized = true;
    gioLib = PR_LoadLibrary("libgio-2.0.so.0");
    if (!gioLib) {
      return -1;
    }
    GET_FUNC(g_unix_fd_list_get, gioLib);
  }

  if (!g_unix_fd_list_get_fn) {
    return -1;
  }

  return g_unix_fd_list_get_fn(list, index_, error);
}

namespace mozilla::extensions {

NS_IMPL_ISUPPORTS(NativeMessagingPortal, nsINativeMessagingPortal)

/* static */
already_AddRefed<NativeMessagingPortal> NativeMessagingPortal::GetSingleton() {
  static StaticRefPtr<NativeMessagingPortal> sInstance;

  if (MOZ_UNLIKELY(!sInstance)) {
    sInstance = new NativeMessagingPortal();
    ClearOnShutdown(&sInstance);
  }

  return do_AddRef(sInstance);
}

static void LogError(const char* aMethod, const GError& aError) {
  g_warning("%s error: %s", aMethod, aError.message);
}

static void RejectPromiseWithErrorMessage(dom::Promise& aPromise,
                                          const GError& aError) {
  aPromise.MaybeRejectWithOperationError(nsDependentCString(aError.message));
}

static nsresult GetPromise(JSContext* aCx, RefPtr<dom::Promise>& aPromise) {
  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }
  ErrorResult result;
  aPromise = dom::Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }
  return NS_OK;
}

struct CallbackData {
  explicit CallbackData(dom::Promise& aPromise,
                        const gchar* aSessionHandle = nullptr)
      : promise(&aPromise), sessionHandle(g_strdup(aSessionHandle)) {}
  RefPtr<dom::Promise> promise;
  GUniquePtr<gchar> sessionHandle;
  guint subscription_id = 0;
};

NativeMessagingPortal::NativeMessagingPortal() {
  LOG_NMP("NativeMessagingPortal::NativeMessagingPortal()");
  mCancellable = dont_AddRef(g_cancellable_new());
  g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr,
                           "org.freedesktop.portal.Desktop",
                           "/org/freedesktop/portal/desktop",
                           "org.freedesktop.portal.WebExtensions", mCancellable,
                           &NativeMessagingPortal::OnProxyReady, this);
}

NativeMessagingPortal::~NativeMessagingPortal() {
  LOG_NMP("NativeMessagingPortal::~NativeMessagingPortal()");

  g_cancellable_cancel(mCancellable);

  // Close all active sessions
  for (const auto& it : mSessions) {
    if (it.second != SessionState::Active) {
      continue;
    }
    GUniquePtr<GError> error;
    RefPtr<GDBusProxy> proxy = dont_AddRef(g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr,
        "org.freedesktop.portal.Desktop", it.first.c_str(),
        "org.freedesktop.portal.Session", nullptr, getter_Transfers(error)));
    if (!proxy) {
      LOG_NMP("failed to get a D-Bus proxy: %s", error->message);
      LogError(__func__, *error);
      continue;
    }
    RefPtr<GVariant> res = dont_AddRef(
        g_dbus_proxy_call_sync(proxy, "Close", nullptr, G_DBUS_CALL_FLAGS_NONE,
                               -1, nullptr, getter_Transfers(error)));
    if (!res) {
      LOG_NMP("failed to close session: %s", error->message);
      LogError(__func__, *error);
    }
  }
}

NS_IMETHODIMP
NativeMessagingPortal::ShouldUse(bool* aResult) {
  *aResult = widget::ShouldUsePortal(widget::PortalKind::NativeMessaging);
  LOG_NMP("will %sbe used", *aResult ? "" : "not ");
  return NS_OK;
}

struct NativeMessagingPortal::DelayedCall {
  using DelayedMethodCall = void (NativeMessagingPortal::*)(dom::Promise&,
                                                            GVariant*);

  DelayedCall(DelayedMethodCall aCallback, dom::Promise& aPromise,
              GVariant* aArgs = nullptr)
      : callback(aCallback), promise(&aPromise), args(aArgs) {
    LOG_NMP("NativeMessagingPortal::DelayedCall::DelayedCall()");
  }
  ~DelayedCall() {
    LOG_NMP("NativeMessagingPortal::DelayedCall::~DelayedCall()");
  }

  DelayedMethodCall callback;
  RefPtr<dom::Promise> promise;
  RefPtr<GVariant> args;
};

/* static */
void NativeMessagingPortal::OnProxyReady(GObject* source, GAsyncResult* result,
                                         gpointer user_data) {
  NativeMessagingPortal* self = static_cast<NativeMessagingPortal*>(user_data);
  GUniquePtr<GError> error;
  self->mProxy = dont_AddRef(
      g_dbus_proxy_new_for_bus_finish(result, getter_Transfers(error)));
  if (self->mProxy) {
    LOG_NMP("D-Bus proxy ready for name %s, path %s, interface %s",
            g_dbus_proxy_get_name(self->mProxy),
            g_dbus_proxy_get_object_path(self->mProxy),
            g_dbus_proxy_get_interface_name(self->mProxy));
  } else {
    LOG_NMP("failed to get a D-Bus proxy: %s", error->message);
    LogError(__func__, *error);
  }
  self->mInitialized = true;
  while (!self->mPending.empty()) {
    auto pending = std::move(self->mPending.front());
    self->mPending.pop_front();
    (self->*pending->callback)(*pending->promise, pending->args.get());
  }
}

NS_IMETHODIMP
NativeMessagingPortal::GetAvailable(JSContext* aCx, dom::Promise** aPromise) {
  RefPtr<dom::Promise> promise;
  MOZ_TRY(GetPromise(aCx, promise));

  if (mInitialized) {
    MaybeDelayedIsAvailable(*promise, nullptr);
  } else {
    auto delayed = MakeUnique<DelayedCall>(
        &NativeMessagingPortal::MaybeDelayedIsAvailable, *promise);
    mPending.push_back(std::move(delayed));
  }

  promise.forget(aPromise);
  return NS_OK;
}

void NativeMessagingPortal::MaybeDelayedIsAvailable(dom::Promise& aPromise,
                                                    GVariant* aArgs) {
  MOZ_ASSERT(!aArgs);

  bool available = false;
  if (mProxy) {
    RefPtr<GVariant> version =
        dont_AddRef(g_dbus_proxy_get_cached_property(mProxy, "version"));
    if (version) {
      if (g_variant_get_uint32(version) >= 1) {
        available = true;
      }
    }
  }

  LOG_NMP("is %savailable", available ? "" : "not ");
  aPromise.MaybeResolve(available);
}

NS_IMETHODIMP
NativeMessagingPortal::CreateSession(const nsACString& aApplication,
                                     JSContext* aCx, dom::Promise** aPromise) {
  RefPtr<dom::Promise> promise;
  MOZ_TRY(GetPromise(aCx, promise));

  // Creating a session requires passing a unique token that will be used as the
  // suffix for the session handle, and it should be a valid D-Bus object path
  // component (i.e. it contains only the characters "[A-Z][a-z][0-9]_", see
  // https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-marshaling-object-path
  // and
  // https://flatpak.github.io/xdg-desktop-portal/#gdbus-org.freedesktop.portal.Session).
  // The token should be unique and not guessable. To avoid clashes with calls
  // made from unrelated libraries, it is a good idea to use a per-library
  // prefix combined with a random number.
  // Here, we build the token by concatenating MOZ_APP_NAME (e.g. "firefox"),
  // with the name of the native application (sanitized to remove invalid
  // characters, see
  // https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests#native_messaging_manifests),
  // and a random number.
  const nsCString& application = PromiseFlatCString(aApplication);
  GUniquePtr<gchar> sanitizedApplicationName(g_strdup(application.get()));
  g_strdelimit(sanitizedApplicationName.get(), ".", '_');
  GUniquePtr<gchar> token(g_strdup_printf("%s_%s_%u", MOZ_APP_NAME,
                                          sanitizedApplicationName.get(),
                                          g_random_int()));
  RefPtr<GVariant> args = dont_AddRef(g_variant_new_string(token.get()));

  if (mInitialized) {
    MaybeDelayedCreateSession(*promise, args);
  } else {
    auto delayed = MakeUnique<DelayedCall>(
        &NativeMessagingPortal::MaybeDelayedCreateSession, *promise, args);
    mPending.push_back(std::move(delayed));
  }

  promise.forget(aPromise);
  return NS_OK;
}

void NativeMessagingPortal::MaybeDelayedCreateSession(dom::Promise& aPromise,
                                                      GVariant* aArgs) {
  MOZ_ASSERT(g_variant_is_of_type(aArgs, G_VARIANT_TYPE_STRING));

  if (!mProxy) {
    return aPromise.MaybeRejectWithOperationError(
        "No D-Bus proxy for the native messaging portal");
  }

  LOG_NMP("creating session with handle suffix %s",
          g_variant_get_string(aArgs, nullptr));

  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "session_handle_token",
                        g_variant_ref_sink(aArgs));
  auto callbackData = MakeUnique<CallbackData>(aPromise);
  g_dbus_proxy_call(mProxy, "CreateSession", g_variant_new("(a{sv})", &options),
                    G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
                    &NativeMessagingPortal::OnCreateSessionDone,
                    callbackData.release());
}

/* static */
void NativeMessagingPortal::OnCreateSessionDone(GObject* source,
                                                GAsyncResult* result,
                                                gpointer user_data) {
  GDBusProxy* proxy = G_DBUS_PROXY(source);
  UniquePtr<CallbackData> callbackData(static_cast<CallbackData*>(user_data));

  GUniquePtr<GError> error;
  RefPtr<GVariant> res = dont_AddRef(
      g_dbus_proxy_call_finish(proxy, result, getter_Transfers(error)));
  if (res) {
    RefPtr<GVariant> sessionHandle =
        dont_AddRef(g_variant_get_child_value(res, 0));
    gsize length;
    const char* value = g_variant_get_string(sessionHandle, &length);
    LOG_NMP("session created with handle %s", value);
    RefPtr<NativeMessagingPortal> portal = GetSingleton();
    portal->mSessions[value] = SessionState::Active;

    GDBusConnection* connection = g_dbus_proxy_get_connection(proxy);
    // The "Closed" signal is emitted e.g. when the user denies access to the
    // native application when the shell prompts them.
    auto subscription_id_ptr = MakeUnique<guint>(0);
    *subscription_id_ptr = g_dbus_connection_signal_subscribe(
        connection, "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Session", "Closed", value, nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE, &NativeMessagingPortal::OnSessionClosedSignal,
        subscription_id_ptr.get(), [](gpointer aUserData) {
          UniquePtr<guint> release(reinterpret_cast<guint*>(aUserData));
        });
    Unused << subscription_id_ptr.release();  // Ownership transferred above.

    callbackData->promise->MaybeResolve(nsDependentCString(value, length));
  } else {
    LOG_NMP("failed to create session: %s", error->message);
    LogError(__func__, *error);
    RejectPromiseWithErrorMessage(*callbackData->promise, *error);
  }
}

NS_IMETHODIMP
NativeMessagingPortal::CloseSession(const nsACString& aHandle, JSContext* aCx,
                                    dom::Promise** aPromise) {
  const nsCString& sessionHandle = PromiseFlatCString(aHandle);

  if (!g_variant_is_object_path(sessionHandle.get())) {
    LOG_NMP("cannot close session %s, invalid handle", sessionHandle.get());
    return NS_ERROR_INVALID_ARG;
  }

  auto sessionIterator = mSessions.find(sessionHandle.get());
  if (sessionIterator == mSessions.end()) {
    LOG_NMP("cannot close session %s, unknown handle", sessionHandle.get());
    return NS_ERROR_INVALID_ARG;
  }

  if (sessionIterator->second != SessionState::Active) {
    LOG_NMP("cannot close session %s, not active", sessionHandle.get());
    return NS_ERROR_FAILURE;
  }

  RefPtr<dom::Promise> promise;
  MOZ_TRY(GetPromise(aCx, promise));

  sessionIterator->second = SessionState::Closing;
  LOG_NMP("closing session %s", sessionHandle.get());
  auto callbackData = MakeUnique<CallbackData>(*promise, sessionHandle.get());
  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr,
      "org.freedesktop.portal.Desktop", sessionHandle.get(),
      "org.freedesktop.portal.Session", nullptr,
      &NativeMessagingPortal::OnCloseSessionProxyReady, callbackData.release());

  promise.forget(aPromise);
  return NS_OK;
}

/* static */
void NativeMessagingPortal::OnCloseSessionProxyReady(GObject* source,
                                                     GAsyncResult* result,
                                                     gpointer user_data) {
  UniquePtr<CallbackData> callbackData(static_cast<CallbackData*>(user_data));

  GUniquePtr<GError> error;
  RefPtr<GDBusProxy> proxy = dont_AddRef(
      g_dbus_proxy_new_for_bus_finish(result, getter_Transfers(error)));
  if (!proxy) {
    LOG_NMP("failed to close session: %s", error->message);
    LogError(__func__, *error);
    return RejectPromiseWithErrorMessage(*callbackData->promise, *error);
  }

  g_dbus_proxy_call(proxy, "Close", nullptr, G_DBUS_CALL_FLAGS_NONE, -1,
                    nullptr, &NativeMessagingPortal::OnCloseSessionDone,
                    callbackData.release());
}

/* static */
void NativeMessagingPortal::OnCloseSessionDone(GObject* source,
                                               GAsyncResult* result,
                                               gpointer user_data) {
  GDBusProxy* proxy = G_DBUS_PROXY(source);
  UniquePtr<CallbackData> callbackData(static_cast<CallbackData*>(user_data));

  RefPtr<NativeMessagingPortal> portal = GetSingleton();
  GUniquePtr<GError> error;
  RefPtr<GVariant> res = dont_AddRef(
      g_dbus_proxy_call_finish(proxy, result, getter_Transfers(error)));
  if (res) {
    LOG_NMP("session %s closed", callbackData->sessionHandle.get());
    portal->mSessions.erase(callbackData->sessionHandle.get());
    callbackData->promise->MaybeResolve(NS_OK);
  } else {
    LOG_NMP("failed to close session %s: %s", callbackData->sessionHandle.get(),
            error->message);
    LogError(__func__, *error);
    portal->mSessions[callbackData->sessionHandle.get()] = SessionState::Error;
    RejectPromiseWithErrorMessage(*callbackData->promise, *error);
  }
}

/* static */
void NativeMessagingPortal::OnSessionClosedSignal(
    GDBusConnection* bus, const gchar* sender_name, const gchar* object_path,
    const gchar* interface_name, const gchar* signal_name, GVariant* parameters,
    gpointer user_data) {
  guint subscription_id = *reinterpret_cast<guint*>(user_data);
  LOG_NMP("session %s was closed by the portal", object_path);
  g_dbus_connection_signal_unsubscribe(bus, subscription_id);
  RefPtr<NativeMessagingPortal> portal = GetSingleton();
  portal->mSessions.erase(object_path);
}

NS_IMETHODIMP
NativeMessagingPortal::GetManifest(const nsACString& aHandle,
                                   const nsACString& aName,
                                   const nsACString& aExtension, JSContext* aCx,
                                   dom::Promise** aPromise) {
  const nsCString& sessionHandle = PromiseFlatCString(aHandle);
  const nsCString& name = PromiseFlatCString(aName);
  const nsCString& extension = PromiseFlatCString(aExtension);

  if (!g_variant_is_object_path(sessionHandle.get())) {
    LOG_NMP("cannot find manifest for %s, invalid session handle %s",
            name.get(), sessionHandle.get());
    return NS_ERROR_INVALID_ARG;
  }

  auto sessionIterator = mSessions.find(sessionHandle.get());
  if (sessionIterator == mSessions.end()) {
    LOG_NMP("cannot find manifest for %s, unknown session handle %s",
            name.get(), sessionHandle.get());
    return NS_ERROR_INVALID_ARG;
  }

  if (sessionIterator->second != SessionState::Active) {
    LOG_NMP("cannot find manifest for %s, inactive session %s", name.get(),
            sessionHandle.get());
    return NS_ERROR_FAILURE;
  }

  if (!mProxy) {
    LOG_NMP("cannot find manifest for %s, missing D-Bus proxy", name.get());
    return NS_ERROR_FAILURE;
  }

  RefPtr<dom::Promise> promise;
  MOZ_TRY(GetPromise(aCx, promise));

  auto callbackData = MakeUnique<CallbackData>(*promise, sessionHandle.get());
  g_dbus_proxy_call(
      mProxy, "GetManifest",
      g_variant_new("(oss)", sessionHandle.get(), name.get(), extension.get()),
      G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
      &NativeMessagingPortal::OnGetManifestDone, callbackData.release());

  promise.forget(aPromise);
  return NS_OK;
}

/* static */
void NativeMessagingPortal::OnGetManifestDone(GObject* source,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  GDBusProxy* proxy = G_DBUS_PROXY(source);
  UniquePtr<CallbackData> callbackData(static_cast<CallbackData*>(user_data));

  GUniquePtr<GError> error;
  RefPtr<GVariant> jsonManifest = dont_AddRef(
      g_dbus_proxy_call_finish(proxy, result, getter_Transfers(error)));
  if (jsonManifest) {
    jsonManifest = dont_AddRef(g_variant_get_child_value(jsonManifest, 0));
    gsize length;
    const char* value = g_variant_get_string(jsonManifest, &length);
    LOG_NMP("manifest found in session %s: %s",
            callbackData->sessionHandle.get(), value);
    callbackData->promise->MaybeResolve(nsDependentCString(value, length));
  } else {
    LOG_NMP("failed to find a manifest in session %s: %s",
            callbackData->sessionHandle.get(), error->message);
    LogError(__func__, *error);
    RejectPromiseWithErrorMessage(*callbackData->promise, *error);
  }
}

NS_IMETHODIMP
NativeMessagingPortal::Start(const nsACString& aHandle, const nsACString& aName,
                             const nsACString& aExtension, JSContext* aCx,
                             dom::Promise** aPromise) {
  const nsCString& sessionHandle = PromiseFlatCString(aHandle);
  const nsCString& name = PromiseFlatCString(aName);
  const nsCString& extension = PromiseFlatCString(aExtension);

  if (!g_variant_is_object_path(sessionHandle.get())) {
    LOG_NMP("cannot start %s, invalid session handle %s", name.get(),
            sessionHandle.get());
    return NS_ERROR_INVALID_ARG;
  }

  auto sessionIterator = mSessions.find(sessionHandle.get());
  if (sessionIterator == mSessions.end()) {
    LOG_NMP("cannot start %s, unknown session handle %s", name.get(),
            sessionHandle.get());
    return NS_ERROR_INVALID_ARG;
  }

  if (sessionIterator->second != SessionState::Active) {
    LOG_NMP("cannot start %s, inactive session %s", name.get(),
            sessionHandle.get());
    return NS_ERROR_FAILURE;
  }

  if (!mProxy) {
    LOG_NMP("cannot start %s, missing D-Bus proxy", name.get());
    return NS_ERROR_FAILURE;
  }

  RefPtr<dom::Promise> promise;
  MOZ_TRY(GetPromise(aCx, promise));

  auto callbackData = MakeUnique<CallbackData>(*promise, sessionHandle.get());
  auto* releasedCallbackData = callbackData.release();

  LOG_NMP("starting %s, requested by %s in session %s", name.get(),
          extension.get(), sessionHandle.get());

  GDBusConnection* connection = g_dbus_proxy_get_connection(mProxy);
  GUniquePtr<gchar> senderName(
      g_strdup(g_dbus_connection_get_unique_name(connection)));
  g_strdelimit(senderName.get(), ".", '_');
  GUniquePtr<gchar> handleToken(
      g_strdup_printf("%s/%d", MOZ_APP_NAME, g_random_int_range(0, G_MAXINT)));
  GUniquePtr<gchar> requestPath(
      g_strdup_printf("/org/freedesktop/portal/desktop/request/%s/%s",
                      senderName.get() + 1, handleToken.get()));
  releasedCallbackData->subscription_id = g_dbus_connection_signal_subscribe(
      connection, "org.freedesktop.portal.Desktop",
      "org.freedesktop.portal.Request", "Response", requestPath.get(), nullptr,
      G_DBUS_SIGNAL_FLAGS_NONE,
      &NativeMessagingPortal::OnStartRequestResponseSignal,
      releasedCallbackData, nullptr);

  auto callbackDataCopy =
      MakeUnique<CallbackData>(*promise, sessionHandle.get());
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(handleToken.get()));
  g_dbus_proxy_call(mProxy, "Start",
                    g_variant_new("(ossa{sv})", sessionHandle.get(), name.get(),
                                  extension.get(), &options),
                    G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
                    &NativeMessagingPortal::OnStartDone,
                    callbackDataCopy.release());

  promise.forget(aPromise);
  return NS_OK;
}

/* static */
void NativeMessagingPortal::OnStartDone(GObject* source, GAsyncResult* result,
                                        gpointer user_data) {
  GDBusProxy* proxy = G_DBUS_PROXY(source);
  UniquePtr<CallbackData> callbackData(static_cast<CallbackData*>(user_data));

  GUniquePtr<GError> error;
  RefPtr<GVariant> handle = dont_AddRef(
      g_dbus_proxy_call_finish(proxy, result, getter_Transfers(error)));
  if (handle) {
    handle = dont_AddRef(g_variant_get_child_value(handle, 0));
    LOG_NMP(
        "native application start requested in session %s, pending response "
        "for %s",
        callbackData->sessionHandle.get(),
        g_variant_get_string(handle, nullptr));
  } else {
    LOG_NMP("failed to start native application in session %s: %s",
            callbackData->sessionHandle.get(), error->message);
    LogError(__func__, *error);
    RejectPromiseWithErrorMessage(*callbackData->promise, *error);
  }
}

/* static */
void NativeMessagingPortal::OnStartRequestResponseSignal(
    GDBusConnection* bus, const gchar* sender_name, const gchar* object_path,
    const gchar* interface_name, const gchar* signal_name, GVariant* parameters,
    gpointer user_data) {
  UniquePtr<CallbackData> callbackData(static_cast<CallbackData*>(user_data));

  LOG_NMP("got response signal for %s in session %s", object_path,
          callbackData->sessionHandle.get());
  g_dbus_connection_signal_unsubscribe(bus, callbackData->subscription_id);

  RefPtr<GVariant> result =
      dont_AddRef(g_variant_get_child_value(parameters, 0));
  guint32 response = g_variant_get_uint32(result);
  // Possible values for response
  // (https://flatpak.github.io/xdg-desktop-portal/#gdbus-signal-org-freedesktop-portal-Request.Response):
  //   0: Success, the request is carried out
  //   1: The user cancelled the interaction
  //   2: The user interaction was ended in some other way
  if (response == 0) {
    LOG_NMP(
        "native application start successful in session %s, requesting file "
        "descriptors",
        callbackData->sessionHandle.get());
    RefPtr<NativeMessagingPortal> portal = GetSingleton();
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_dbus_proxy_call_with_unix_fd_list(
        portal->mProxy.get(), "GetPipes",
        g_variant_new("(oa{sv})", callbackData->sessionHandle.get(), &options),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr,
        &NativeMessagingPortal::OnGetPipesDone, callbackData.release());
  } else if (response == 1) {
    LOG_NMP("native application start canceled by user in session %s",
            callbackData->sessionHandle.get());
    callbackData->promise->MaybeRejectWithAbortError(
        "Native application start canceled by user");
  } else {
    LOG_NMP("native application start failed in session %s",
            callbackData->sessionHandle.get());
    callbackData->promise->MaybeRejectWithNotFoundError(
        "Native application start failed");
  }
}

static gint GetFD(const RefPtr<GVariant>& result, GUnixFDList* fds,
                  gint index) {
  RefPtr<GVariant> value =
      dont_AddRef(g_variant_get_child_value(result, index));
  GUniquePtr<GError> error;
  gint fd = _g_unix_fd_list_get(fds, g_variant_get_handle(value),
                                getter_Transfers(error));
  if (fd == -1) {
    LOG_NMP("failed to get file descriptor at index %d: %s", index,
            error->message);
    LogError("GetFD", *error);
  }
  return fd;
}

/* static */
void NativeMessagingPortal::OnGetPipesDone(GObject* source,
                                           GAsyncResult* result,
                                           gpointer user_data) {
  GDBusProxy* proxy = G_DBUS_PROXY(source);
  UniquePtr<CallbackData> callbackData(static_cast<CallbackData*>(user_data));
  auto promise = callbackData->promise;

  RefPtr<GUnixFDList> fds;
  GUniquePtr<GError> error;
  RefPtr<GVariant> pipes =
      dont_AddRef(g_dbus_proxy_call_with_unix_fd_list_finish(
          proxy, getter_AddRefs(fds), result, getter_Transfers(error)));

  if (!pipes) {
    LOG_NMP(
        "failed to get file descriptors for native application in session %s: "
        "%s",
        callbackData->sessionHandle.get(), error->message);
    LogError(__func__, *error);
    return RejectPromiseWithErrorMessage(*promise, *error);
  }

  gint32 _stdin = GetFD(pipes, fds, 0);
  gint32 _stdout = GetFD(pipes, fds, 1);
  gint32 _stderr = GetFD(pipes, fds, 2);
  LOG_NMP(
      "got file descriptors for native application in session %s: (%d, %d, %d)",
      callbackData->sessionHandle.get(), _stdin, _stdout, _stderr);

  if (_stdin == -1 || _stdout == -1 || _stderr == -1) {
    return promise->MaybeRejectWithOperationError("Invalid file descriptor");
  }

  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
    return promise->MaybeRejectWithUnknownError(
        "Failed to initialize JS context");
  }
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> jsPipes(cx, JS_NewPlainObject(cx));
  if (!jsPipes) {
    return promise->MaybeRejectWithOperationError(
        "Failed to create a JS object to hold the file descriptors");
  }

  auto setPipeProperty = [&](const char* name, int32_t value) {
    JS::Rooted<JS::Value> jsValue(cx, JS::Value::fromInt32(value));
    return JS_SetProperty(cx, jsPipes, name, jsValue);
  };
  if (!setPipeProperty("stdin", _stdin)) {
    return promise->MaybeRejectWithOperationError(
        "Failed to set the 'stdin' property on the JS object");
  }
  if (!setPipeProperty("stdout", _stdout)) {
    return promise->MaybeRejectWithOperationError(
        "Failed to set the 'stdout' property on the JS object");
  }
  if (!setPipeProperty("stderr", _stderr)) {
    return promise->MaybeRejectWithOperationError(
        "Failed to set the 'stderr' property on the JS object");
  }

  promise->MaybeResolve(jsPipes);
}

}  // namespace mozilla::extensions
