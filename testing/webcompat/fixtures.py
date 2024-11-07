# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import asyncio
import json
import os
import re
import subprocess
import sys
from datetime import datetime

import pytest
import webdriver

from client import Client

try:
    import pathlib
except ImportError:
    import pathlib2 as pathlib

APS_PREF = "privacy.partition.always_partition_third_party_non_cookie_storage"
CB_PBM_PREF = "network.cookie.cookieBehavior.pbmode"
CB_PREF = "network.cookie.cookieBehavior"
INJECTIONS_PREF = "extensions.webcompat.perform_injections"
NOTIFICATIONS_PERMISSIONS_PREF = "permissions.default.desktop-notification"
PBM_PREF = "browser.privatebrowsing.autostart"
PIP_OVERRIDES_PREF = "extensions.webcompat.enable_picture_in_picture_overrides"
SHIMS_PREF = "extensions.webcompat.enable_shims"
STRICT_ETP_PREF = "privacy.trackingprotection.enabled"
UA_OVERRIDES_PREF = "extensions.webcompat.perform_ua_overrides"
SYSTEM_ADDON_UPDATES_PREF = "extensions.systemAddon.update.enabled"


class WebDriver:
    def __init__(self, config):
        self.browser_binary = config.getoption("browser_binary")
        self.device_serial = config.getoption("device_serial")
        self.package_name = config.getoption("package_name")
        self.addon = config.getoption("addon")
        self.webdriver_binary = config.getoption("webdriver_binary")
        self.port = config.getoption("webdriver_port")
        self.ws_port = config.getoption("webdriver_ws_port")
        self.log_level = config.getoption("webdriver_log_level")
        self.headless = config.getoption("headless")
        self.debug = config.getoption("debug")
        self.proc = None

    def command_line_driver(self):
        raise NotImplementedError

    def capabilities(self, test_config):
        raise NotImplementedError

    def __enter__(self):
        assert self.proc is None
        self.proc = subprocess.Popen(self.command_line_driver())
        return self

    def __exit__(self, *args, **kwargs):
        self.proc.kill()


class FirefoxWebDriver(WebDriver):
    def command_line_driver(self):
        rv = [
            self.webdriver_binary,
            "--port",
            str(self.port),
            "--websocket-port",
            str(self.ws_port),
        ]
        if self.debug:
            rv.append("-vv")
        elif self.log_level == "DEBUG":
            rv.append("-v")
        return rv

    def capabilities(self, test_config):
        prefs = {}

        if "aps" in test_config:
            prefs[APS_PREF] = test_config["aps"]

        if "use_interventions" in test_config:
            value = test_config["use_interventions"]
            prefs[INJECTIONS_PREF] = value
            prefs[UA_OVERRIDES_PREF] = value
            prefs[PIP_OVERRIDES_PREF] = value

        if "use_pbm" in test_config:
            prefs[PBM_PREF] = test_config["use_pbm"]

        if "use_shims" in test_config:
            prefs[SHIMS_PREF] = test_config["use_shims"]

        if "use_strict_etp" in test_config:
            prefs[STRICT_ETP_PREF] = test_config["use_strict_etp"]

        if "no_overlay_scrollbars" in test_config:
            prefs["widget.gtk.overlay-scrollbars.enabled"] = False

        # keep system addon updates off to prevent bug 1882562
        prefs[SYSTEM_ADDON_UPDATES_PREF] = False

        # remote/cdp/CDP.sys.mjs sets cookieBehavior to 0,
        # which we definitely do not want, so set it back to 5.
        cookieBehavior = 4 if test_config.get("without_tcp") else 5
        prefs[CB_PREF] = cookieBehavior
        prefs[CB_PBM_PREF] = cookieBehavior

        # prevent "allow notifications for?" popups by setting the
        # default permission for notificaitons to PERM_DENY_ACTION.
        prefs[NOTIFICATIONS_PERMISSIONS_PREF] = 2

        fx_options = {"prefs": prefs}

        if self.browser_binary:
            fx_options["binary"] = self.browser_binary
            if self.headless:
                fx_options["args"] = ["--headless"]

        if self.device_serial:
            fx_options["androidDeviceSerial"] = self.device_serial
            fx_options["androidPackage"] = self.package_name

        if self.addon:
            prefs["xpinstall.signatures.required"] = False
            prefs["extensions.experiments.enabled"] = True

        return {
            "pageLoadStrategy": "normal",
            "moz:firefoxOptions": fx_options,
        }


@pytest.fixture(scope="session")
def should_do_2fa(request):
    return request.config.getoption("do2fa", False)


@pytest.fixture(scope="session")
def config_file(request):
    path = request.config.getoption("config")
    if not path:
        return None
    with open(path) as f:
        return json.load(f)


@pytest.fixture
def bug_number(request):
    return re.findall(r"\d+", str(request.fspath.basename))[0]


@pytest.fixture
def in_headless_mode(request):
    return request.config.getoption("headless")


@pytest.fixture
def credentials(bug_number, config_file):
    if not config_file:
        pytest.skip(f"login info required for bug #{bug_number}")
        return None

    try:
        credentials = config_file[bug_number]
    except KeyError:
        pytest.skip(f"no login for bug #{bug_number} found")
        return

    return {"username": credentials["username"], "password": credentials["password"]}


@pytest.fixture(scope="session")
def driver(pytestconfig):
    if pytestconfig.getoption("browser") == "firefox":
        cls = FirefoxWebDriver
    else:
        assert False

    with cls(pytestconfig) as driver_instance:
        yield driver_instance


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    rep = outcome.get_result()
    setattr(item, "rep_" + rep.when, rep)


