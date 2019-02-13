/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_MEDIA_STREAM_H_
#define FAKE_MEDIA_STREAM_H_

#include <set>
#include <string>
#include <sstream>

#include "nsNetCID.h"
#include "nsITimer.h"
#include "nsComponentManagerUtils.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsISupportsImpl.h"
#include "nsServiceManagerUtils.h"

// #includes from MediaStream.h
#include "mozilla/Mutex.h"
#include "AudioSegment.h"
#include "MediaSegment.h"
#include "StreamBuffer.h"
#include "nsTArray.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"

class nsIDOMWindow;

namespace mozilla {
   class MediaStreamGraph;
   class MediaStreamGraphImpl;
   class MediaSegment;
};

class Fake_VideoSink {
public:
  Fake_VideoSink() {}
  virtual void SegmentReady(mozilla::MediaSegment* aSegment) = 0;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_VideoSink)
protected:
  virtual ~Fake_VideoSink() {}
};

class Fake_SourceMediaStream;

class Fake_MediaStreamListener
{
protected:
  virtual ~Fake_MediaStreamListener() {}

public:
  virtual void NotifyQueuedTrackChanges(mozilla::MediaStreamGraph* aGraph, mozilla::TrackID aID,
                                        mozilla::StreamTime aTrackOffset,
                                        uint32_t aTrackEvents,
                                        const mozilla::MediaSegment& aQueuedMedia)  = 0;
  virtual void NotifyPull(mozilla::MediaStreamGraph* aGraph, mozilla::StreamTime aDesiredTime) = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStreamListener)
};

class Fake_MediaStreamDirectListener : public Fake_MediaStreamListener
{
protected:
  virtual ~Fake_MediaStreamDirectListener() {}

public:
  virtual void NotifyRealtimeData(mozilla::MediaStreamGraph* graph, mozilla::TrackID tid,
                                  mozilla::StreamTime offset,
                                  uint32_t events,
                                  const mozilla::MediaSegment& media) = 0;
};

// Note: only one listener supported
class Fake_MediaStream {
 protected:
  virtual ~Fake_MediaStream() { Stop(); }

 public:
  Fake_MediaStream () : mListeners(), mMutex("Fake MediaStream") {}

  static uint32_t GraphRate() { return 16000; }

  void AddListener(Fake_MediaStreamListener *aListener) {
    mozilla::MutexAutoLock lock(mMutex);
    mListeners.insert(aListener);
  }

  void RemoveListener(Fake_MediaStreamListener *aListener) {
    mozilla::MutexAutoLock lock(mMutex);
    mListeners.erase(aListener);
  }

  void NotifyPull(mozilla::MediaStreamGraph* graph,
                  mozilla::StreamTime aDesiredTime) {

    mozilla::MutexAutoLock lock(mMutex);
    std::set<Fake_MediaStreamListener *>::iterator it;
    for (it = mListeners.begin(); it != mListeners.end(); ++it) {
      (*it)->NotifyPull(graph, aDesiredTime);
    }
  }

  virtual Fake_SourceMediaStream *AsSourceStream() { return nullptr; }

  virtual mozilla::MediaStreamGraphImpl *GraphImpl() { return nullptr; }
  virtual nsresult Start() { return NS_OK; }
  virtual nsresult Stop() { return NS_OK; }
  virtual void StopStream() {}

  virtual void Periodic() {}

  double StreamTimeToSeconds(mozilla::StreamTime aTime);
  mozilla::StreamTime
  TicksToTimeRoundDown(mozilla::TrackRate aRate, mozilla::TrackTicks aTicks);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStream);

 protected:
  std::set<Fake_MediaStreamListener *> mListeners;
  mozilla::Mutex mMutex;  // Lock to prevent the listener list from being modified while
                          // executing Periodic().
};

class Fake_MediaPeriodic : public nsITimerCallback {
public:
  explicit Fake_MediaPeriodic(Fake_MediaStream *aStream) : mStream(aStream),
                                                           mCount(0) {}
  void Detach() {
    mStream = nullptr;
  }

  int GetTimesCalled() { return mCount; }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

protected:
  virtual ~Fake_MediaPeriodic() {}

  Fake_MediaStream *mStream;
  int mCount;
};


class Fake_SourceMediaStream : public Fake_MediaStream {
 public:
  Fake_SourceMediaStream() : mSegmentsAdded(0),
                             mDesiredTime(0),
                             mPullEnabled(false),
                             mStop(false),
                             mPeriodic(new Fake_MediaPeriodic(this)) {}

  enum {
    ADDTRACK_QUEUED    = 0x01 // Queue track add until FinishAddTracks()
  };

  void AddVideoSink(const nsRefPtr<Fake_VideoSink>& aSink) {
    mSink  = aSink;
  }

