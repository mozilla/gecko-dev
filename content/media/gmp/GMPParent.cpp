/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPParent.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsNetUtil.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"

namespace mozilla {
namespace gmp {

NS_IMPL_ISUPPORTS0(GMPParent)

GMPParent::GMPParent()
: mState(GMPStateNotLoaded),
  mProcess(nullptr)
{
}

GMPParent::~GMPParent()
{
}

nsresult
GMPParent::Init(nsIFile* pluginDir)
{
  MOZ_ASSERT(pluginDir, "Plugin directory cannot be NULL!");
  mDirectory = pluginDir;

  nsAutoString basename;
  nsresult rv;
  if (NS_FAILED(rv = pluginDir->GetLeafName(basename))) {
    return rv;
  }
  mName = Substring(basename, 4, basename.Length() - 1);

  return ReadGMPMetaData();
}

nsresult
GMPParent::LoadProcess()
{
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");

  if (mState == GMPStateLoaded) {
    return NS_OK;
  }
  nsAutoCString path;
  mDirectory->GetNativePath(path);
  mProcess = new GMPProcessParent(path.get());
  if (!mProcess->Launch(30 * 1000)) {
    mProcess->Delete();
    mProcess = nullptr;
    return NS_ERROR_FAILURE;
  }

  Open(mProcess->GetChannel(), mProcess->GetChildProcessHandle());

  mState = GMPStateLoaded;
  return NS_OK;
}

void
GMPParent::MaybeUnloadProcess()
{
  if (mVideoDecoders.Length() == 0 &&
      mVideoEncoders.Length() == 0) {
    UnloadProcess();
  }
}

void
GMPParent::UnloadProcess()
{
  if (mState == GMPStateNotLoaded) {
    return;
  }

  mState = GMPStateNotLoaded;

  // Invalidate and remove any remaining API objects.
  for (int32_t i = mVideoDecoders.Length() - 1; i >= 0; i--) {
    mVideoDecoders[i]->DecodingComplete();
  }

  // Invalidate and remove any remaining API objects.
  for (int32_t i = mVideoEncoders.Length() - 1; i >= 0; i--) {
    mVideoEncoders[i]->EncodingComplete();
  } 

  Close();
  if (mProcess) {
    mProcess->Delete();
    mProcess = nullptr;
  }
}

void
GMPParent::VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder)
{
  mVideoDecoders.RemoveElement(aDecoder);

  // Recv__delete__ is on the stack, don't potentially destroy the top-level actor
  // until after this has completed.
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &GMPParent::MaybeUnloadProcess);
  NS_DispatchToMainThread(event);
}

void
GMPParent::VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder)
{
  mVideoEncoders.RemoveElement(aEncoder);

  // Recv__delete__ is on the stack, don't potentially destroy the top-level actor
  // until after this has completed.
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &GMPParent::MaybeUnloadProcess);
  NS_DispatchToMainThread(event);
}

