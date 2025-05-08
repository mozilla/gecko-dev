# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Extension permission description keys are derived from permission names.
## Permissions for which the message has been changed and the key updated
## must have a corresponding entry in the `PERMISSION_L10N_ID_OVERRIDES` map.

webext-perms-description-bookmarks = Read and modify bookmarks
webext-perms-description-browserSettings = Read and modify browser settings
webext-perms-description-browsingData = Clear recent browsing history, cookies, and related data
webext-perms-description-clipboardRead = Get data from the clipboard
webext-perms-description-clipboardWrite = Input data to the clipboard
webext-perms-description-declarativeNetRequest = Block content on any page
webext-perms-description-declarativeNetRequestFeedback = Read your browsing history
webext-perms-description-devtools = Extend developer tools to access your data in open tabs
webext-perms-description-downloads = Download files and read and modify the browser’s download history
webext-perms-description-downloads-open = Open files downloaded to your computer
webext-perms-description-find = Read the text of all open tabs
webext-perms-description-geolocation = Access your location
webext-perms-description-history = Access browsing history
webext-perms-description-management = Monitor extension usage and manage themes
webext-perms-description-nativeMessaging = Exchange messages with programs other than { -brand-short-name }
webext-perms-description-notifications = Display notifications to you
webext-perms-description-pkcs11 = Provide cryptographic authentication services
webext-perms-description-privacy = Read and modify privacy settings
webext-perms-description-proxy = Control browser proxy settings
webext-perms-description-sessions = Access recently closed tabs
webext-perms-description-tabs = Access browser tabs
webext-perms-description-tabHide = Hide and show browser tabs
webext-perms-description-topSites = Access browsing history
webext-perms-description-trialML = Download and run AI models on your device
webext-perms-description-userScripts = Allow unverified third-party scripts to access your data
webext-perms-description-webNavigation = Access browser activity during navigation

## The userScripts permission includes an additional explanation that is
## displayed prominently near the usual permission description.

webext-perms-extra-warning-userScripts-long = Unverified scripts can pose security and privacy risks, such as running harmful code or tracking website activity. Only run scripts from extensions or sources you trust.
# A shorter warning is displayed in UI surfaces with little room, such as a permission prompt.
webext-perms-extra-warning-userScripts-short = Unverified scripts can pose security and privacy risks. Only run scripts from extensions or sources you trust.

## Short form to be used in lists or in a string (`webext-perms-description-data-some`)
## that formats some of the data collection permissions below using `Intl.ListFormat`.
##
## This is used when the data collection permissions are required.

webext-perms-description-data-short-authenticationInfo = authentication information
webext-perms-description-data-short-bookmarksInfo = bookmarks
webext-perms-description-data-short-browsingActivity = browsing activity
webext-perms-description-data-short-financialAndPaymentInfo = financial and payment information
webext-perms-description-data-short-healthInfo = health information
webext-perms-description-data-short-locationInfo = location
webext-perms-description-data-short-personalCommunications = personal communications
webext-perms-description-data-short-personallyIdentifyingInfo = personally identifying information
webext-perms-description-data-short-searchTerms = search terms
webext-perms-description-data-short-technicalAndInteraction = technical and interaction data
webext-perms-description-data-short-websiteActivity = website activity
webext-perms-description-data-short-websiteContent = website content

## Long form to be used in `about:addons` when these data collection permissions are optional.

webext-perms-description-data-long-authenticationInfo = Share authentication information with extension developer
webext-perms-description-data-long-bookmarksInfo = Share bookmarks information with extension developer
webext-perms-description-data-long-browsingActivity = Share browsing activity with extension developer
webext-perms-description-data-long-financialAndPaymentInfo = Share financial and payment information with extension developer
webext-perms-description-data-long-healthInfo = Share health information with extension developer
webext-perms-description-data-long-locationInfo = Share location information with extension developer
webext-perms-description-data-long-personalCommunications = Share personal communications with extension developer
webext-perms-description-data-long-personallyIdentifyingInfo = Share personally identifying information with extension developer
webext-perms-description-data-long-searchTerms = Share search terms with extension developer
webext-perms-description-data-long-technicalAndInteraction = Share technical and interaction data with extension developer
webext-perms-description-data-long-websiteActivity = Share website activity with extension developer
webext-perms-description-data-long-websiteContent = Share website content with extension developer
