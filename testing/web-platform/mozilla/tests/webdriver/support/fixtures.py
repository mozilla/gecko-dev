import pytest
import pytest_asyncio
from tests.support.helpers import deep_update

from .helpers import (
    Browser,
    Geckodriver,
    create_custom_profile,
    get_pref,
    get_profile_folder,
    read_user_preferences,
    set_pref,
)


def pytest_collection_modifyitems(items):
    """Auto-apply markers for specific API usage by fixtures"""
    for item in items:
        if "use_pref" in getattr(item, "fixturenames", ()):
            item.add_marker("allow_system_access")


def pytest_configure(config):
    # register the allow_system_access marker
    config.addinivalue_line(
        "markers", "allow_system_access: Mark test to allow system access"
    )


@pytest.fixture(scope="module")
def browser(configuration, firefox_options):
    """Start a Firefox instance without using geckodriver.

    geckodriver will automatically use the --remote-allow-hosts and
    --remote.allow.origins command line arguments.

    Starting Firefox without geckodriver allows to set those command line arguments
    as needed. The fixture method returns the browser instance that should be used
    to connect to a RemoteAgent supported protocol (WebDriver BiDi).
    """
    current_browser = None

    def _browser(
        clone_profile=True,
        extra_args=None,
        extra_prefs=None,
        use_bidi=False,
        use_marionette=False,
    ):
        nonlocal current_browser

        webdriver_args = configuration["webdriver"]["args"]
        log_level = None
        truncate_enabled = True

        if "-vv" or "-vvv" in webdriver_args:
            log_level = "Trace"

            if "-vvv" in webdriver_args:
                truncate_enabled = False
        elif "-v" in webdriver_args:
            log_level = "Debug"

        # If the requested preferences and arguments match the ones for the
        # already started firefox, we can reuse the current firefox instance,
        # return the instance immediately.
        if current_browser:
            if (
                current_browser.extra_args == extra_args
                and current_browser.extra_prefs == extra_prefs
                and current_browser.is_running
                and current_browser.use_bidi == use_bidi
                and current_browser.use_marionette == use_marionette
                and current_browser.log_level == log_level
                and current_browser.truncate_enabled == truncate_enabled
            ):
                return current_browser

            # Otherwise, if firefox is already started, terminate it because we need
            # to create a new instance for the provided preferences.
            current_browser.quit()

        binary = configuration["browser"]["binary"]
        env = configuration["browser"]["env"]

        profile_path = get_profile_folder(firefox_options)
        default_prefs = read_user_preferences(profile_path)
        profile = create_custom_profile(
            profile_path, default_prefs, clone=clone_profile
        )

        current_browser = Browser(
            binary,
            profile,
            extra_args=extra_args,
            extra_prefs=extra_prefs,
            env=env,
            log_level=log_level,
            truncate_enabled=truncate_enabled,
            use_bidi=use_bidi,
            use_marionette=use_marionette,
        )
        current_browser.start()
        return current_browser

    yield _browser

    # Stop firefox at the end of the test module.
    if current_browser is not None:
        current_browser.quit()
        current_browser = None


@pytest.fixture
def default_capabilities(request):
    """Default capabilities to use for a new WebDriver session for Mozilla specific tests."""

    # Get the value from the overwritten original fixture
    capabilities = request.getfixturevalue("default_capabilities")

    allow_system_access = any(
        marker.name == "allow_system_access" for marker in request.node.own_markers
    )

    if allow_system_access:
        deep_update(
            capabilities,
            {
                "moz:firefoxOptions": {
                    "args": [
                        "--remote-allow-system-access",
                    ]
                }
            },
        )

    return capabilities


@pytest.fixture
def default_preferences(profile_folder):
    return read_user_preferences(profile_folder)


@pytest.fixture(name="create_custom_profile")
def fixture_create_custom_profile(default_preferences, profile_folder):
    profile = None

    def _create_custom_profile(clone=True):
        profile = create_custom_profile(
            profile_folder, default_preferences, clone=clone
        )

        return profile

    yield _create_custom_profile

    # if profile is not None:
    if profile:
        profile.cleanup()


@pytest.fixture(scope="session")
def firefox_options(configuration):
    return configuration["capabilities"]["moz:firefoxOptions"]


@pytest_asyncio.fixture
async def geckodriver(configuration):
    """Start a geckodriver instance directly."""
    driver = None

    def _geckodriver(config=None, hostname=None, extra_args=None, extra_env=None):
        nonlocal driver

        if config is None:
            config = configuration

        driver = Geckodriver(config, hostname, extra_args, extra_env)
        driver.start()

        return driver

    yield _geckodriver

    if driver is not None:
        await driver.stop()


@pytest.fixture
def profile_folder(firefox_options):
    return get_profile_folder(firefox_options)


@pytest.fixture
def use_pref(session):
    """Set a specific pref value."""
    reset_values = {}

    def _use_pref(pref, value):
        if pref not in reset_values:
            reset_values[pref] = get_pref(session, pref)

        set_pref(session, pref, value)

    yield _use_pref

    for pref, reset_value in reset_values.items():
        set_pref(session, pref, reset_value)