  void AddTrack(mozilla::TrackID aID, mozilla::StreamTime aStart,
                mozilla::MediaSegment* aSegment, uint32_t aFlags = 0) {
    delete aSegment;
  }
  void AddAudioTrack(mozilla::TrackID aID, mozilla::TrackRate aRate, mozilla::StreamTime aStart,
                     mozilla::AudioSegment* aSegment, uint32_t aFlags = 0) {
    delete aSegment;
  }
  void FinishAddTracks() {}
  void EndTrack(mozilla::TrackID aID) {}

  bool AppendToTrack(mozilla::TrackID aID, mozilla::MediaSegment* aSegment,
                     mozilla::MediaSegment *aRawSegment) {
    return AppendToTrack(aID, aSegment);
  }

  bool AppendToTrack(mozilla::TrackID aID, mozilla::MediaSegment* aSegment) {
    bool nonZeroSample = false;
    MOZ_ASSERT(aSegment);
    if(aSegment->GetType() == mozilla::MediaSegment::AUDIO) {
      //On audio segment append, we verify for validity
      //of the audio samples.
      mozilla::AudioSegment* audio =
              static_cast<mozilla::AudioSegment*>(aSegment);
      mozilla::AudioSegment::ChunkIterator iter(*audio);
      while(!iter.IsEnded()) {
        mozilla::AudioChunk& chunk = *(iter);
        MOZ_ASSERT(chunk.mBuffer);
        const int16_t* buf =
          static_cast<const int16_t*>(chunk.mChannelData[0]);
        for(int i=0; i<chunk.mDuration; i++) {
          if(buf[i]) {
            //atleast one non-zero sample found.
            nonZeroSample = true;
            break;
          }
        }
        //process next chunk
        iter.Next();
      }
      if(nonZeroSample) {
          //we increment segments count if
          //atleast one non-zero samples was found.
          ++mSegmentsAdded;
      }
    } else {
      //in the case of video segment appended, we just increase the
      //segment count.
      if (mSink.get()) {
        mSink->SegmentReady(aSegment);
      }
      ++mSegmentsAdded;
    }
    return true;
  }

  void AdvanceKnownTracksTime(mozilla::StreamTime aKnownTime) {}

  void SetPullEnabled(bool aEnabled) {
    mPullEnabled = aEnabled;
  }
  void AddDirectListener(Fake_MediaStreamListener* aListener) {}
  void RemoveDirectListener(Fake_MediaStreamListener* aListener) {}

  //Don't pull anymore data,if mStop is true.
  virtual void StopStream() {
   mStop = true;
  }

  virtual Fake_SourceMediaStream *AsSourceStream() { return this; }

  virtual nsresult Start();
  virtual nsresult Stop();

  virtual void Periodic();

  virtual int GetSegmentsAdded() {
    return mSegmentsAdded;
  }

 protected:
  int mSegmentsAdded;
  uint64_t mDesiredTime;
  bool mPullEnabled;
  bool mStop;
  nsRefPtr<Fake_MediaPeriodic> mPeriodic;
  nsRefPtr<Fake_VideoSink> mSink;
  nsCOMPtr<nsITimer> mTimer;
};

class Fake_DOMMediaStream;

class Fake_MediaStreamTrack
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Fake_MediaStreamTrack)

  Fake_MediaStreamTrack(bool aIsVideo, Fake_DOMMediaStream* aStream) :
    mIsVideo (aIsVideo),
    mStream (aStream)
  {
    static size_t counter = 0;
    std::ostringstream os;
    os << counter++;
    mID = os.str();
  }

  mozilla::TrackID GetTrackID() { return mIsVideo ? 1 : 0; }
  std::string GetId() const { return mID; }
  void AssignId(const std::string& id) { mID = id; }
  Fake_DOMMediaStream *GetStream() { return mStream; }
  const Fake_MediaStreamTrack* AsVideoStreamTrack() const
  {
    return mIsVideo? this : nullptr;
  }
  const Fake_MediaStreamTrack* AsAudioStreamTrack() const
  {
    return mIsVideo? nullptr : this;
  }
  const uint32_t typeSize () const
  {
    return sizeof(Fake_MediaStreamTrack);
  }
  const char* typeName () const
  {
    return "Fake_MediaStreamTrack";
  }
private:
  ~Fake_MediaStreamTrack() {}

  const bool mIsVideo;
  Fake_DOMMediaStream* mStream;
  std::string mID;
};

class Fake_DOMMediaStream : public nsISupports
{
protected:
  virtual ~Fake_DOMMediaStream() {
    // Note: memory leak
    mMediaStream->Stop();
  }

public:
  explicit Fake_DOMMediaStream(Fake_MediaStream *stream = nullptr)
    : mMediaStream(stream ? stream : new Fake_MediaStream())
    , mVideoTrack(new Fake_MediaStreamTrack(true, this))
    , mAudioTrack(new Fake_MediaStreamTrack(false, this))
    {
      static size_t counter = 0;
      std::ostringstream os;
      os << counter++;
      mID = os.str();
    }

