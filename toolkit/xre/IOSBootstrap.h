/* clang-format off */
/* -*- Mode: Objective-C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IOSBootstrap_h
#define IOSBootstrap_h

/* This header needs to stay valid Objective-C (not Objective-C++) when
 * built independently because it is used for Swift bridging too. */
#ifdef MOZILLA_CLIENT
#  include <mozilla/Types.h>
#else
#  define MOZ_EXPORT
#  define MOZ_BEGIN_EXTERN_C
#  define MOZ_END_EXTERN_C
#endif
#include <xpc/xpc.h>

@protocol SwiftGeckoViewRuntime;
@protocol GeckoProcessExtension;

MOZ_BEGIN_EXTERN_C

MOZ_EXPORT int MainProcessInit(int aArgc, char** aArgv,
                               id<SwiftGeckoViewRuntime> aRuntime);

MOZ_EXPORT void ChildProcessInit(xpc_connection_t aXpcConnection,
                                 id<GeckoProcessExtension> aProcess,
                                 id<SwiftGeckoViewRuntime> aRuntime);

MOZ_END_EXTERN_C

#endif /* IOSBootstrap_h */
