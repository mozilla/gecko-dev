/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

/*
  Currently supported is h2
*/

#include "nsHttp.h"
#include "nsHttpHandler.h"

#include "ASpdySession.h"
#include "Http2Session.h"

#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"

namespace mozilla {
namespace net {

ASpdySession* ASpdySession::NewSpdySession(net::SpdyVersion version,
                                           nsISocketTransport* aTransport,
                                           bool attemptingEarlyData) {
  // This is a necko only interface, so we can enforce version
  // requests as a precondition
  MOZ_ASSERT(version == SpdyVersion::HTTP_2, "Unsupported spdy version");

  // Don't do a runtime check of IsSpdyV?Enabled() here because pref value
  // may have changed since starting negotiation. The selected protocol comes
  // from a list provided in the SERVER HELLO filtered by our acceptable
  // versions, so there is no risk of the server ignoring our prefs.

  return Http2Session::CreateSession(aTransport, version, attemptingEarlyData);
}

SpdyInformation::SpdyInformation() {
  // highest index of enabled protocols is the
  // most preferred for ALPN negotiaton
  Version = SpdyVersion::HTTP_2;
  VersionString = "h2"_ns;
  ALPNCallbacks = Http2Session::ALPNCallback;
}

}  // namespace net
}  // namespace mozilla
