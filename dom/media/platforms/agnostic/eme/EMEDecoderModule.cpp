/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EMEDecoderModule.h"
#include "EMEAudioDecoder.h"
#include "EMEVideoDecoder.h"
#include "MediaDataDecoderProxy.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/Unused.h"
#include "nsAutoPtr.h"
#include "nsServiceManagerUtils.h"
#include "MediaInfo.h"
#include "nsClassHashtable.h"
#include "GMPDecoderModule.h"
#include "MP4Decoder.h"

namespace mozilla {

typedef MozPromiseRequestHolder<CDMProxy::DecryptPromise> DecryptPromiseRequestHolder;

class EMEDecryptor : public MediaDataDecoder {

public:

  EMEDecryptor(MediaDataDecoder* aDecoder,
               MediaDataDecoderCallback* aCallback,
               CDMProxy* aProxy,
               TaskQueue* aDecodeTaskQueue)
    : mDecoder(aDecoder)
    , mCallback(aCallback)
    , mTaskQueue(aDecodeTaskQueue)
    , mProxy(aProxy)
    , mSamplesWaitingForKey(new SamplesWaitingForKey(this, this->mCallback,
                                                     mTaskQueue, mProxy))
    , mIsShutdown(false)
  {
  }

  RefPtr<InitPromise> Init() override {
    MOZ_ASSERT(!mIsShutdown);
    return mDecoder->Init();
  }

  void Input(MediaRawData* aSample) override {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    if (mIsShutdown) {
      NS_WARNING("EME encrypted sample arrived after shutdown");
      return;
    }
    if (mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)) {
      return;
    }

    nsAutoPtr<MediaRawDataWriter> writer(aSample->CreateWriter());
    mProxy->GetSessionIdsForKeyId(aSample->mCrypto.mKeyId,
                                  writer->mCrypto.mSessionIds);

    mDecrypts.Put(aSample, new DecryptPromiseRequestHolder());
    mDecrypts.Get(aSample)->Begin(mProxy->Decrypt(aSample)->Then(
      mTaskQueue, __func__, this,
      &EMEDecryptor::Decrypted,
      &EMEDecryptor::Decrypted));
    return;
  }

  void Decrypted(const DecryptResult& aDecrypted) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(aDecrypted.mSample);

    nsAutoPtr<DecryptPromiseRequestHolder> holder;
    mDecrypts.RemoveAndForget(aDecrypted.mSample, holder);
    if (holder) {
      holder->Complete();
    } else {
      // Decryption is not in the list of decrypt operations waiting
      // for a result. It must have been flushed or drained. Ignore result.
      return;
    }

    if (mIsShutdown) {
      NS_WARNING("EME decrypted sample arrived after shutdown");
      return;
    }

    if (aDecrypted.mStatus == NoKeyErr) {
      // Key became unusable after we sent the sample to CDM to decrypt.
      // Call Input() again, so that the sample is enqueued for decryption
      // if the key becomes usable again.
      Input(aDecrypted.mSample);
    } else if (aDecrypted.mStatus != Ok) {
      if (mCallback) {
        mCallback->Error(MediaResult(
          NS_ERROR_DOM_MEDIA_FATAL_ERR,
          RESULT_DETAIL("decrypted.mStatus=%u", uint32_t(aDecrypted.mStatus))));
      }
    } else {
      MOZ_ASSERT(!mIsShutdown);
      // The Adobe GMP AAC decoder gets confused if we pass it non-encrypted
      // samples with valid crypto data. So clear the crypto data, since the
      // sample should be decrypted now anyway. If we don't do this and we're
      // using the Adobe GMP for unencrypted decoding of data that is decrypted
      // by gmp-clearkey, decoding will fail.
      UniquePtr<MediaRawDataWriter> writer(aDecrypted.mSample->CreateWriter());
      writer->mCrypto = CryptoSample();
      mDecoder->Input(aDecrypted.mSample);
    }
  }

  void Flush() override {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    for (auto iter = mDecrypts.Iter(); !iter.Done(); iter.Next()) {
      nsAutoPtr<DecryptPromiseRequestHolder>& holder = iter.Data();
      holder->DisconnectIfExists();
      iter.Remove();
    }
    mDecoder->Flush();
    mSamplesWaitingForKey->Flush();
  }

  void Drain() override {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    for (auto iter = mDecrypts.Iter(); !iter.Done(); iter.Next()) {
      nsAutoPtr<DecryptPromiseRequestHolder>& holder = iter.Data();
      holder->DisconnectIfExists();
      iter.Remove();
    }
    mDecoder->Drain();
  }

  void Shutdown() override {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    mIsShutdown = true;
    mDecoder->Shutdown();
    mSamplesWaitingForKey->BreakCycles();
    mSamplesWaitingForKey = nullptr;
    mDecoder = nullptr;
    mProxy = nullptr;
    mCallback = nullptr;
  }

  const char* GetDescriptionName() const override {
    return mDecoder->GetDescriptionName();
  }

private:

  RefPtr<MediaDataDecoder> mDecoder;
  MediaDataDecoderCallback* mCallback;
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<CDMProxy> mProxy;
  nsClassHashtable<nsRefPtrHashKey<MediaRawData>, DecryptPromiseRequestHolder> mDecrypts;
  RefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
  bool mIsShutdown;
};

