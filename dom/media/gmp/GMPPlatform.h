/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPPlatform_h_
#define GMPPlatform_h_

#include "mozilla/RefPtr.h"
#include "gmp-platform.h"
#include <functional>
#include "mozilla/gmp/PGMPChild.h"

namespace mozilla {
#ifdef XP_WIN
struct ModulePaths;
#endif

namespace ipc {
class ByteBuf;
}  // namespace ipc

namespace gmp {

class GMPChild;

void InitPlatformAPI(GMPPlatformAPI& aPlatformAPI, GMPChild* aChild);
void ShutdownPlatformAPI();

GMPErr RunOnMainThread(GMPTask* aTask);

GMPTask* NewGMPTask(std::function<void()>&& aFunction);

GMPErr SetTimerOnMainThread(GMPTask* aTask, int64_t aTimeoutMS);

/**
 * This is intended to be used by encoders/decoders that will make a GMP call
 * that is a synchronous post to the GMP worker thread. Because the GMP worker
 * threads can synchronously callback to the main thread, this has the potential
 * for a deadlock. If the encoder/decoder tracks any outstanding requests that
 * will result in a synchronous callback to the main thread, we can simply spin
 * the event loop on those callbacks until they are completed. Then we can
 * safefully make our own synchronous call to the GMP worker thread without fear
 * of a deadlock.
 *
 * Note that each encoder/decoder has its own worker thread, so assuming we
 * drain the synchronous events for that specific encoder/decoder, we know there
 * are no more forthcoming to cause us to deadlock.
 */
using SpinPendingPredicate = std::function<bool()>;
bool SpinPendingGmpEventsUntil(const SpinPendingPredicate& aPred,
                               uint32_t aTimeoutMs);

void SendFOGData(ipc::ByteBuf&& buf);

#ifdef XP_WIN
RefPtr<PGMPChild::GetModulesTrustPromise> SendGetModulesTrust(
    ModulePaths&& aModules, bool aRunNormal);
#endif

}  // namespace gmp
}  // namespace mozilla

#endif  // GMPPlatform_h_
