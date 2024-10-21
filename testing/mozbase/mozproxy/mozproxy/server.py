# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozproxy.backends.mitm.android import MitmproxyAndroid
from mozproxy.backends.mitm.desktop import MitmproxyDesktop
from mozproxy.utils import LOG

_BACKENDS = {"mitmproxy": MitmproxyDesktop, "mitmproxy-android": MitmproxyAndroid}


def get_backend(name, *args, **kw):
    """Returns the class that implements the backend.

    Raises KeyError in case the backend does not exists.
    """

    # Bug 1883701
    # Our linux desktop platforms are still using ubuntu 18
    # which is not compatible with mitm 11. So for the time being
    # force 8.1.1
    if name == "mitmproxy" and args[0]["platform"] == "linux":
        args[0]["playback_version"] = "8.1.1"
        LOG.info("Forcing mitmproxy version 8.1.1 for linux desktop.")

    return _BACKENDS[name](*args, **kw)