  NS_DECL_THREADSAFE_ISUPPORTS

  static already_AddRefed<Fake_DOMMediaStream>
  CreateSourceStream(nsIDOMWindow* aWindow, uint32_t aHintContents = 0) {
    Fake_SourceMediaStream *source = new Fake_SourceMediaStream();

    nsRefPtr<Fake_DOMMediaStream> ds = new Fake_DOMMediaStream(source);
    ds->SetHintContents(aHintContents);

    return ds.forget();
  }

  virtual void Stop() {} // Really DOMLocalMediaStream

  virtual bool AddDirectListener(Fake_MediaStreamListener *aListener) { return false; }
  virtual void RemoveDirectListener(Fake_MediaStreamListener *aListener) {}

  Fake_MediaStream *GetStream() { return mMediaStream; }
  std::string GetId() const { return mID; }
  void AssignId(const std::string& id) { mID = id; }

  // Hints to tell the SDP generator about whether this
  // MediaStream probably has audio and/or video
  typedef uint8_t TrackTypeHints;
  enum {
    HINT_CONTENTS_AUDIO = 0x01,
    HINT_CONTENTS_VIDEO = 0x02
  };
  uint32_t GetHintContents() const { return mHintContents; }
  void SetHintContents(uint32_t aHintContents) { mHintContents = aHintContents; }

  void
  GetTracks(nsTArray<nsRefPtr<Fake_MediaStreamTrack> >& aTracks)
  {
    GetAudioTracks(aTracks);
    GetVideoTracks(aTracks);
  }

  void GetAudioTracks(nsTArray<nsRefPtr<Fake_MediaStreamTrack> >& aTracks)
  {
    if (mHintContents & HINT_CONTENTS_AUDIO) {
      aTracks.AppendElement(mAudioTrack);
    }
  }

  void
  GetVideoTracks(nsTArray<nsRefPtr<Fake_MediaStreamTrack> >& aTracks)
  {
    if (mHintContents & HINT_CONTENTS_VIDEO) {
      aTracks.AppendElement(mVideoTrack);
    }
  }

  bool
  HasTrack(const Fake_MediaStreamTrack& aTrack) const
  {
    return ((mHintContents & HINT_CONTENTS_AUDIO) && aTrack.AsAudioStreamTrack()) ||
           ((mHintContents & HINT_CONTENTS_VIDEO) && aTrack.AsVideoStreamTrack());
  }

  void SetTrackEnabled(mozilla::TrackID aTrackID, bool aEnabled) {}

  class PrincipalChangeObserver
  {
  public:
    virtual void PrincipalChanged(Fake_DOMMediaStream* aMediaStream) = 0;
  };
  void AddPrincipalChangeObserver(void* ignoredObserver) {}
  void RemovePrincipalChangeObserver(void* ignoredObserver) {}

private:
  nsRefPtr<Fake_MediaStream> mMediaStream;

  // tells the SDP generator about whether this
  // MediaStream probably has audio and/or video
  uint32_t mHintContents;
  nsRefPtr<Fake_MediaStreamTrack> mVideoTrack;
  nsRefPtr<Fake_MediaStreamTrack> mAudioTrack;

  std::string mID;
};

class Fake_MediaStreamBase : public Fake_MediaStream {
 public:
  Fake_MediaStreamBase() : mPeriodic(new Fake_MediaPeriodic(this)) {}

  virtual nsresult Start();
  virtual nsresult Stop();

  virtual int GetSegmentsAdded() {
    return mPeriodic->GetTimesCalled();
  }

 private:
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<Fake_MediaPeriodic> mPeriodic;
};


class Fake_AudioStreamSource : public Fake_MediaStreamBase {
 public:
  Fake_AudioStreamSource() : Fake_MediaStreamBase(),
                             mCount(0),
                             mStop(false) {}
  //Signaling Agent indicates us to stop generating
  //further audio.
  void StopStream() {
    mStop = true;
  }
  virtual void Periodic();
  int mCount;
  bool mStop;
};

class Fake_VideoStreamSource : public Fake_MediaStreamBase {
 public:
  Fake_VideoStreamSource() : Fake_MediaStreamBase() {}
};


namespace mozilla {
typedef Fake_MediaStream MediaStream;
typedef Fake_SourceMediaStream SourceMediaStream;
typedef Fake_MediaStreamListener MediaStreamListener;
typedef Fake_MediaStreamDirectListener MediaStreamDirectListener;
typedef Fake_DOMMediaStream DOMMediaStream;
typedef Fake_DOMMediaStream DOMLocalMediaStream;
}

#endif
