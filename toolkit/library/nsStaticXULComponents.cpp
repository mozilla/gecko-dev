/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Module.h"
#include "nsXPCOM.h"
#include "nsMemory.h"

#ifdef MOZ_AUTH_EXTENSION
#define AUTH_MODULE    MODULE(nsAuthModule)
#else
#define AUTH_MODULE
#endif

#ifdef MOZ_PERMISSIONS
#define PERMISSIONS_MODULES                  \
    MODULE(nsCookieModule)                   \
    MODULE(nsPermissionsModule)
#else
#define PERMISSIONS_MODULES
#endif

#ifdef MOZ_UNIVERSALCHARDET
#define UNIVERSALCHARDET_MODULE MODULE(nsUniversalCharDetModule)
#else
#define UNIVERSALCHARDET_MODULE
#endif

#ifdef XP_WIN
#  define WIDGET_MODULES MODULE(nsWidgetModule)
#elif defined(XP_MACOSX)
#  define WIDGET_MODULES MODULE(nsWidgetMacModule)
#elif defined(XP_OS2)
#  define WIDGET_MODULES MODULE(nsWidgetOS2Module)
#elif defined(MOZ_WIDGET_GTK)
#  define WIDGET_MODULES MODULE(nsWidgetGtk2Module)
#elif defined(MOZ_WIDGET_QT)
#  define WIDGET_MODULES MODULE(nsWidgetQtModule)
#elif defined(MOZ_WIDGET_ANDROID)
#  define WIDGET_MODULES MODULE(nsWidgetAndroidModule)
#elif defined(MOZ_WIDGET_GONK)
#  define WIDGET_MODULES MODULE(nsWidgetGonkModule)
#else
#  error Unknown widget module.
#endif

#ifdef ICON_DECODER
#define ICON_MODULE MODULE(nsIconDecoderModule)
#else
#define ICON_MODULE
#endif

#ifdef MOZ_ENABLE_XREMOTE
#define XREMOTE_MODULES MODULE(RemoteServiceModule)
#else
#define XREMOTE_MODULES
#endif

#ifdef MOZ_PREF_EXTENSIONS
#define SYSTEMPREF_MODULES MODULE(nsAutoConfigModule)
#else
#define SYSTEMPREF_MODULES
#endif

#ifdef ENABLE_LAYOUTDEBUG
#define LAYOUT_DEBUG_MODULE MODULE(nsLayoutDebugModule)
#else
#define LAYOUT_DEBUG_MODULE
#endif

#ifdef MOZ_JSDEBUGGER
#define JSDEBUGGER_MODULES \
    MODULE(JavaScript_Debugger)
#else
#define JSDEBUGGER_MODULES
#endif

#if defined(MOZ_FILEVIEW) && defined(MOZ_XUL)
#define FILEVIEW_MODULE MODULE(nsFileViewModule)
#else
#define FILEVIEW_MODULE
#endif

#ifdef MOZ_ZIPWRITER
#define ZIPWRITER_MODULE MODULE(ZipWriterModule)
#else
#define ZIPWRITER_MODULE
#endif

#ifdef MOZ_PLACES
#define PLACES_MODULES \
    MODULE(nsPlacesModule)
#else
#define PLACES_MODULES
#endif

#ifdef MOZ_XUL
#define XULENABLED_MODULES                   \
    MODULE(tkAutoCompleteModule)             \
    MODULE(satchel)                          \
    MODULE(PKI)
#else
#define XULENABLED_MODULES
#endif

#ifdef MOZ_SPELLCHECK
#define SPELLCHECK_MODULE MODULE(mozSpellCheckerModule)
#else
#define SPELLCHECK_MODULE
#endif

#ifdef MOZ_XUL
#ifdef MOZ_WIDGET_GTK
#define UNIXPROXY_MODULE MODULE(nsUnixProxyModule)
#endif
#if defined(MOZ_WIDGET_QT)
#define UNIXPROXY_MODULE MODULE(nsUnixProxyModule)
#endif
#endif
#ifndef UNIXPROXY_MODULE
#define UNIXPROXY_MODULE
#endif

#if defined(XP_MACOSX)
#define OSXPROXY_MODULE MODULE(nsOSXProxyModule)
#else
#define OSXPROXY_MODULE
#endif

#if defined(XP_WIN)
#define WINDOWSPROXY_MODULE MODULE(nsWindowsProxyModule)
#else
#define WINDOWSPROXY_MODULE
#endif

#if defined(MOZ_WIDGET_ANDROID)
#define ANDROIDPROXY_MODULE MODULE(nsAndroidProxyModule)
#else
#define ANDROIDPROXY_MODULE
#endif

