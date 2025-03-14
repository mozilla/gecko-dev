# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This string is shown at the end of the tooltip text for
# <input type='file' multiple> when there are more than 21 files selected
# (when we will only list the first 20, plus an "and X more" line).
# Variables:
#   $fileCount (Number): The number of remaining files.
input-file-and-more-files =
    { $fileCount ->
        [one] and one more
       *[other] and { $fileCount } more
    }

form-post-secure-to-insecure-warning-title = Security Warning
form-post-secure-to-insecure-warning-message =
    The information you have entered on this page will be sent over an insecure connection and could be read by a third party.

    Are you sure you want to send this information?
form-post-secure-to-insecure-warning-continue = Continue
