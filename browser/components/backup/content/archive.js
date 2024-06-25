/**
#if 0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# This file is used to construct the single-file archive of a backup. This
# file is part of our source code, and so this is why we include the license
# header above. We do not, however, want to apply the header to backup files
# that are generated via this template. This is why we use the pre-processor
# mechanism to remove this comment block at build time.
#
#endif
*/

const UA = navigator.userAgent;
const isMozBrowser = /Firefox/.test(UA);

document.body.toggleAttribute("is-moz-browser", isMozBrowser);
