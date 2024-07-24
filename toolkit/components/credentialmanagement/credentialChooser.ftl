# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Credential Chooser panel
##
## Variables:
##  $host (String): the hostname of the website that the user is on.
##  $provider (String): the hostname of a website the user has an account on, but is not the one they are on now (e.g. "apple.com", "accounts.google.com", "identity.example.com")

credential-chooser-header = Would you like to sign in to { $host }?
credential-chooser-identity = Use a { $provider } account
credential-chooser-sign-in-button =
    .label = Sign In
    .accesskey = S
credential-chooser-cancel-button =
    .label = Cancel
    .accesskey = C
credential-chooser-urlbar-anchor =
    .tooltiptext = Open credential panel
# This indicates that an account is initially from another website.
# This is short for "we are getting this thing from { $provider } in English.
# This is displayed on a new line below the provider's name.
credential-chooser-host-descriptor = from { $provider }
