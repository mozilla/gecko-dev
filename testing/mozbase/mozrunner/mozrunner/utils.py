#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""Utility functions for mozrunner"""

__all__ = ['findInPath', 'get_metadata_from_egg', 'uses_marionette']


from functools import wraps
import mozinfo
import os
import sys


### python package method metadata by introspection
try:
    import pkg_resources
    def get_metadata_from_egg(module):
        ret = {}
        try:
            dist = pkg_resources.get_distribution(module)
        except pkg_resources.DistributionNotFound:
            return {}
        if dist.has_metadata("PKG-INFO"):
            key = None
            for line in dist.get_metadata("PKG-INFO").splitlines():
                # see http://www.python.org/dev/peps/pep-0314/
                if key == 'Description':
                    # descriptions can be long
                    if not line or line[0].isspace():
                        value += '\n' + line
                        continue
                    else:
                        key = key.strip()
                        value = value.strip()
                        ret[key] = value

                key, value = line.split(':', 1)
                key = key.strip()
                value = value.strip()
                ret[key] = value
        if dist.has_metadata("requires.txt"):
            ret["Dependencies"] = "\n" + dist.get_metadata("requires.txt")
        return ret
except ImportError:
    # package resources not avaialable
    def get_metadata_from_egg(module):
        return {}


def findInPath(fileName, path=os.environ['PATH']):
    """python equivalent of which; should really be in the stdlib"""
    dirs = path.split(os.pathsep)
    for dir in dirs:
        if os.path.isfile(os.path.join(dir, fileName)):
            return os.path.join(dir, fileName)
        if mozinfo.isWin:
            if os.path.isfile(os.path.join(dir, fileName + ".exe")):
                return os.path.join(dir, fileName + ".exe")

if __name__ == '__main__':
    for i in sys.argv[1:]:
        print findInPath(i)


def _find_marionette_in_args(*args, **kwargs):
    try:
        m = [a for a in args + tuple(kwargs.values()) if hasattr(a, 'session')][0]
    except IndexError:
        print("Can only apply decorator to function using a marionette object")
        raise
    return m

def uses_marionette(func):
    """Decorator which creates a marionette session and deletes it
    afterwards if one doesn't already exist.
    """
    @wraps(func)
    def _(*args, **kwargs):
        m = _find_marionette_in_args(*args, **kwargs)
        delete_session = False
        if not m.session:
            delete_session = True
            m.start_session()

        m.set_context(m.CONTEXT_CHROME)
        ret = func(*args, **kwargs)

        if delete_session:
            m.delete_session()

        return ret
    return _
