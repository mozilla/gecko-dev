/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPChild_h_
#define GMPChild_h_

#include "nsString.h"
#include "mozilla/gmp/PGMPChild.h"
#include "gmp-entrypoints.h"
#include "prlink.h"

namespace mozilla {
namespace gmp {

class GMPChild : public PGMPChild
{
public:
  GMPChild();
  virtual ~GMPChild();

  bool Init(const std::string& aPluginPath,
            base::ProcessHandle aParentProcessHandle,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);
  bool LoadPluginLibrary(const std::string& aPluginPath);

private:
  virtual PGMPVideoDecoderChild* AllocPGMPVideoDecoderChild() MOZ_OVERRIDE;
  virtual bool DeallocPGMPVideoDecoderChild(PGMPVideoDecoderChild* actor) MOZ_OVERRIDE;
  virtual PGMPVideoEncoderChild* AllocPGMPVideoEncoderChild() MOZ_OVERRIDE;
  virtual bool DeallocPGMPVideoEncoderChild(PGMPVideoEncoderChild* aActor) MOZ_OVERRIDE;
  virtual bool RecvPGMPVideoDecoderConstructor(PGMPVideoDecoderChild* actor) MOZ_OVERRIDE;
  virtual bool RecvPGMPVideoEncoderConstructor(PGMPVideoEncoderChild* actor) MOZ_OVERRIDE;
  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
  virtual void ProcessingError(Result what) MOZ_OVERRIDE;

  PRLibrary* mLib;
  GMPGetAPIFunc mGetAPIFunc;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPChild_h_
