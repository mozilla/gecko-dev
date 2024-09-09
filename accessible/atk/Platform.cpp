/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/GRefPtr.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsIAccessibleEvent.h"
#include "nsIGSettingsService.h"
#include "nsMai.h"
#include "nsServiceManagerUtils.h"
#include "nsWindow.h"
#include "prenv.h"
#include "prlink.h"

#ifdef MOZ_ENABLE_DBUS
#  include "mozilla/widget/AsyncDBus.h"
#endif
#include <gtk/gtk.h>

using namespace mozilla;
using namespace mozilla::a11y;

int atkMajorVersion = 1, atkMinorVersion = 12, atkMicroVersion = 0;

GType (*gAtkTableCellGetTypeFunc)();

extern "C" {
typedef GType (*AtkGetTypeType)(void);
typedef void (*AtkBridgeAdaptorInit)(int*, char**[]);
}

static PRLibrary* sATKLib = nullptr;
static const char sATKLibName[] = "libatk-1.0.so.0";
static const char sATKHyperlinkImplGetTypeSymbol[] =
    "atk_hyperlink_impl_get_type";

gboolean toplevel_event_watcher(GSignalInvocationHint*, guint, const GValue*,
                                gpointer);
static bool sToplevel_event_hook_added = false;
static gulong sToplevel_show_hook = 0;
static gulong sToplevel_hide_hook = 0;

GType g_atk_hyperlink_impl_type = G_TYPE_INVALID;

struct AtkBridgeModule {
  const char* libName;
  PRLibrary* lib;
  const char* initName;
  AtkBridgeAdaptorInit init;
};

static AtkBridgeModule sAtkBridge = {"libatk-bridge-2.0.so.0", nullptr,
                                     "atk_bridge_adaptor_init", nullptr};

