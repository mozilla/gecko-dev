# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains tests that verify various HTTPS-First upgrade scenarios
# by loading requested URLs into the browser's location bar.
#
# These tests allow nonlocal connections to real websites, ensuring that the
# feature works as intended.
#
# Additional HTTPS-First tests can be found at: dom/security/test/https-first


from marionette_driver import By, Keys, Wait
from marionette_driver.errors import JavascriptException
from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestHTTPSFirst(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestHTTPSFirst, self).setUp()

        self.http_url = "http://example.org/"
        self.https_url = "https://example.org/"
        self.schemeless_url = "example.org"

        self.http_only_url = "http://http.badssl.com/"
        self.bad_ssl_certificate = "http://expired.badssl.com/"

    def tearDown(self):
        with self.marionette.using_context("chrome"):
            self.marionette.execute_script("Services.perms.removeAll();")

        super(TestHTTPSFirst, self).tearDown()

    def test_upgrade_with_schemeless_url(self):
        self.navigate_in_urlbar(self.schemeless_url)
        self.wait_for_page_navigated(
            self.https_url, f"Expected HTTPS-First upgrade to {self.https_url}"
        )

    def test_no_upgrade_with_http_scheme(self):
        self.navigate_in_urlbar(self.http_url)
        self.wait_for_page_navigated(
            self.http_url, f"Expected no HTTPS-First upgrade for {self.http_url}"
        )

    def test_no_upgrade_with_http_only_site(self):
        self.navigate_in_urlbar(self.http_only_url)
        self.wait_for_page_navigated(
            self.http_only_url, "Expected no HTTPS-First upgrade for HTTP-only site"
        )

    def navigate_in_urlbar(self, url):
        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element(By.ID, "urlbar-input")
            urlbar.clear()
            urlbar.send_keys(url + Keys.ENTER)

    def wait_for_page_navigated(self, target_url, message):
        def navigated(m):
            return self.marionette.execute_async_script(
                """
                const [url, resolve] = arguments;

                if (
                  ["interactive", "complete"].includes(document.readyState) &&
                  window.location.href == url
                ) {
                  resolve(window.location.href);
                } else {
                  window.addEventListener("DOMContentLoaded", () => {
                    resolve(window.location.href)
                  }, { once: true });
                }
            """,
                script_args=[target_url],
            )

        Wait(self.marionette, ignored_exceptions=[JavascriptException]).until(
            navigated, message=message
        )
