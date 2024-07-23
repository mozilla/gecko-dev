# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Imported from
# https://searchfox.org/mozilla-central/rev/ece43b04e7baa4680dac46a06d5ad42b27b124f4/python/mozbuild/mozbuild/util.py#102

import copy


class ReadOnlyDict(dict):
    """A read-only dictionary."""

    def __init__(self, *args, **kwargs):
        dict.__init__(self, *args, **kwargs)

    def __delitem__(self, key):
        raise Exception("Object does not support deletion.")

    def __setitem__(self, key, value):
        raise Exception("Object does not support assignment.")

    def update(self, *args, **kwargs):
        raise Exception("Object does not support update.")

    def __copy__(self, *args, **kwargs):
        return ReadOnlyDict(**dict.copy(self, *args, **kwargs))

    def __deepcopy__(self, memo):
        result = {}
        for k, v in self.items():
            result[k] = copy.deepcopy(v, memo)

        return ReadOnlyDict(**result)

    def __reduce__(self, *args, **kwargs):
        """
        Support for `pickle`.
        """

        return (self.__class__, (dict(self),))