static nsresult LoadGtkModule(AtkBridgeModule& aModule) {
  NS_ENSURE_ARG(aModule.libName);

  if (!(aModule.lib = PR_LoadLibrary(aModule.libName))) {
    return NS_ERROR_FAILURE;
  }

  // we have loaded the library, try to get the function ptrs
  if (!(aModule.init = (AtkBridgeAdaptorInit)PR_FindFunctionSymbol(
            aModule.lib, aModule.initName))) {
    // fail, :(
    PR_UnloadLibrary(aModule.lib);
    aModule.lib = nullptr;
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void a11y::PlatformInit() {
  if (!ShouldA11yBeEnabled()) return;

  sATKLib = PR_LoadLibrary(sATKLibName);
  if (!sATKLib) return;

  AtkGetTypeType pfn_atk_hyperlink_impl_get_type =
      (AtkGetTypeType)PR_FindFunctionSymbol(sATKLib,
                                            sATKHyperlinkImplGetTypeSymbol);
  if (pfn_atk_hyperlink_impl_get_type) {
    g_atk_hyperlink_impl_type = pfn_atk_hyperlink_impl_get_type();
  }

  gAtkTableCellGetTypeFunc =
      (GType(*)())PR_FindFunctionSymbol(sATKLib, "atk_table_cell_get_type");

  const char* (*atkGetVersion)() =
      (const char* (*)())PR_FindFunctionSymbol(sATKLib, "atk_get_version");
  if (atkGetVersion) {
    const char* version = atkGetVersion();
    if (version) {
      char* endPtr = nullptr;
      atkMajorVersion = strtol(version, &endPtr, 10);
      if (atkMajorVersion != 0L) {
        atkMinorVersion = strtol(endPtr + 1, &endPtr, 10);
        if (atkMinorVersion != 0L) {
          atkMicroVersion = strtol(endPtr + 1, &endPtr, 10);
        }
      }
    }
  }

  // Initialize the MAI Utility class, it will overwrite gail_util.
  g_type_class_unref(g_type_class_ref(mai_util_get_type()));

  // Init atk-bridge now
  PR_SetEnv("NO_AT_BRIDGE=0");
  nsresult rv = LoadGtkModule(sAtkBridge);
  if (NS_SUCCEEDED(rv)) {
    (*sAtkBridge.init)(nullptr, nullptr);
  }

  if (!sToplevel_event_hook_added) {
    sToplevel_event_hook_added = true;
    sToplevel_show_hook = g_signal_add_emission_hook(
        g_signal_lookup("show", GTK_TYPE_WINDOW), 0, toplevel_event_watcher,
        reinterpret_cast<gpointer>(nsIAccessibleEvent::EVENT_SHOW), nullptr);
    sToplevel_hide_hook = g_signal_add_emission_hook(
        g_signal_lookup("hide", GTK_TYPE_WINDOW), 0, toplevel_event_watcher,
        reinterpret_cast<gpointer>(nsIAccessibleEvent::EVENT_HIDE), nullptr);
  }
}

void a11y::PlatformShutdown() {
  if (sToplevel_event_hook_added) {
    sToplevel_event_hook_added = false;
    g_signal_remove_emission_hook(g_signal_lookup("show", GTK_TYPE_WINDOW),
                                  sToplevel_show_hook);
    g_signal_remove_emission_hook(g_signal_lookup("hide", GTK_TYPE_WINDOW),
                                  sToplevel_hide_hook);
  }

  if (sAtkBridge.lib) {
    // Do not shutdown/unload atk-bridge,
    // an exit function registered will take care of it
    // PR_UnloadLibrary(sAtkBridge.lib);
    sAtkBridge.lib = nullptr;
    sAtkBridge.init = nullptr;
  }
  // if (sATKLib) {
  //     PR_UnloadLibrary(sATKLib);
  //     sATKLib = nullptr;
  // }
}

static const char sAccEnv[] = "GNOME_ACCESSIBILITY";
#ifdef MOZ_ENABLE_DBUS
StaticRefPtr<GDBusProxy> sA11yBusProxy;
StaticRefPtr<GCancellable> sCancellable;
#endif

static void StartAccessibility() {
  if (nsWindow* window = nsWindow::GetFocusedWindow()) {
    window->DispatchActivateEventAccessible();
  }
}

static void A11yBusProxyPropertyChanged(GDBusProxy* aProxy,
                                        GVariant* aChangedProperties,
                                        char** aInvalidatedProperties,
                                        gpointer aUserData) {
  gboolean isEnabled;
  g_variant_lookup(aChangedProperties, "IsEnabled", "b", &isEnabled);
  if (isEnabled) {
    StartAccessibility();
  }
}

// Called from `nsWindow::Create()` before the window is shown.
void a11y::PreInit() {
#ifdef MOZ_ENABLE_DBUS
  static bool sInited = false;
  if (sInited) {
    return;
  }
  sInited = true;
  sCancellable = dont_AddRef(g_cancellable_new());

  widget::CreateDBusProxyForBus(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE,
                                /* aInterfaceInfo = */ nullptr, "org.a11y.Bus",
                                "/org/a11y/bus", "org.a11y.Status",
                                sCancellable)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,

          [](RefPtr<GDBusProxy>&& aProxy) {
            sA11yBusProxy = std::move(aProxy);
            sCancellable = nullptr;
            g_signal_connect(sA11yBusProxy, "g-properties-changed",
                             G_CALLBACK(A11yBusProxyPropertyChanged), nullptr);
            RefPtr<GVariant> isEnabled = dont_AddRef(
                g_dbus_proxy_get_cached_property(sA11yBusProxy, "IsEnabled"));
            if (isEnabled && g_variant_get_boolean(isEnabled)) {
              // If a window is already focused, `StartAccessibility()` will
              // initialize a11y by sending an activate event. If the window has
              // not yet been shown/focused, nothing will happen. We now have
              // the `IsEnabled` property cached so when `ShouldA11yBeEnabled()`
              // is called from `nsWindow::Show()`, a root accessible will be
              // created and events will be dispatched henceforth.
              StartAccessibility();
            }
          },
          [](GUniquePtr<GError>&& aError) {
            sCancellable = nullptr;
            if (!g_error_matches(aError.get(), G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED)) {
              g_warning(
                  "Failed to create DBus proxy for org.a11y.Bus: "
                  "%s\n",
                  aError->message);
            }
          });

  RunOnShutdown([] {
    if (sCancellable) {
      g_cancellable_cancel(sCancellable);
      sCancellable = nullptr;
    }

    if (sA11yBusProxy) {
      sA11yBusProxy = nullptr;
    }
  });
#endif
}

bool a11y::ShouldA11yBeEnabled() {
  EPlatformDisabledState disabledState = PlatformDisabledState();
  if (disabledState == ePlatformIsDisabled) {
    return false;
  }
  if (disabledState == ePlatformIsForceEnabled) {
    return true;
  }

  // check if accessibility enabled/disabled by environment variable
  if (const char* envValue = PR_GetEnv(sAccEnv)) {
    return !!atoi(envValue);
  }

#ifdef MOZ_ENABLE_DBUS
  if (sA11yBusProxy) {
    RefPtr<GVariant> isEnabled = dont_AddRef(
        g_dbus_proxy_get_cached_property(sA11yBusProxy, "IsEnabled"));
    // result can be null if proxy isn't actually working.
    if (isEnabled) {
      return g_variant_get_boolean(isEnabled);
    }
  }
#endif

  // check GSettings
  nsCOMPtr<nsIGSettingsService> gsettings =
      do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);

  if (gsettings) {
    bool shouldEnable = false;
    nsCOMPtr<nsIGSettingsCollection> a11y_settings;
    gsettings->GetCollectionForSchema(
        nsLiteralCString("org.gnome.desktop.interface"),
        getter_AddRefs(a11y_settings));
    if (a11y_settings) {
      a11y_settings->GetBoolean(nsLiteralCString("toolkit-accessibility"),
                                &shouldEnable);
    }

    return shouldEnable;
  }

  return false;
}

uint64_t a11y::GetCacheDomainsForKnownClients(uint64_t aCacheDomains) {
  return aCacheDomains;
}
