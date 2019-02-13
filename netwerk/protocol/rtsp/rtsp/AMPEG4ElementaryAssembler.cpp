/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "AMPEG4ElementaryAssembler"
#include "RtspPrlog.h"

#include "AMPEG4ElementaryAssembler.h"

#include "ARTPSource.h"

#include "mozilla/Assertions.h"

#include <media/stagefright/foundation/ABitReader.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/Utils.h>

#include <ctype.h>
#include <stdint.h>

namespace android {

static bool GetAttribute(const char *s, const char *key, AString *value) {
    value->clear();

    size_t keyLen = strlen(key);

    for (;;) {
        while (isspace(*s)) {
            ++s;
        }

        const char *colonPos = strchr(s, ';');

        size_t len =
            (colonPos == NULL) ? strlen(s) : colonPos - s;

        if (len >= keyLen + 1 && s[keyLen] == '='
                && !strncasecmp(s, key, keyLen)) {
            value->setTo(&s[keyLen + 1], len - keyLen - 1);
            return true;
        }

        if (colonPos == NULL) {
            return false;
        }

        s = colonPos + 1;
    }
}

static bool GetIntegerAttribute(
        const char *s, const char *key, unsigned *x) {
    *x = 0;

    AString val;
    if (!GetAttribute(s, key, &val)) {
        return false;
    }

    s = val.c_str();
    char *end;
    unsigned y = strtoul(s, &end, 10);

    if (end == s || *end != '\0') {
        return false;
    }

    *x = y;

    return true;
}

// static
AMPEG4ElementaryAssembler::AMPEG4ElementaryAssembler(
        const sp<AMessage> &notify, const AString &desc, const AString &params)
    : mNotifyMsg(notify),
      mIsGeneric(false),
      mParams(params),
      mSizeLength(0),
      mIndexLength(0),
      mIndexDeltaLength(0),
      mCTSDeltaLength(0),
      mDTSDeltaLength(0),
      mRandomAccessIndication(false),
      mStreamStateIndication(0),
      mAuxiliaryDataSizeLength(0),
      mConstantDuration(0),
      mPreviousAUCount(0),
      mHasAUHeader(false),
      mAccessUnitRTPTime(0),
      mNextExpectedSeqNoValid(false),
      mNextExpectedSeqNo(0),
      mAccessUnitDamaged(false) {
    mIsGeneric = !strncasecmp(desc.c_str(),"mpeg4-generic/", 14);

    if (mIsGeneric) {
        AString value;
        CHECK(GetAttribute(params.c_str(), "mode", &value));

        if (!GetIntegerAttribute(params.c_str(), "sizeLength", &mSizeLength)) {
            mSizeLength = 0;
        }

        if (!GetIntegerAttribute(
                    params.c_str(), "indexLength", &mIndexLength)) {
            mIndexLength = 0;
        }

        if (!GetIntegerAttribute(
                    params.c_str(), "indexDeltaLength", &mIndexDeltaLength)) {
            mIndexDeltaLength = 0;
        }

        if (!GetIntegerAttribute(
                    params.c_str(), "CTSDeltaLength", &mCTSDeltaLength)) {
            mCTSDeltaLength = 0;
        }

        if (!GetIntegerAttribute(
                    params.c_str(), "DTSDeltaLength", &mDTSDeltaLength)) {
            mDTSDeltaLength = 0;
        }

        unsigned x;
        if (!GetIntegerAttribute(
                    params.c_str(), "randomAccessIndication", &x)) {
            mRandomAccessIndication = false;
        } else {
            CHECK(x == 0 || x == 1);
            mRandomAccessIndication = (x != 0);
        }

        if (!GetIntegerAttribute(
                    params.c_str(), "streamStateIndication",
                    &mStreamStateIndication)) {
            mStreamStateIndication = 0;
        }

        if (!GetIntegerAttribute(
                    params.c_str(), "auxiliaryDataSizeLength",
                    &mAuxiliaryDataSizeLength)) {
            mAuxiliaryDataSizeLength = 0;
        }

        if (!GetIntegerAttribute(
                    params.c_str(), "constantDuration",
                    &mConstantDuration)) {
            mConstantDuration = 0;
        }

        mHasAUHeader =
            mSizeLength > 0
            || mIndexLength > 0
            || mIndexDeltaLength > 0
            || mCTSDeltaLength > 0
            || mDTSDeltaLength > 0
            || mRandomAccessIndication
            || mStreamStateIndication > 0;
    }
}

AMPEG4ElementaryAssembler::~AMPEG4ElementaryAssembler() {
}

struct AUHeader {
    unsigned mSize;
    unsigned mSerial;
};

ARTPAssembler::AssemblyStatus AMPEG4ElementaryAssembler::addPacket(
        const sp<ARTPSource> &source) {
    List<sp<ABuffer> > *queue = source->queue();

    if (queue->empty()) {
        return NOT_ENOUGH_DATA;
    }

    if (mNextExpectedSeqNoValid) {
        List<sp<ABuffer> >::iterator it = queue->begin();
        while (it != queue->end()) {
            if ((uint32_t)(*it)->int32Data() >= mNextExpectedSeqNo) {
                break;
            }

            it = queue->erase(it);
        }

        if (queue->empty()) {
            return NOT_ENOUGH_DATA;
        }
    }

    sp<ABuffer> buffer = *queue->begin();

    if (!mNextExpectedSeqNoValid) {
        mNextExpectedSeqNoValid = true;
        mNextExpectedSeqNo = (uint32_t)buffer->int32Data();
    } else if ((uint32_t)buffer->int32Data() != mNextExpectedSeqNo) {
        LOGV("Not the sequence number I expected");

        return WRONG_SEQUENCE_NUMBER;
    }

    uint32_t rtpTime;
    if (!buffer->meta()->findInt32("rtp-time", (int32_t *)&rtpTime)) {
        LOGW("Cannot find rtp-time. Malformed packet.");

        return MALFORMED_PACKET;
    }

    if (mPackets.size() > 0 && rtpTime != mAccessUnitRTPTime) {
        if (!submitAccessUnit()) {
            LOGW("Cannot find rtp-time. Malformed packet.");

            return MALFORMED_PACKET;
        }
    }

    // If constantDuration and CTSDelta are not present. We should assume the
    // stream has fixed duration and calculate the mConstantDuration.
    if (!mConstantDuration && !mCTSDeltaLength && mPreviousAUCount
        && rtpTime > mAccessUnitRTPTime) {
        mConstantDuration = (rtpTime - mAccessUnitRTPTime) / mPreviousAUCount;
    }

    mAccessUnitRTPTime = rtpTime;

    if (!mIsGeneric) {
        mPackets.push_back(buffer);
    } else {
        // hexdump(buffer->data(), buffer->size());

        if (buffer->size() < 2u) {
            LOGW("Payload format error. Malformed packet.");

            return MALFORMED_PACKET;
        }
        unsigned AU_headers_length = U16_AT(buffer->data());  // in bits

        if (buffer->size() < 2 + (AU_headers_length + 7) / 8) {
            LOGW("Payload format error. Malformed packet.");

            return MALFORMED_PACKET;
        }

        List<AUHeader> headers;

        ABitReader bits(buffer->data() + 2, buffer->size() - 2);
        unsigned numBitsLeft = AU_headers_length;

        unsigned AU_serial = 0;
        for (;;) {
            if (numBitsLeft < mSizeLength) { break; }

            unsigned AU_size = bits.getBits(mSizeLength);
            numBitsLeft -= mSizeLength;

            size_t n = headers.empty() ? mIndexLength : mIndexDeltaLength;
            if (numBitsLeft < n) { break; }

            unsigned AU_index = bits.getBits(n);
            numBitsLeft -= n;

            if (headers.empty()) {
                AU_serial = AU_index;
            } else {
                AU_serial += 1 + AU_index;
            }

            if (mCTSDeltaLength > 0) {
                if (numBitsLeft < 1) {
                    break;
                }
                --numBitsLeft;
                if (bits.getBits(1)) {
                    if (numBitsLeft < mCTSDeltaLength) {
                        break;
                    }
                    bits.skipBits(mCTSDeltaLength);
                    numBitsLeft -= mCTSDeltaLength;
                }
            }

            if (mDTSDeltaLength > 0) {
                if (numBitsLeft < 1) {
                    break;
                }
                --numBitsLeft;
                if (bits.getBits(1)) {
                    if (numBitsLeft < mDTSDeltaLength) {
                        break;
                    }
                    bits.skipBits(mDTSDeltaLength);
                    numBitsLeft -= mDTSDeltaLength;
                }
            }

            if (mRandomAccessIndication) {
                if (numBitsLeft < 1) {
                    break;
                }
                bits.skipBits(1);
                --numBitsLeft;
            }

            if (mStreamStateIndication > 0) {
                if (numBitsLeft < mStreamStateIndication) {
                    break;
                }
                bits.skipBits(mStreamStateIndication);
            }

            AUHeader header;
            header.mSize = AU_size;
            header.mSerial = AU_serial;
            headers.push_back(header);
        }

        size_t offset = 2 + (AU_headers_length + 7) / 8;

        if (mAuxiliaryDataSizeLength > 0) {
            ABitReader bits(buffer->data() + offset, buffer->size() - offset);

            unsigned auxSize = bits.getBits(mAuxiliaryDataSizeLength);

            offset += (mAuxiliaryDataSizeLength + auxSize + 7) / 8;
        }

        mPreviousAUCount = 0;
        for (List<AUHeader>::iterator it = headers.begin();
             it != headers.end(); ++it) {
            mPreviousAUCount++;
            const AUHeader &header = *it;
            const AUHeader &first = *headers.begin();
            if (offset + header.mSize > buffer->size()) {
                LOGW("Payload format error. Malformed packet.");

                return MALFORMED_PACKET;
            }

            sp<ABuffer> accessUnit = new ABuffer(header.mSize);
            memcpy(accessUnit->data(), buffer->data() + offset, header.mSize);

            offset += header.mSize;

            int rtpTime = mAccessUnitRTPTime +
                          mConstantDuration * (header.mSerial - first.mSerial);
            accessUnit->meta()->setInt32("rtp-time", rtpTime);
            accessUnit->setInt32Data(buffer->int32Data());

            mPackets.push_back(accessUnit);
        }

        if (offset != buffer->size()) {
            LOGW("Payload format error. Malformed packet.");

            return MALFORMED_PACKET;
        }
    }

    queue->erase(queue->begin());
    ++mNextExpectedSeqNo;

    return OK;
}

bool AMPEG4ElementaryAssembler::submitAccessUnit() {
    MOZ_ASSERT(mPackets.empty());

    LOGV("Access unit complete (%d nal units)", mPackets.size());

    if (mIsGeneric) {
        /*
         * Bug 877116.
         * In order to remedy a latency problem caused by hardware decoder for
         * mpeg4-generic audios, we artificially divide AUs into more smaller
         * AUs before feeding them to decoder.
         *
         * TODO: However, we are not sure this solution is appropriate to video
         * or not. Need more investigation on this. Refer to RFC 3640.
         */
        for (List<sp<ABuffer> >::iterator it = mPackets.begin();
             it != mPackets.end(); ++it) {
            sp<ABuffer> accessUnit = new ABuffer((*it)->size());
            sp<ABuffer> nal = *it;
            memcpy(accessUnit->data(), nal->data(), nal->size());
            if (!CopyTimes(accessUnit, nal)) {
                return false;
            }

            if (mAccessUnitDamaged) {
                accessUnit->meta()->setInt32("damaged", true);
            }

            sp<AMessage> msg = mNotifyMsg->dup();
            msg->setObject("access-unit", accessUnit);
            msg->post();
        }
    } else {
        /*
         * For MP4V-ES (MPEG-4 Visual Elementary Streams), NAL units with the
         * same RTP timestamp are assembled into an AU, which results in one
         * decoded picture (RFC 6416).
         */
        size_t totalSize = 0;

        for (List<sp<ABuffer> >::iterator it = mPackets.begin();
             it != mPackets.end(); ++it) {
            totalSize += (*it)->size();
        }
        sp<ABuffer> accessUnit = new ABuffer(totalSize);
        size_t offset = 0;
        for (List<sp<ABuffer> >::iterator it = mPackets.begin();
             it != mPackets.end(); ++it) {
            sp<ABuffer> nal = *it;
            memcpy(accessUnit->data() + offset, nal->data(), nal->size());
            offset += nal->size();
        }
        if (!CopyTimes(accessUnit, *mPackets.begin())) {
            return false;
        }

        if (mAccessUnitDamaged) {
            accessUnit->meta()->setInt32("damaged", true);
        }

        sp<AMessage> msg = mNotifyMsg->dup();
        msg->setObject("access-unit", accessUnit);
        msg->post();
    }

    mPackets.clear();
    mAccessUnitDamaged = false;
    return true;
}

ARTPAssembler::AssemblyStatus AMPEG4ElementaryAssembler::assembleMore(
        const sp<ARTPSource> &source) {
    AssemblyStatus status = addPacket(source);
    if (status == MALFORMED_PACKET) {
        mAccessUnitDamaged = true;
    }
    return status;
}

void AMPEG4ElementaryAssembler::packetLost() {
    CHECK(mNextExpectedSeqNoValid);
    LOGV("packetLost (expected %d)", mNextExpectedSeqNo);

    ++mNextExpectedSeqNo;

    mAccessUnitDamaged = true;

    mPreviousAUCount = 0;
}

void AMPEG4ElementaryAssembler::onByeReceived() {
    sp<AMessage> msg = mNotifyMsg->dup();
    msg->setInt32("eos", true);
    msg->post();
}

}  // namespace android
