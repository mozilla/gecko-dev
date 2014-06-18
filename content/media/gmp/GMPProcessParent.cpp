/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPProcessParent.h"

#include "base/string_util.h"
#include "base/process_util.h"

using std::vector;
using std::string;

using mozilla::gmp::GMPProcessParent;
using mozilla::ipc::GeckoChildProcessHost;
using base::ProcessArchitecture;

template<>
struct RunnableMethodTraits<GMPProcessParent>
{
  static void RetainCallee(GMPProcessParent* obj) { }
  static void ReleaseCallee(GMPProcessParent* obj) { }
};

namespace mozilla {
namespace gmp {

GMPProcessParent::GMPProcessParent(const std::string& aGMPPath)
: GeckoChildProcessHost(GeckoProcessType_GMPlugin),
  mGMPPath(aGMPPath)
{
}

GMPProcessParent::~GMPProcessParent()
{
}

bool
GMPProcessParent::Launch(int32_t aTimeoutMs)
{
  vector<string> args;
  args.push_back(mGMPPath);
  return SyncLaunch(args, aTimeoutMs, base::GetCurrentProcessArchitecture());
}

void
GMPProcessParent::Delete()
{
  MessageLoop* currentLoop = MessageLoop::current();
  MessageLoop* ioLoop = XRE_GetIOMessageLoop();

  if (currentLoop == ioLoop) {
    delete this;
    return;
  }

  ioLoop->PostTask(FROM_HERE, NewRunnableMethod(this, &GMPProcessParent::Delete));
}

} // namespace gmp
} // namespace mozilla
