# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

## Strings for the (add-ons) data consent feature.
## TODO: Bug 1956463 - expose these strings to localizers.
##
##
## Strings for data collection permissions in the permission prompt.

webext-perms-description-data-none = The developer says this extension doesn’t require data collection.

# Variables:
#    $permissions (String): a list of data collection permissions formatted with `Intl.ListFormat` using the "narrow" style.
webext-perms-description-data-some = The developer says this extension collects: { $permissions }.

## Short form to be used in lists or in a string (`webext-perms-description-data-some`)
## that formats some of these permissions below using `Intl.ListFormat`.
##
## This is used when the permissions are required.

webext-perms-description-data-short-authenticationInfo = authentication information
webext-perms-description-data-short-bookmarksInfo = bookmarks
webext-perms-description-data-short-browsingHistory = browsing history
webext-perms-description-data-short-financialAndPaymentInfo = financial and payment information
webext-perms-description-data-short-healthInfo = health information
webext-perms-description-data-short-locationInfo = location
webext-perms-description-data-short-personalCommunications = personal communications
webext-perms-description-data-short-personallyIdentifyingInfo = personally identifying information
webext-perms-description-data-short-technicalAndInteraction = technical and interaction data
webext-perms-description-data-short-websiteActivity = website activity
webext-perms-description-data-short-websiteContent = website content

## Long form to be used in `about:addons` when these permissions are optional.
##
## Note that for `technicalAndInteraction`, the long form is also used in the
## install prompt.

webext-perms-description-data-long-authenticationInfo = Share authentication information with extension developer
webext-perms-description-data-long-bookmarksInfo = Share bookmarks information with extension developer
webext-perms-description-data-long-browsingHistory = Share browsing history with extension developer
webext-perms-description-data-long-financialAndPaymentInfo = Share financial and payment information with extension developer
webext-perms-description-data-long-healthInfo = Share health information with extension developer
webext-perms-description-data-long-locationInfo = Share location information with extension developer
webext-perms-description-data-long-personalCommunications = Share personal communications with extension developer
webext-perms-description-data-long-personallyIdentifyingInfo = Share personally identifying information with extension developer
webext-perms-description-data-long-technicalAndInteraction = Share technical and interaction data with extension developer
webext-perms-description-data-long-websiteActivity = Share website activity with extension developer
webext-perms-description-data-long-websiteContent = Share website content with extension developer