#if defined(BUILD_CTYPES)
#define JSCTYPES_MODULE MODULE(jsctypes)
#else
#define JSCTYPES_MODULE
#endif

#ifndef MOZ_APP_COMPONENT_MODULES
#if defined(MOZ_APP_COMPONENT_INCLUDE)
#include MOZ_APP_COMPONENT_INCLUDE
#define MOZ_APP_COMPONENT_MODULES APP_COMPONENT_MODULES
#else
#define MOZ_APP_COMPONENT_MODULES
#endif
#endif

#if defined(MOZ_ENABLE_PROFILER_SPS)
#define PROFILER_MODULE MODULE(nsProfilerModule)
#else
#define PROFILER_MODULE
#endif

#if defined(MOZ_WEBRTC)
#define PEERCONNECTION_MODULE MODULE(peerconnection)
#else
#define PEERCONNECTION_MODULE
#endif

#if defined(MOZ_GIO_COMPONENT)
#define GIO_MODULE MODULE(nsGIOModule)
#else
#define GIO_MODULE
#endif

#if defined(MOZ_SYNTH_PICO)
#define SYNTH_PICO_MODULE MODULE(synthpico)
#else
#define SYNTH_PICO_MODULE
#endif

#define XUL_MODULES                          \
    MODULE(nsUConvModule)                    \
    MODULE(nsI18nModule)                     \
    MODULE(nsChardetModule)                  \
    UNIVERSALCHARDET_MODULE                  \
    MODULE(necko)                            \
    PERMISSIONS_MODULES                      \
    AUTH_MODULE                              \
    MODULE(nsJarModule)                      \
    ZIPWRITER_MODULE                         \
    MODULE(StartupCacheModule)               \
    MODULE(nsPrefModule)                     \
    MODULE(nsRDFModule)                      \
    MODULE(nsWindowDataSourceModule)         \
    MODULE(nsParserModule)                   \
    MODULE(nsImageLib2Module)                \
    MODULE(nsMediaSnifferModule)             \
    MODULE(nsGfxModule)                      \
    PROFILER_MODULE                          \
    WIDGET_MODULES                           \
    ICON_MODULE                              \
    MODULE(nsPluginModule)                   \
    MODULE(nsLayoutModule)                   \
    MODULE(docshell_provider)                \
    MODULE(embedcomponents)                  \
    MODULE(Browser_Embedding_Module)         \
    MODULE(appshell)                         \
    MODULE(nsTransactionManagerModule)       \
    MODULE(nsComposerModule)                 \
    MODULE(application)                      \
    MODULE(Apprunner)                        \
    MODULE(CommandLineModule)                \
    FILEVIEW_MODULE                          \
    MODULE(mozStorageModule)                 \
    PLACES_MODULES                           \
    XULENABLED_MODULES                       \
    MODULE(nsToolkitCompsModule)             \
    XREMOTE_MODULES                          \
    JSDEBUGGER_MODULES                       \
    MODULE(BOOT)                             \
    MODULE(NSS)                              \
    SYSTEMPREF_MODULES                       \
    SPELLCHECK_MODULE                        \
    LAYOUT_DEBUG_MODULE                      \
    UNIXPROXY_MODULE                         \
    OSXPROXY_MODULE                          \
    WINDOWSPROXY_MODULE                      \
    ANDROIDPROXY_MODULE                      \
    JSCTYPES_MODULE                          \
    MODULE(jsreflect)                        \
    MODULE(jsperf)                           \
    MODULE(identity)                         \
    MODULE(nsServicesCryptoModule)           \
    MOZ_APP_COMPONENT_MODULES                \
    MODULE(nsTelemetryModule)                \
    MODULE(jsinspector)                      \
    MODULE(jsdebugger)                       \
    PEERCONNECTION_MODULE                    \
    GIO_MODULE                               \
    SYNTH_PICO_MODULE                        \
    MODULE(DiskSpaceWatcherModule)           \
    /* end of list */

#define MODULE(_name) \
  NSMODULE_DECL(_name);

XUL_MODULES

#ifdef MOZ_WIDGET_GONK
MODULE(WifiProxyServiceModule)
MODULE(NetworkWorkerModule)
#endif

#undef MODULE

#define MODULE(_name) \
    &NSMODULE_NAME(_name),

extern const mozilla::Module *const *const kPStaticModules[] = {
  XUL_MODULES
#ifdef MOZ_WIDGET_GONK
MODULE(WifiProxyServiceModule)
MODULE(NetworkWorkerModule)
#endif
  nullptr
};

#undef MODULE
