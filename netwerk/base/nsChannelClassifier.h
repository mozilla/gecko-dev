/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChannelClassifier_h__
#define nsChannelClassifier_h__

#include "nsIURIClassifier.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

class nsIChannel;
class nsIHttpChannelInternal;
class nsIDocument;

namespace mozilla {
namespace net {

class nsChannelClassifier final : public nsIURIClassifierCallback
{
public:
    nsChannelClassifier();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIURICLASSIFIERCALLBACK

    // Calls nsIURIClassifier.Classify with the principal of the given channel,
    // and cancels the channel on a bad verdict.
    void Start(nsIChannel *aChannel);
    // Whether or not tracking protection should be enabled on this channel.
    nsresult ShouldEnableTrackingProtection(nsIChannel *aChannel, bool *result);

private:
    // True if the channel is on the allow list.
    bool mIsAllowListed;
    // True if the channel has been suspended.
    bool mSuspendedChannel;
    nsCOMPtr<nsIChannel> mChannel;

    ~nsChannelClassifier() {}
    // Caches good classifications for the channel principal.
    void MarkEntryClassified(nsresult status);
    bool HasBeenClassified(nsIChannel *aChannel);
    // Helper function so that we ensure we call ContinueBeginConnect once
    // Start is called. Returns NS_OK if and only if we will get a callback
    // from the classifier service.
    nsresult StartInternal();
    // Helper function to check a tracking URI against the whitelist
    nsresult IsTrackerWhitelisted();
    // Helper function to check a URI against the hostname whitelist
    bool IsHostnameWhitelisted(nsIURI *aUri, const nsACString &aWhitelisted);
    // Checks that the channel was loaded by the URI currently loaded in aDoc
    static bool SameLoadingURI(nsIDocument *aDoc, nsIChannel *aChannel);

public:
    // If we are blocking tracking content, update the corresponding flag in
    // the respective docshell and call nsISecurityEventSink::onSecurityChange.
    static nsresult SetBlockedTrackingContent(nsIChannel *channel);
    static nsresult NotifyTrackingProtectionDisabled(nsIChannel *aChannel);
};

} // namespace net
} // namespace mozilla

#endif
