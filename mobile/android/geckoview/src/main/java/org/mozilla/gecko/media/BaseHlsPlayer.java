/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import java.util.concurrent.ConcurrentLinkedQueue;

public interface BaseHlsPlayer {

    public enum TrackType {
        UNDEFINED,
        AUDIO,
        VIDEO,
        TEXT,
    }

    public enum ResourceError {
        BASE(-100),
        UNKNOWN(-101),
        PLAYER(-102),
        UNSUPPORTED(-103);

        int mNumVal;
        private ResourceError(int numVal) {
            mNumVal = numVal;
        }
        public int code() {
            return mNumVal;
        }
    }

    public enum DemuxerError {
        BASE(-200),
        UNKNOWN(-201),
        PLAYER(-202),
        UNSUPPORTED(-203);

        int mNumVal;
        DemuxerError(int numVal) {
            mNumVal = numVal;
        }
        public int code() {
            return mNumVal;
        }
    }

    public interface DemuxerCallbacks {
        void onInitialized(boolean hasAudio, boolean hasVideo);
        void onError(int errorCode);
    }

    public interface ResourceCallbacks {
        void onDataArrived();
        void onError(int errorCode);
    }

    // Used to identify player instance.
    int getId();

    // =======================================================================
    // API for GeckoHLSResourceWrapper
    // =======================================================================
    void init(String url, ResourceCallbacks callback);

    boolean isLiveStream();

    // =======================================================================
    // API for GeckoHLSDemuxerWrapper
    // =======================================================================
    void addDemuxerWrapperCallbackListener(DemuxerCallbacks callback);

    ConcurrentLinkedQueue<GeckoHLSSample> getSamples(TrackType trackType, int number);

    long getBufferedPosition();

    int getNumberOfTracks(TrackType trackType);

    GeckoVideoInfo getVideoInfo(int index);

    GeckoAudioInfo getAudioInfo(int index);

    boolean seek(long positionUs);

    long getNextKeyFrameTime();

    void suspend();

    void resume();

    void play();

    void pause();

    void release();
}