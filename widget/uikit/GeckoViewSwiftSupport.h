/* clang-format off */
/* -*- Mode: Objective-C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoView_GeckoViewSwiftSupport_h
#define GeckoView_GeckoViewSwiftSupport_h

/* This header needs to stay valid Objective-C (not Objective-C++) when
 * built independently because it is used for Swift bridging too. */
#ifdef MOZILLA_CLIENT
#  include <mozilla/Types.h>
#else
#  define MOZ_EXPORT
#  define MOZ_BEGIN_EXTERN_C
#  define MOZ_END_EXTERN_C
#endif

#import <Foundation/NSObject.h>

@protocol SwiftGeckoViewRuntime <NSObject>
@end
@protocol GeckoProcessExtension <NSObject>
@end

@protocol GeckoEventDispatcher <NSObject>
@end

@protocol SwiftEventDispatcher <NSObject>
- (void)attach:(id<GeckoEventDispatcher>)geckoEventDispatcher;
@end

@protocol GeckoViewWindow <NSObject>
@end

MOZ_BEGIN_EXTERN_C

MOZ_EXPORT id<GeckoViewWindow> GeckoViewOpenWindow(
    NSString* aId, id<SwiftEventDispatcher> aDispatcher, id aInitData,
    bool aPrivateMode);

MOZ_END_EXTERN_C

#endif /* GeckoView_GeckoViewSwiftSupport_h */
