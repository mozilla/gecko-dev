# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase, parameterized, WindowManagerMixin


PAGE_XHTML = "chrome://remote/content/marionette/test.xhtml"
PAGE_XUL = "chrome://remote/content/marionette/test_xul.xhtml"


class TestTitleChrome(WindowManagerMixin, MarionetteTestCase):
    def tearDown(self):
        try:
            self.close_all_windows()
        finally:
            super(TestTitleChrome, self).tearDown()

    @parameterized("XUL", PAGE_XUL)
    @parameterized("XHTML", PAGE_XHTML)
    def test_get_title(self, chrome_url):
        win = self.open_chrome_window(chrome_url)
        self.marionette.switch_to_window(win)

        with self.marionette.using_context("chrome"):
            expected_title = self.marionette.execute_script(
                "return window.document.title;"
            )
            self.assertEqual(self.marionette.title, expected_title)
