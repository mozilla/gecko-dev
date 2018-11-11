# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import copy

from marionette_harness import MarionetteTestCase


class TestCommandLineArguments(MarionetteTestCase):

    def setUp(self):
        super(TestCommandLineArguments, self).setUp()

        self.orig_arguments = copy.copy(self.marionette.instance.app_args)

    def tearDown(self):
        self.marionette.instance.app_args = self.orig_arguments
        self.marionette.quit(clean=True)

        super(TestCommandLineArguments, self).tearDown()

    def test_start_in_safe_mode(self):
        self.marionette.instance.app_args.append("-safe-mode")

        self.marionette.quit()
        self.marionette.start_session()

        with self.marionette.using_context("chrome"):
            safe_mode = self.marionette.execute_script("""
              Cu.import("resource://gre/modules/Services.jsm");

              return Services.appinfo.inSafeMode;
            """)
            self.assertTrue(safe_mode, "Safe Mode has not been enabled")

    def test_startup_timeout(self):
        try:
            self.marionette.quit()
            with self.assertRaisesRegexp(IOError, "Process killed after 0s"):
                # Use a small enough timeout which should always cause an IOError
                self.marionette.start_session(timeout=0)
        finally:
            self.marionette.start_session()