class EMEMediaDataDecoderProxy : public MediaDataDecoderProxy {
public:
  EMEMediaDataDecoderProxy(already_AddRefed<AbstractThread> aProxyThread,
                           MediaDataDecoderCallback* aCallback,
                           CDMProxy* aProxy,
                           TaskQueue* aTaskQueue)
   : MediaDataDecoderProxy(Move(aProxyThread), aCallback)
   , mSamplesWaitingForKey(new SamplesWaitingForKey(this, aCallback,
                                                    aTaskQueue, aProxy))
   , mProxy(aProxy)
  {
  }

  void Input(MediaRawData* aSample) override;
  void Shutdown() override;

private:
  RefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
  RefPtr<CDMProxy> mProxy;
};

void
EMEMediaDataDecoderProxy::Input(MediaRawData* aSample)
{
  if (mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)) {
    return;
  }

  nsAutoPtr<MediaRawDataWriter> writer(aSample->CreateWriter());
  mProxy->GetSessionIdsForKeyId(aSample->mCrypto.mKeyId,
                                writer->mCrypto.mSessionIds);

  MediaDataDecoderProxy::Input(aSample);
}

void
EMEMediaDataDecoderProxy::Shutdown()
{
  MediaDataDecoderProxy::Shutdown();

  mSamplesWaitingForKey->BreakCycles();
  mSamplesWaitingForKey = nullptr;
  mProxy = nullptr;
}

EMEDecoderModule::EMEDecoderModule(CDMProxy* aProxy, PDMFactory* aPDM)
  : mProxy(aProxy)
  , mPDM(aPDM)
{
}

EMEDecoderModule::~EMEDecoderModule()
{
}

static already_AddRefed<MediaDataDecoderProxy>
CreateDecoderWrapper(MediaDataDecoderCallback* aCallback, CDMProxy* aProxy, TaskQueue* aTaskQueue)
{
  RefPtr<gmp::GeckoMediaPluginService> s(gmp::GeckoMediaPluginService::GetGeckoMediaPluginService());
  if (!s) {
    return nullptr;
  }
  RefPtr<AbstractThread> thread(s->GetAbstractGMPThread());
  if (!thread) {
    return nullptr;
  }
  RefPtr<MediaDataDecoderProxy> decoder(
    new EMEMediaDataDecoderProxy(thread.forget(), aCallback, aProxy, aTaskQueue));
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  MOZ_ASSERT(aParams.mConfig.mCrypto.mValid);

  if (SupportsMimeType(aParams.mConfig.mMimeType, nullptr)) {
    // GMP decodes. Assume that means it can decrypt too.
    RefPtr<MediaDataDecoderProxy> wrapper =
      CreateDecoderWrapper(aParams.mCallback, mProxy, aParams.mTaskQueue);
    auto params = GMPVideoDecoderParams(aParams).WithCallback(wrapper);
    wrapper->SetProxyTarget(new EMEVideoDecoder(mProxy, params));
    return wrapper.forget();
  }

  MOZ_ASSERT(mPDM);
  RefPtr<MediaDataDecoder> decoder(mPDM->CreateDecoder(aParams));
  if (!decoder) {
    return nullptr;
  }

  RefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                       aParams.mCallback,
                                                       mProxy,
                                                       AbstractThread::GetCurrent()->AsTaskQueue()));
  return emeDecoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateAudioDecoder(const CreateDecoderParams& aParams)
{
  MOZ_ASSERT(aParams.mConfig.mCrypto.mValid);

  if (SupportsMimeType(aParams.mConfig.mMimeType, nullptr)) {
    // GMP decodes. Assume that means it can decrypt too.
    RefPtr<MediaDataDecoderProxy> wrapper =
      CreateDecoderWrapper(aParams.mCallback, mProxy, aParams.mTaskQueue);
    auto gmpParams = GMPAudioDecoderParams(aParams).WithCallback(wrapper);
    wrapper->SetProxyTarget(new EMEAudioDecoder(mProxy, gmpParams));
    return wrapper.forget();
  }

  MOZ_ASSERT(mPDM);
  RefPtr<MediaDataDecoder> decoder(mPDM->CreateDecoder(aParams));
  if (!decoder) {
    return nullptr;
  }

  RefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                       aParams.mCallback,
                                                       mProxy,
                                                       AbstractThread::GetCurrent()->AsTaskQueue()));
  return emeDecoder.forget();
}

PlatformDecoderModule::ConversionRequired
EMEDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  if (aConfig.IsVideo() && MP4Decoder::IsH264(aConfig.mMimeType)) {
    return ConversionRequired::kNeedAVCC;
  } else {
    return ConversionRequired::kNeedNone;
  }
}

bool
EMEDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                   DecoderDoctorDiagnostics* aDiagnostics) const
{
  Maybe<nsCString> gmp;
  gmp.emplace(NS_ConvertUTF16toUTF8(mProxy->KeySystem()));
  return GMPDecoderModule::SupportsMimeType(aMimeType, gmp);
}

} // namespace mozilla
