# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from mach.logging import LoggingManager

from mozbuild.util import ReadOnlyDict

import mozpack.path as mozpath


# By including this module, tests get structured logging.
log_manager = LoggingManager()
log_manager.add_terminal_logging()

# mozconfig is not a reusable type (it's actually a module) so, we
# have to mock it.
class MockConfig(object):
    def __init__(self, topsrcdir='/path/to/topsrcdir', extra_substs={}):
        self.topsrcdir = mozpath.abspath(topsrcdir)
        self.topobjdir = mozpath.abspath('/path/to/topobjdir')

        self.substs = ReadOnlyDict({
            'MOZ_FOO': 'foo',
            'MOZ_BAR': 'bar',
            'MOZ_TRUE': '1',
            'MOZ_FALSE': '',
        }, **extra_substs)

        self.substs_unicode = ReadOnlyDict({k.decode('utf-8'): v.decode('utf-8',
            'replace') for k, v in self.substs.items()})

        self.defines = self.substs

        self.external_source_dir = None
        self.lib_prefix = 'lib'
        self.lib_suffix = '.so'
