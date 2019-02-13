/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DynamicsCompressorNode_h_
#define DynamicsCompressorNode_h_

#include "AudioNode.h"
#include "AudioParam.h"

namespace mozilla {
namespace dom {

class AudioContext;

class DynamicsCompressorNode final : public AudioNode
{
public:
  explicit DynamicsCompressorNode(AudioContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DynamicsCompressorNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  AudioParam* Threshold() const
  {
    return mThreshold;
  }

  AudioParam* Knee() const
  {
    return mKnee;
  }

  AudioParam* Ratio() const
  {
    return mRatio;
  }

  AudioParam* Attack() const
  {
    return mAttack;
  }

  // Called GetRelease to prevent clashing with the nsISupports::Release name
  AudioParam* GetRelease() const
  {
    return mRelease;
  }

  float Reduction() const
  {
    return mReduction;
  }

  virtual const char* NodeType() const override
  {
    return "DynamicsCompressorNode";
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  void SetReduction(float aReduction)
  {
    MOZ_ASSERT(NS_IsMainThread());
    mReduction = aReduction;
  }

protected:
  virtual ~DynamicsCompressorNode();

private:
  static void SendThresholdToStream(AudioNode* aNode);
  static void SendKneeToStream(AudioNode* aNode);
  static void SendRatioToStream(AudioNode* aNode);
  static void SendAttackToStream(AudioNode* aNode);
  static void SendReleaseToStream(AudioNode* aNode);

private:
  nsRefPtr<AudioParam> mThreshold;
  nsRefPtr<AudioParam> mKnee;
  nsRefPtr<AudioParam> mRatio;
  float mReduction;
  nsRefPtr<AudioParam> mAttack;
  nsRefPtr<AudioParam> mRelease;
};

}
}

#endif

