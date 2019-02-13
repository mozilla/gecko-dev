/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_network_impl.h"

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc
{

VoENetwork* VoENetwork::GetInterface(VoiceEngine* voiceEngine)
{
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
    s->AddRef();
    return s;
}

VoENetworkImpl::VoENetworkImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoENetworkImpl() - ctor");
}

VoENetworkImpl::~VoENetworkImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "~VoENetworkImpl() - dtor");
}

int VoENetworkImpl::RegisterExternalTransport(int channel,
                                              Transport& transport)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetExternalTransport(channel=%d, transport=0x%x)",
                 channel, &transport);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetExternalTransport() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterExternalTransport(transport);
}

int VoENetworkImpl::DeRegisterExternalTransport(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DeRegisterExternalTransport(channel=%d)", channel);
    if (!_shared->statistics().Initialized())
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
                     VoEId(_shared->instance_id(), -1),
                     "DeRegisterExternalTransport() - invalid state");
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "DeRegisterExternalTransport() failed to locate channel");
        return -1;
    }
    return channelPtr->DeRegisterExternalTransport();
}

int VoENetworkImpl::ReceivedRTPPacket(int channel,
                                      const void* data,
                                      unsigned int length) {
  return ReceivedRTPPacket(channel, data, length, webrtc::PacketTime());
}

int VoENetworkImpl::ReceivedRTPPacket(int channel,
                                      const void* data,
                                      unsigned int length,
                                      const PacketTime& packet_time)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ReceivedRTPPacket(channel=%d, length=%u)", channel, length);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    // L16 at 32 kHz, stereo, 10 ms frames (+12 byte RTP header) -> 1292 bytes
    if ((length < 12) || (length > 1292))
    {
        _shared->SetLastError(VE_INVALID_PACKET);
        LOG(LS_ERROR) << "Invalid packet length: " << length;
        return -1;
    }
    if (NULL == data)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "ReceivedRTPPacket() invalid data vector");
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "ReceivedRTPPacket() failed to locate channel");
        return -1;
    }

    if (!channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "ReceivedRTPPacket() external transport is not enabled");
        return -1;
    }
    return channelPtr->ReceivedRTPPacket((const int8_t*) data, length,
                                         packet_time);
}

int VoENetworkImpl::ReceivedRTCPPacket(int channel, const void* data,
                                       unsigned int length)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "ReceivedRTCPPacket(channel=%d, length=%u)", channel, length);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (length < 4)
    {
        _shared->SetLastError(VE_INVALID_PACKET, kTraceError,
            "ReceivedRTCPPacket() invalid packet length");
        return -1;
    }
    if (NULL == data)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "ReceivedRTCPPacket() invalid data vector");
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "ReceivedRTCPPacket() failed to locate channel");
        return -1;
    }
    if (!channelPtr->ExternalTransport())
    {
        _shared->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "ReceivedRTCPPacket() external transport is not enabled");
        return -1;
    }
    return channelPtr->ReceivedRTCPPacket((const int8_t*) data, length);
}
}  // namespace webrtc
