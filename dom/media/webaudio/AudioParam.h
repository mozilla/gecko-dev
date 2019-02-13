/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioParam_h_
#define AudioParam_h_

#include "AudioParamTimeline.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "AudioNode.h"
#include "mozilla/dom/TypedArray.h"
#include "WebAudioUtils.h"
#include "js/TypeDecls.h"

namespace mozilla {

namespace dom {

class AudioParam final : public nsWrapperCache,
                         public AudioParamTimeline
{
  virtual ~AudioParam();

public:
  typedef void (*CallbackType)(AudioNode*);

  AudioParam(AudioNode* aNode,
             CallbackType aCallback,
             float aDefaultValue,
             const char* aName);

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(MozExternalRefCountType) Release(void);
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AudioParam)

  AudioContext* GetParentObject() const
  {
    return mNode->Context();
  }

  double DOMTimeToStreamTime(double aTime) const
  {
    return mNode->Context()->DOMTimeToStreamTime(aTime);
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // We override SetValueCurveAtTime to convert the Float32Array to the wrapper
  // object.
  void SetValueCurveAtTime(const Float32Array& aValues, double aStartTime, double aDuration, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    aValues.ComputeLengthAndData();
    AudioParamTimeline::SetValueCurveAtTime(aValues.Data(), aValues.Length(),
                                            DOMTimeToStreamTime(aStartTime), aDuration, aRv);
    mCallback(mNode);
  }

  // We override the rest of the mutating AudioParamTimeline methods in order to make
  // sure that the callback is called every time that this object gets mutated.
  void SetValue(float aValue)
  {
    // Optimize away setting the same value on an AudioParam
    if (HasSimpleValue() &&
        WebAudioUtils::FuzzyEqual(GetValue(), aValue)) {
      return;
    }
    AudioParamTimeline::SetValue(aValue);
    mCallback(mNode);
  }
  void SetValueAtTime(float aValue, double aStartTime, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioParamTimeline::SetValueAtTime(aValue, DOMTimeToStreamTime(aStartTime), aRv);
    mCallback(mNode);
  }
  void LinearRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aEndTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioParamTimeline::LinearRampToValueAtTime(aValue, DOMTimeToStreamTime(aEndTime), aRv);
    mCallback(mNode);
  }
  void ExponentialRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aEndTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioParamTimeline::ExponentialRampToValueAtTime(aValue, DOMTimeToStreamTime(aEndTime), aRv);
    mCallback(mNode);
  }
  void SetTargetAtTime(float aTarget, double aStartTime, double aTimeConstant, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aStartTime) ||
        !WebAudioUtils::IsTimeValid(aTimeConstant)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioParamTimeline::SetTargetAtTime(aTarget, DOMTimeToStreamTime(aStartTime), aTimeConstant, aRv);
    mCallback(mNode);
  }
  void CancelScheduledValues(double aStartTime, ErrorResult& aRv)
  {
    if (!WebAudioUtils::IsTimeValid(aStartTime)) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
    AudioParamTimeline::CancelScheduledValues(DOMTimeToStreamTime(aStartTime));
    mCallback(mNode);
  }

  uint32_t ParentNodeId()
  {
    return mNode->Id();
  }

  void GetName(nsAString& aName)
  {
    aName.AssignASCII(mName);
  }

  float DefaultValue() const
  {
    return mDefaultValue;
  }

  AudioNode* Node() const
  {
    return mNode;
  }

  const nsTArray<AudioNode::InputNode>& InputNodes() const
  {
    return mInputNodes;
  }

  void RemoveInputNode(uint32_t aIndex)
  {
    mInputNodes.RemoveElementAt(aIndex);
  }

  AudioNode::InputNode* AppendInputNode()
  {
    return mInputNodes.AppendElement();
  }

  void DisconnectFromGraphAndDestroyStream();

  // May create the stream if it doesn't exist
  MediaStream* Stream();

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = AudioParamTimeline::SizeOfExcludingThis(aMallocSizeOf);
    // Not owned:
    // - mNode

    // Just count the array, actual nodes are counted in mNode.
    amount += mInputNodes.SizeOfExcludingThis(aMallocSizeOf);

    if (mNodeStreamPort) {
      amount += mNodeStreamPort->SizeOfIncludingThis(aMallocSizeOf);
    }

    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  nsCycleCollectingAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

private:
  nsRefPtr<AudioNode> mNode;
  // For every InputNode, there is a corresponding entry in mOutputParams of the
  // InputNode's mInputNode.
  nsTArray<AudioNode::InputNode> mInputNodes;
  CallbackType mCallback;
  const float mDefaultValue;
  const char* mName;
  // The input port used to connect the AudioParam's stream to its node's stream
  nsRefPtr<MediaInputPort> mNodeStreamPort;
};

}
}

#endif

