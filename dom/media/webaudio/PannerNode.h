/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PannerNode_h_
#define PannerNode_h_

#include "AudioNode.h"
#include "mozilla/dom/PannerNodeBinding.h"
#include "ThreeDPoint.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/Preferences.h"
#include "WebAudioUtils.h"
#include <set>

namespace mozilla {
namespace dom {

class AudioContext;
class AudioBufferSourceNode;

class PannerNode final : public AudioNode,
                         public SupportsWeakPtr<PannerNode>
{
public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(PannerNode)
  explicit PannerNode(AudioContext* aContext);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual void DestroyMediaStream() override;

  virtual void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override
  {
    if (aChannelCount > 2) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCount(aChannelCount, aRv);
  }
  virtual void SetChannelCountModeValue(ChannelCountMode aMode, ErrorResult& aRv) override
  {
    if (aMode == ChannelCountMode::Max) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioNode::SetChannelCountModeValue(aMode, aRv);
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PannerNode, AudioNode)

  PanningModelType PanningModel() const
  {
    return mPanningModel;
  }
  void SetPanningModel(PanningModelType aPanningModel)
  {
    mPanningModel = aPanningModel;
    SendInt32ParameterToStream(PANNING_MODEL, int32_t(mPanningModel));
  }

  DistanceModelType DistanceModel() const
  {
    return mDistanceModel;
  }
  void SetDistanceModel(DistanceModelType aDistanceModel)
  {
    mDistanceModel = aDistanceModel;
    SendInt32ParameterToStream(DISTANCE_MODEL, int32_t(mDistanceModel));
  }

  void SetPosition(double aX, double aY, double aZ)
  {
    if (WebAudioUtils::FuzzyEqual(mPosition.x, aX) &&
        WebAudioUtils::FuzzyEqual(mPosition.y, aY) &&
        WebAudioUtils::FuzzyEqual(mPosition.z, aZ)) {
      return;
    }
    mPosition.x = aX;
    mPosition.y = aY;
    mPosition.z = aZ;
    SendThreeDPointParameterToStream(POSITION, mPosition);
  }

  void SetOrientation(double aX, double aY, double aZ)
  {
    ThreeDPoint orientation(aX, aY, aZ);
    if (!orientation.IsZero()) {
      orientation.Normalize();
    }
    if (mOrientation.FuzzyEqual(orientation)) {
      return;
    }
    mOrientation = orientation;
    SendThreeDPointParameterToStream(ORIENTATION, mOrientation);
  }

  void SetVelocity(double aX, double aY, double aZ)
  {
    if (WebAudioUtils::FuzzyEqual(mVelocity.x, aX) &&
        WebAudioUtils::FuzzyEqual(mVelocity.y, aY) &&
        WebAudioUtils::FuzzyEqual(mVelocity.z, aZ)) {
      return;
    }
    mVelocity.x = aX;
    mVelocity.y = aY;
    mVelocity.z = aZ;
    SendThreeDPointParameterToStream(VELOCITY, mVelocity);
    SendDopplerToSourcesIfNeeded();
  }

  double RefDistance() const
  {
    return mRefDistance;
  }
  void SetRefDistance(double aRefDistance)
  {
    if (WebAudioUtils::FuzzyEqual(mRefDistance, aRefDistance)) {
      return;
    }
    mRefDistance = aRefDistance;
    SendDoubleParameterToStream(REF_DISTANCE, mRefDistance);
  }

  double MaxDistance() const
  {
    return mMaxDistance;
  }
  void SetMaxDistance(double aMaxDistance)
  {
    if (WebAudioUtils::FuzzyEqual(mMaxDistance, aMaxDistance)) {
      return;
    }
    mMaxDistance = aMaxDistance;
    SendDoubleParameterToStream(MAX_DISTANCE, mMaxDistance);
  }

  double RolloffFactor() const
  {
    return mRolloffFactor;
  }
  void SetRolloffFactor(double aRolloffFactor)
  {
    if (WebAudioUtils::FuzzyEqual(mRolloffFactor, aRolloffFactor)) {
      return;
    }
    mRolloffFactor = aRolloffFactor;
    SendDoubleParameterToStream(ROLLOFF_FACTOR, mRolloffFactor);
  }

  double ConeInnerAngle() const
  {
    return mConeInnerAngle;
  }
  void SetConeInnerAngle(double aConeInnerAngle)
  {
    if (WebAudioUtils::FuzzyEqual(mConeInnerAngle, aConeInnerAngle)) {
      return;
    }
    mConeInnerAngle = aConeInnerAngle;
    SendDoubleParameterToStream(CONE_INNER_ANGLE, mConeInnerAngle);
  }

  double ConeOuterAngle() const
  {
    return mConeOuterAngle;
  }
  void SetConeOuterAngle(double aConeOuterAngle)
  {
    if (WebAudioUtils::FuzzyEqual(mConeOuterAngle, aConeOuterAngle)) {
      return;
    }
    mConeOuterAngle = aConeOuterAngle;
    SendDoubleParameterToStream(CONE_OUTER_ANGLE, mConeOuterAngle);
  }

  double ConeOuterGain() const
  {
    return mConeOuterGain;
  }
  void SetConeOuterGain(double aConeOuterGain)
  {
    if (WebAudioUtils::FuzzyEqual(mConeOuterGain, aConeOuterGain)) {
      return;
    }
    mConeOuterGain = aConeOuterGain;
    SendDoubleParameterToStream(CONE_OUTER_GAIN, mConeOuterGain);
  }

  float ComputeDopplerShift();
  void SendDopplerToSourcesIfNeeded();
  void FindConnectedSources();
  void FindConnectedSources(AudioNode* aNode, nsTArray<AudioBufferSourceNode*>& aSources, std::set<AudioNode*>& aSeenNodes);

  virtual const char* NodeType() const override
  {
    return "PannerNode";
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~PannerNode();

private:
  friend class AudioListener;
  friend class PannerNodeEngine;
  enum EngineParameters {
    LISTENER_POSITION,
    LISTENER_FRONT_VECTOR, // unit length
    LISTENER_RIGHT_VECTOR, // unit length, orthogonal to LISTENER_FRONT_VECTOR
    LISTENER_VELOCITY,
    LISTENER_DOPPLER_FACTOR,
    LISTENER_SPEED_OF_SOUND,
    PANNING_MODEL,
    DISTANCE_MODEL,
    POSITION,
    ORIENTATION, // unit length or zero
    VELOCITY,
    REF_DISTANCE,
    MAX_DISTANCE,
    ROLLOFF_FACTOR,
    CONE_INNER_ANGLE,
    CONE_OUTER_ANGLE,
    CONE_OUTER_GAIN
  };

private:
  PanningModelType mPanningModel;
  DistanceModelType mDistanceModel;
  ThreeDPoint mPosition;
  ThreeDPoint mOrientation;
  ThreeDPoint mVelocity;
  double mRefDistance;
  double mMaxDistance;
  double mRolloffFactor;
  double mConeInnerAngle;
  double mConeOuterAngle;
  double mConeOuterGain;

  // An array of all the AudioBufferSourceNode connected directly or indirectly
  // to this AudioPannerNode.
  nsTArray<AudioBufferSourceNode*> mSources;
};

}
}

#endif

