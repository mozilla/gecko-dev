/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTSENDERRECEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTSENDERRECEIVER_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/test/channel_transport/udp_transport.h"
#include "webrtc/typedefs.h"

class TestLoadGenerator;
namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
}

using namespace webrtc;

#define MAX_BITRATE_KBPS 50000


class SendRecCB
{
public:
    virtual void OnOnNetworkChanged(const uint32_t bitrateTarget,
        const uint8_t fractionLost,
        const uint16_t roundTripTimeMs,
        const uint16_t bwEstimateKbitMin,
        const uint16_t bwEstimateKbitMax) = 0;

    virtual ~SendRecCB() {};
};


class TestSenderReceiver : public RtpFeedback, public RtpData, public UdpTransportData, public RtpVideoFeedback
{

public:
    TestSenderReceiver (void);

    ~TestSenderReceiver (void);

    void SetCallback (SendRecCB *cb) { _sendRecCB = cb; };

    int32_t Start();

    int32_t Stop();

    bool ProcLoop();

    /////////////////////////////////////////////
    // Receiver methods

    int32_t InitReceiver (const uint16_t rtpPort,
                          const uint16_t rtcpPort = 0,
                          const int8_t payloadType = 127);

    int32_t ReceiveBitrateKbps ();

    int32_t SetPacketTimeout(const uint32_t timeoutMS);

    // Inherited from RtpFeedback
    int32_t OnInitializeDecoder(const int32_t id,
                                const int8_t payloadType,
                                const int8_t payloadName[RTP_PAYLOAD_NAME_SIZE],
                                const uint32_t frequency,
                                const uint8_t channels,
                                const uint32_t rate) override {
      return 0;
    }

    void OnIncomingSSRCChanged(const int32_t id, const uint32_t SSRC) override {
    }

    void OnIncomingCSRCChanged(const int32_t id,
                               const uint32_t CSRC,
                               const bool added) override {}

    // Inherited from RtpData
    int32_t OnReceivedPayloadData(
        const uint8_t* payloadData,
        const size_t payloadSize,
        const webrtc::WebRtcRTPHeader* rtpHeader) override;

    // Inherited from UdpTransportData
    void IncomingRTPPacket(const int8_t* incomingRtpPacket,
                           const size_t rtpPacketLength,
                           const int8_t* fromIP,
                           const uint16_t fromPort) override;

    void IncomingRTCPPacket(const int8_t* incomingRtcpPacket,
                            const size_t rtcpPacketLength,
                            const int8_t* fromIP,
                            const uint16_t fromPort) override;

    /////////////////////////////////
    // Sender methods

    int32_t InitSender (const uint32_t startBitrateKbps,
                        const int8_t* ipAddr,
                        const uint16_t rtpPort,
                        const uint16_t rtcpPort = 0,
                        const int8_t payloadType = 127);

    int32_t SendOutgoingData(const uint32_t timeStamp,
        const uint8_t* payloadData,
        const size_t payloadSize,
        const webrtc::FrameType frameType = webrtc::kVideoFrameDelta);

    int32_t SetLoadGenerator(TestLoadGenerator *generator);

    uint32_t BitrateSent() { return (_rtp->BitrateSent()); };


    // Inherited from RtpVideoFeedback
    virtual void OnReceivedIntraFrameRequest(const int32_t id,
        const uint8_t message = 0) {};

    virtual void OnNetworkChanged(const int32_t id,
                                  const uint32_t minBitrateBps,
                                  const uint32_t maxBitrateBps,
                                  const uint8_t fractionLost,
                                  const uint16_t roundTripTimeMs,
                                  const uint16_t bwEstimateKbitMin,
                                  const uint16_t bwEstimateKbitMax);

private:
    RtpRtcp* _rtp;
    UdpTransport* _transport;
    webrtc::CriticalSectionWrapper* _critSect;
    webrtc::EventWrapper *_eventPtr;
    rtc::scoped_ptr<webrtc::ThreadWrapper> _procThread;
    bool _running;
    int8_t _payloadType;
    TestLoadGenerator* _loadGenerator;
    bool _isSender;
    bool _isReceiver;
    SendRecCB * _sendRecCB;
    size_t _lastBytesReceived;
    int64_t _lastTime;

};

#endif // WEBRTC_MODULES_RTP_RTCP_TEST_BWESTANDALONE_TESTSENDERRECEIVER_H_
