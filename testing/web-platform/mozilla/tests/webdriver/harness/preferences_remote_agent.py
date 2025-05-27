from support.helpers import read_user_preferences
from tests.support.sync import Poll


def test_remote_agent_recommended_preferences_applied(browser):
    # Marionette cannot be enabled for this test because it will also set the
    # recommended preferences. Therefore only enable Remote Agent protocols.
    current_browser = browser(use_bidi=True)

    def pref_is_set(_):
        preferences = read_user_preferences(current_browser.profile.profile, "prefs.js")
        return preferences.get("remote.prefs.recommended.applied", False)

    # Without Marionette enabled preferences cannot be retrieved via script evaluation yet.
    wait = Poll(
        None,
        timeout=5,
        ignored_exceptions=IOError,
        message="""Preference "remote.prefs.recommended.applied" is not true""",
    )
    wait.until(pref_is_set)