@pytest.fixture(scope="function", autouse=True)
async def test_failed_check(request):
    yield
    if (
        not request.config.getoption("no_failure_screenshots")
        and request.node.rep_setup.passed
        and request.node.rep_call.failed
    ):
        session = request.node.funcargs["session"]
        try:
            file_name = f'{request.node.nodeid}_failure_{datetime.today().strftime("%Y-%m-%d_%H:%M")}.png'.replace(
                "/", "_"
            ).replace(
                "::", "__"
            )
            await take_screenshot(session, file_name)
            print("Saved failure screenshot to: ", file_name)
        except Exception as e:
            print("Error saving screenshot: ", e)


async def take_screenshot(session, file_name):
    cwd = pathlib.Path(os.getcwd())
    path = cwd / file_name

    top = await session.bidi_session.browsing_context.get_tree()
    screenshot = await session.bidi_session.browsing_context.capture_screenshot(
        context=top[0]["context"]
    )

    with path.open("wb") as strm:
        strm.write(screenshot)

    return file_name


@pytest.fixture(scope="session")
def event_loop():
    return asyncio.get_event_loop_policy().new_event_loop()


@pytest.fixture(scope="function")
async def client(request, session, event_loop):
    return Client(request, session, event_loop)


def install_addon(session, addon_file_path):
    context = session.send_session_command("GET", "moz/context")
    session.send_session_command("POST", "moz/context", {"context": "chrome"})
    session.execute_async_script(
        """
        async function installAsBuiltinExtension(xpi) {
            // The built-in location requires a resource: URL that maps to a
            // jar: or file: URL.  This would typically be something bundled
            // into omni.ja but we use a temp file.
            let base = Services.io.newURI(`jar:file:${xpi.path}!/`);
            let resProto = Services.io
              .getProtocolHandler("resource")
              .QueryInterface(Ci.nsIResProtocolHandler);
            resProto.setSubstitution("ext-test", base);
            return AddonManager.installBuiltinAddon("resource://ext-test/");
        }

        const addon_file_path = arguments[0];
        const cb = arguments[1];
        const { AddonManager } = ChromeUtils.importESModule(
            "resource://gre/modules/AddonManager.sys.mjs"
        );
        const { ExtensionPermissions } = ChromeUtils.importESModule(
            "resource://gre/modules/ExtensionPermissions.sys.mjs"
        );
        const { FileUtils } = ChromeUtils.importESModule(
            "resource://gre/modules/FileUtils.sys.mjs"
        );
        const file = new FileUtils.File(arguments[0]);
        installAsBuiltinExtension(file).then(addon => {
            // also make sure the addon works in private browsing mode
            const incognitoPermission = {
                permissions: ["internal:privateBrowsingAllowed"],
                origins: [],
            };
            ExtensionPermissions.add(addon.id, incognitoPermission).then(() => {
                addon.reload().then(cb);
            });
        });
        """,
        [addon_file_path],
    )
    session.send_session_command("POST", "moz/context", {"context": context})


@pytest.fixture(scope="function")
async def session(driver, test_config):
    caps = driver.capabilities(test_config)
    caps.update(
        {
            "acceptInsecureCerts": True,
            "webSocketUrl": True,
        }
    )
    caps = {"alwaysMatch": caps}
    print(caps)

    session = None
    for i in range(0, 15):
        try:
            if not session:
                session = webdriver.Session(
                    "localhost", driver.port, capabilities=caps, enable_bidi=True
                )
                session.test_config = test_config
            session.start()
            break
        except (ConnectionRefusedError, webdriver.error.TimeoutException):
            await asyncio.sleep(0.5)

    try:
        await session.bidi_session.start()
    except AttributeError:
        sys.exit("Could not start a WebDriver session; please try again")

    if driver.addon:
        install_addon(session, driver.addon)

    yield session

    await session.bidi_session.end()
    session.end()


@pytest.fixture(autouse=True)
def platform(session):
    return session.capabilities["platformName"]


@pytest.fixture(autouse=True)
def check_visible_scrollbars(session):
    plat = session.capabilities["platformName"]
    if plat == "android":
        return "Android does not have visible scrollbars"
    elif plat == "mac":
        cmd = ["defaults", "read", "-g", "AppleShowScrollBars"]
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        p.wait()
        if "Always" in str(p.stdout.readline()):
            return None
        return "scrollbars are not set to always be visible in MacOS system preferences"
    return None


@pytest.fixture(autouse=True)
def need_visible_scrollbars(bug_number, check_visible_scrollbars, request, session):
    if request.node.get_closest_marker("need_visible_scrollbars"):
        if (
            request.node.get_closest_marker("need_visible_scrollbars")
            and check_visible_scrollbars
        ):
            pytest.skip(f"Bug #{bug_number} skipped: {check_visible_scrollbars}")


@pytest.fixture(autouse=True)
def only_platforms(bug_number, platform, request, session):
    if request.node.get_closest_marker("only_platforms"):
        plats = request.node.get_closest_marker("only_platforms").args
        for only in plats:
            if only == platform:
                return
        pytest.skip(
            f"Bug #{bug_number} skipped on platform ({platform}, test only for {' or '.join(plats)})"
        )


@pytest.fixture(autouse=True)
def skip_platforms(bug_number, platform, request, session):
    if request.node.get_closest_marker("skip_platforms"):
        plats = request.node.get_closest_marker("skip_platforms").args
        for skipped in plats:
            if skipped == platform:
                pytest.skip(
                    f"Bug #{bug_number} skipped on platform ({platform}, test skipped for {' and '.join(plats)})"
                )
