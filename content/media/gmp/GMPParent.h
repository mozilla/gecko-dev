/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPParent_h_
#define GMPParent_h_

#include "nscore.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIFile.h"
#include "GMPProcessParent.h"
#include "mozilla/gmp/PGMPParent.h"
#include "nsCOMPtr.h"
#include "GMPVideoDecoderParent.h"
#include "GMPVideoEncoderParent.h"

class nsILineInputStream;

namespace mozilla {
namespace gmp {

class GMPCapability
{
public:
  nsCString mAPIName;
  nsTArray<nsCString> mAPITags;
};

class GMPParent : public nsISupports,
                  public PGMPParent
{
public:
  GMPParent();

  NS_DECL_ISUPPORTS

  enum GMPState {
    GMPStateNotLoaded,
    GMPStateLoaded
  };

  nsresult Init(nsIFile *pluginDir);
  nsresult LoadProcess();
  void MaybeUnloadProcess();
  void UnloadProcess();
  bool SupportsAPI(const nsCString &aAPI, const nsCString &aTag);
  nsresult GetGMPVideoDecoder(GMPVideoDecoderParent** gmpVD);
  void VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder);
  nsresult GetGMPVideoEncoder(GMPVideoEncoderParent** gmpVE);
  void VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder);

private:
  virtual ~GMPParent();
  bool EnsureProcessLoaded();
  nsresult ParseNextRecord(nsILineInputStream* aLineInputStream,
                           const nsCString& aPrefix,
                           nsCString& aResult,
                           bool& aMoreLines);
  nsresult ReadGMPMetaData();
  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
  virtual PGMPVideoDecoderParent* AllocPGMPVideoDecoderParent() MOZ_OVERRIDE;
  virtual bool DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* actor) MOZ_OVERRIDE;
  virtual PGMPVideoEncoderParent* AllocPGMPVideoEncoderParent() MOZ_OVERRIDE;
  virtual bool DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* actor) MOZ_OVERRIDE;

  GMPState           mState;
  nsCOMPtr<nsIFile>  mDirectory; // plugin directory on disk
  nsString            mName; // base name of plugin on disk, UTF-16 because used for paths
  nsCString           mDisplayName; // name of plugin displayed to users
  nsCString           mDescription; // description of plugin for display to users
  nsCString           mVersion;
  nsTArray<nsAutoPtr<GMPCapability>> mCapabilities;
  GMPProcessParent*  mProcess;

  nsTArray<nsRefPtr<GMPVideoDecoderParent>> mVideoDecoders;
  nsTArray<nsRefPtr<GMPVideoEncoderParent>> mVideoEncoders;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPParent_h_
