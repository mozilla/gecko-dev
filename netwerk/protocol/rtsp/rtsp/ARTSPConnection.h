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

#ifndef A_RTSP_CONNECTION_H_

#define A_RTSP_CONNECTION_H_

#include "mozilla/Types.h"
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/AString.h>

#include "prio.h"

namespace android {

struct MOZ_EXPORT ABuffer;

struct ARTSPResponse : public RefBase {
    unsigned long mStatusCode;
    AString mStatusLine;
    KeyedVector<AString,AString> mHeaders;
    sp<ABuffer> mContent;
};

struct ARTSPConnection : public AHandler {
    ARTSPConnection(bool uidValid = false, uid_t uid = 0);

    void connect(const char *url, const sp<AMessage> &reply);
    void disconnect(const sp<AMessage> &reply);

    void sendRequest(const char *request, const sp<AMessage> &reply);

    void observeBinaryData(const sp<AMessage> &reply);

    static bool ParseURL(
            const char *url, AString *host, uint16_t *port, AString *path,
            AString *user, AString *pass);
    void MakeUserAgent(const char *userAgent);

protected:
    virtual ~ARTSPConnection();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    enum State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
    };

    enum {
        kWhatConnect = 1,
        kWhatDisconnect,
        kWhatCompleteConnection,
        kWhatSendRequest,
        kWhatReceiveResponse,
        kWhatObserveBinaryData,
    };

    enum AuthType {
        NONE,
        BASIC,
        DIGEST
    };

    static const uint32_t kSocketPollTimeoutUs;
    static const uint32_t kSocketPollTimeoutRetries;
    static const uint32_t kSocketBlokingRecvTimeout;

    bool mUIDValid;
    uid_t mUID;
    State mState;
    AString mUser, mPass;
    AuthType mAuthType;
    AString mNonce;
    int32_t mConnectionID;
    int32_t mNextCSeq;
    bool mReceiveResponseEventPending;
    PRFileDesc *mSocket;
    uint32_t mNumSocketPollTimeoutRetries;

    KeyedVector<int32_t, sp<AMessage> > mPendingRequests;

    sp<AMessage> mObserveBinaryMessage;

    AString mUserAgent;

    void performDisconnect();

    void onConnect(const sp<AMessage> &msg);
    void onDisconnect(const sp<AMessage> &msg);
    void onCompleteConnection(const sp<AMessage> &msg);
    void onSendRequest(const sp<AMessage> &msg);
    void onReceiveResponse();

    void flushPendingRequests();
    void postReceiveResponseEvent();

    // Return false iff something went unrecoverably wrong.
    bool receiveRTSPResponse();
    status_t receive(void *data, size_t size);
    bool receiveLine(AString *line);
    sp<ABuffer> receiveBinaryData();
    bool notifyResponseListener(const sp<ARTSPResponse> &response);

    bool parseAuthMethod(const sp<ARTSPResponse> &response);
    void addAuthentication(AString *request);

    void addUserAgent(AString *request) const;

    status_t findPendingRequest(
            const sp<ARTSPResponse> &response, ssize_t *index) const;

    bool handleServerRequest(const sp<ARTSPResponse> &request);

    static bool ParseSingleUnsignedLong(
            const char *from, unsigned long *x);

    void closeSocket();

    DISALLOW_EVIL_CONSTRUCTORS(ARTSPConnection);
};

}  // namespace android

#endif  // A_RTSP_CONNECTION_H_
