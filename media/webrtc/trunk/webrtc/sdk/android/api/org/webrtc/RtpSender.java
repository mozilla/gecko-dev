/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/** Java wrapper for a C++ RtpSenderInterface. */
public class RtpSender {
  final long nativeRtpSender;

  private MediaStreamTrack cachedTrack;
  private boolean ownsTrack = true;

  private final DtmfSender dtmfSender;

  public RtpSender(long nativeRtpSender) {
    this.nativeRtpSender = nativeRtpSender;
    long track = nativeGetTrack(nativeRtpSender);
    // It may be possible for an RtpSender to be created without a track.
    cachedTrack = (track != 0) ? new MediaStreamTrack(track) : null;

    long nativeDtmfSender = nativeGetDtmfSender(nativeRtpSender);
    dtmfSender = (nativeDtmfSender != 0) ? new DtmfSender(nativeDtmfSender) : null;
  }

  /**
   * Starts sending a new track, without requiring additional SDP negotiation.
   * <p>
   * Note: This is equivalent to replaceTrack in the official WebRTC API. It
   * was just implemented before the standards group settled on a name.
   *
   * @param takeOwnership If true, the RtpSender takes ownership of the track
   *                      from the caller, and will auto-dispose of it when no
   *                      longer needed. |takeOwnership| should only be used if
   *                      the caller owns the track; it is not appropriate when
   *                      the track is owned by, for example, another RtpSender
   *                      or a MediaStream.
   * @return              true on success and false on failure.
   */
  public boolean setTrack(MediaStreamTrack track, boolean takeOwnership) {
    if (!nativeSetTrack(nativeRtpSender, (track == null) ? 0 : track.nativeTrack)) {
      return false;
    }
    if (cachedTrack != null && ownsTrack) {
      cachedTrack.dispose();
    }
    cachedTrack = track;
    ownsTrack = takeOwnership;
    return true;
  }

  public MediaStreamTrack track() {
    return cachedTrack;
  }

  public boolean setParameters(RtpParameters parameters) {
    return nativeSetParameters(nativeRtpSender, parameters);
  }

  public RtpParameters getParameters() {
    return nativeGetParameters(nativeRtpSender);
  }

  public String id() {
    return nativeId(nativeRtpSender);
  }

  public DtmfSender dtmf() {
    return dtmfSender;
  }

  public void dispose() {
    if (dtmfSender != null) {
      dtmfSender.dispose();
    }
    if (cachedTrack != null && ownsTrack) {
      cachedTrack.dispose();
    }
    JniCommon.nativeReleaseRef(nativeRtpSender);
  }

  private static native boolean nativeSetTrack(long nativeRtpSender, long nativeTrack);

  // This should increment the reference count of the track.
  // Will be released in dispose() or setTrack().
  private static native long nativeGetTrack(long nativeRtpSender);

  // This should increment the reference count of the DTMF sender.
  // Will be released in dispose().
  private static native long nativeGetDtmfSender(long nativeRtpSender);

  private static native boolean nativeSetParameters(long nativeRtpSender, RtpParameters parameters);

  private static native RtpParameters nativeGetParameters(long nativeRtpSender);

  private static native String nativeId(long nativeRtpSender);
};