bool
GMPParent::SupportsAPI(const nsCString &aAPI, const nsCString &aTag)
{
  for (uint32_t i = 0; i < mCapabilities.Length(); i++) {
    if (mCapabilities[i]->mAPIName.Equals(aAPI)) {
      nsTArray<nsCString>& tags = mCapabilities[i]->mAPITags;
      for (uint32_t j = 0; j < tags.Length(); j++) {
        if (tags[j].Equals(aTag)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool
GMPParent::EnsureProcessLoaded()
{
  if (mState == GMPStateLoaded) {
    return true;
  }
  LoadProcess();
  return (mState == GMPStateLoaded);
}

nsresult
GMPParent::GetGMPVideoDecoder(GMPVideoDecoderParent** gmpVD)
{
  if (!EnsureProcessLoaded()) {
    return NS_ERROR_FAILURE;
  }

  PGMPVideoDecoderParent* pvdp = SendPGMPVideoDecoderConstructor();
  if (!pvdp) {
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<GMPVideoDecoderParent> vdp = static_cast<GMPVideoDecoderParent*>(pvdp);
  mVideoDecoders.AppendElement(vdp);
  vdp.forget(gmpVD);

  return NS_OK;
}

nsresult
GMPParent::GetGMPVideoEncoder(GMPVideoEncoderParent** gmpVE)
{
  if (!EnsureProcessLoaded()) {
    return NS_ERROR_FAILURE;
  }

  PGMPVideoEncoderParent* pvep = SendPGMPVideoEncoderConstructor();
  if (!pvep) {
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<GMPVideoEncoderParent> vep = static_cast<GMPVideoEncoderParent*>(pvep);
  mVideoEncoders.AppendElement(vep);
  vep.forget(gmpVE);

  return NS_OK;
}

void
GMPParent::ActorDestroy(ActorDestroyReason why)
{
  UnloadProcess();
}

PGMPVideoDecoderParent*
GMPParent::AllocPGMPVideoDecoderParent()
{
  GMPVideoDecoderParent* vdp = new GMPVideoDecoderParent(this);
  NS_ADDREF(vdp);
  return vdp;
}

bool
GMPParent::DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* actor)
{
  GMPVideoDecoderParent* vdp = static_cast<GMPVideoDecoderParent*>(actor);
  NS_RELEASE(vdp);
  return true;
}

PGMPVideoEncoderParent*
GMPParent::AllocPGMPVideoEncoderParent()
{
  GMPVideoEncoderParent* vep = new GMPVideoEncoderParent(this);
  NS_ADDREF(vep);
  return vep;
}

bool
GMPParent::DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* actor)
{
  GMPVideoEncoderParent* vep = static_cast<GMPVideoEncoderParent*>(actor);
  NS_RELEASE(vep);
  return true;
}

nsresult
GMPParent::ParseNextRecord(nsILineInputStream* aLineInputStream,
                           const nsCString& aPrefix,
                           nsCString& aResult,
                           bool& aMoreLines)
{
  nsAutoCString record;
  nsresult rv = aLineInputStream->ReadLine(record, &aMoreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (record.Length() <= aPrefix.Length() ||
      !Substring(record, 0, aPrefix.Length()).Equals(aPrefix)) {
    return NS_ERROR_FAILURE;
  }

  aResult = (Substring(record, aPrefix.Length(), record.Length() - aPrefix.Length()));
  aResult.Trim("\b\t\r\n ");

  return NS_OK;
}

nsresult
GMPParent::ReadGMPMetaData()
{
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");
  MOZ_ASSERT(!mName.IsEmpty(), "Plugin mName cannot be empty!");

  nsString info_leaf = mName + NS_LITERAL_STRING(".info");
  nsCOMPtr<nsIFile> infoFile;
  mDirectory->Clone(getter_AddRefs(infoFile));
  infoFile->AppendRelativePath(info_leaf);

  nsCOMPtr<nsIInputStream> inputStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), infoFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(inputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString value;
  bool moreLines = false;

  // 'Name:' record
  nsCString prefix = NS_LITERAL_CSTRING("Name:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (value.IsEmpty()) {
    // Not OK for name to be empty. Must have one non-whitespace character.
    return NS_ERROR_FAILURE;
  }
  mDisplayName = value;

  // 'Description:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("Description:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mDescription = value;

  // 'Version:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("Version:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mVersion = value;

  // 'Capability:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("APIs:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCCharSeparatedTokenizer apiTokens(value, ',');
  while (apiTokens.hasMoreTokens()) {
    nsAutoCString api(apiTokens.nextToken());
    api.StripWhitespace();
    if (api.IsEmpty()) {
      continue;
    }

    auto tagsStart = api.FindChar('[');
    if (tagsStart == 0) {
      // Not allowed to be the first character.
      // API name must be at least one character.
      continue;
    }

    auto cap = new GMPCapability();

    if (tagsStart == -1) {
      // No tags.
      cap->mAPIName.Assign(api);
    } else {
      // We know the API name here.
      cap->mAPIName.Assign(Substring(api, 0, tagsStart));

      auto tagsEnd = api.FindChar(']');
      if (tagsEnd != -1 &&
          tagsEnd > tagsStart &&
          (tagsEnd - tagsStart) > 1) {
        nsAutoCString ts(Substring(api, tagsStart + 1, tagsEnd - tagsStart - 1));
        nsCCharSeparatedTokenizer tagTokens(ts, ':');
        while (tagTokens.hasMoreTokens()) {
          nsAutoCString tag(tagTokens.nextToken());
          cap->mAPITags.AppendElement(tag);
        }
      }
    }

    mCapabilities.AppendElement(cap);
  }
  if (mCapabilities.Length() < 1) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // namespace gmp
} // namespace mozilla
