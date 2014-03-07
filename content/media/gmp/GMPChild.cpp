/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPChild.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "GMPVideoHost.h"
#include "nsIFile.h"
#include "nsXULAppAPI.h"
#include <stdlib.h>
#include "gmp-video-decode.h"
#include "gmp-video-encode.h"
#include "GMPPlatform.h"

namespace mozilla {
namespace gmp {

GMPChild::GMPChild()
: mLib(nullptr),
  mGetAPIFunc(nullptr)
{
}

GMPChild::~GMPChild()
{
}

bool
GMPChild::Init(const std::string& aPluginPath,
               base::ProcessHandle aParentProcessHandle,
               MessageLoop* aIOLoop,
               IPC::Channel* aChannel)
{
  MOZ_ASSERT(aChannel, "Need a channel!");

  return LoadPluginLibrary(aPluginPath) &&
         Open(aChannel, aParentProcessHandle, aIOLoop);
}

bool
GMPChild::LoadPluginLibrary(const std::string& aPluginPath)
{
  nsAutoCString pluginPath(aPluginPath.c_str());

  nsCOMPtr<nsIFile> libFile;
  NS_NewNativeLocalFile(pluginPath, true, getter_AddRefs(libFile));

  nsAutoString leafName;
  if (NS_FAILED(libFile->GetLeafName(leafName))) {
    return false;
  }
  nsAutoString baseName(Substring(leafName, 4, leafName.Length() - 1));

#if defined(XP_MACOSX)
  nsAutoString binaryName = NS_LITERAL_STRING("lib") + baseName + NS_LITERAL_STRING(".dylib");
#elif defined(OS_POSIX)
  nsAutoString binaryName = NS_LITERAL_STRING("lib") + baseName + NS_LITERAL_STRING(".so");
#elif defined(XP_WIN)
  nsAutoString binaryName =                            baseName + NS_LITERAL_STRING(".dll");
#else
#error not defined
#endif
  libFile->AppendRelativePath(binaryName);

  nsAutoCString nativePath;
  libFile->GetNativePath(nativePath);
  mLib = PR_LoadLibrary(nativePath.get());
  if (!mLib) {
    return false;
  }

  GMPInitFunc initFunc = reinterpret_cast<GMPInitFunc>(PR_FindFunctionSymbol(mLib, "GMPInit"));
  if (!initFunc) {
    return false;
  }

  auto platformAPI = new GMPPlatformAPI();
  if (!platformAPI) {
    return false;
  }
  InitPlatformAPI(*platformAPI);

  if (initFunc(platformAPI) != GMPNoErr) {
    return false;
  }

  mGetAPIFunc = reinterpret_cast<GMPGetAPIFunc>(PR_FindFunctionSymbol(mLib, "GMPGetAPI"));
  if (!mGetAPIFunc) {
    return false;
  }

  return true;
}

void
GMPChild::ActorDestroy(ActorDestroyReason why)
{
  if (mLib) {
    GMPShutdownFunc shutdownFunc = reinterpret_cast<GMPShutdownFunc>(PR_FindFunctionSymbol(mLib, "GMPShutdown"));
    if (shutdownFunc) {
      shutdownFunc();
    }
  }

  if (AbnormalShutdown == why) {
    NS_WARNING("Abnormal shutdown of GMP process!");
    _exit(0);
  }

  XRE_ShutdownChildProcess();
}

void
GMPChild::ProcessingError(Result what)
{
  switch (what) {
    case MsgDropped:
      _exit(0); // Don't trigger a crash report.
    case MsgNotKnown:
      MOZ_CRASH("aborting because of MsgNotKnown");
    case MsgNotAllowed:
      MOZ_CRASH("aborting because of MsgNotAllowed");
    case MsgPayloadError:
      MOZ_CRASH("aborting because of MsgPayloadError");
    case MsgProcessingError:
      MOZ_CRASH("aborting because of MsgProcessingError");
    case MsgRouteError:
      MOZ_CRASH("aborting because of MsgRouteError");
    case MsgValueError:
      MOZ_CRASH("aborting because of MsgValueError");
    default:
      MOZ_CRASH("not reached");
  }
}

PGMPVideoDecoderChild*
GMPChild::AllocPGMPVideoDecoderChild()
{
  GMPVideoDecoderChild* vdc = new GMPVideoDecoderChild();
  return vdc;
}

bool
GMPChild::DeallocPGMPVideoDecoderChild(PGMPVideoDecoderChild* aActor)
{
  delete aActor;
  return true;
}

PGMPVideoEncoderChild*
GMPChild::AllocPGMPVideoEncoderChild()
{
  GMPVideoEncoderChild* vec = new GMPVideoEncoderChild();
  return vec;
}

bool
GMPChild::DeallocPGMPVideoEncoderChild(PGMPVideoEncoderChild* aActor)
{
  delete aActor;
  return true;
}

bool
GMPChild::RecvPGMPVideoDecoderConstructor(PGMPVideoDecoderChild* actor)
{
  auto vdc = static_cast<GMPVideoDecoderChild*>(actor);

  void* vdvoid = nullptr;
  GMPErr err = mGetAPIFunc("decode-video", &vdc->Host(), &vdvoid);
  if (err != GMPNoErr || !vdvoid) {
    return false;
  }
  auto vd = static_cast<GMPVideoDecoder*>(vdvoid);

  vdc->Init(vd);
  return true;
}

bool
GMPChild::RecvPGMPVideoEncoderConstructor(PGMPVideoEncoderChild* actor)
{
  auto vec = static_cast<GMPVideoEncoderChild*>(actor);

  void* vevoid = nullptr;
  GMPErr err = mGetAPIFunc("encode-video", &vec->Host(), &vevoid);
  if (err != GMPNoErr || !vevoid) {
    return false;
  }
  auto ve = static_cast<GMPVideoEncoder*>(vevoid);

  vec->Init(ve);
  return true;
}

} // namespace gmp
} // namespace mozilla
