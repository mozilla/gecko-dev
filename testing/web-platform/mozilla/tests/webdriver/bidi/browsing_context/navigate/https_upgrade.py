from urllib.parse import urlunsplit

import pytest
from tests.bidi.browsing_context.navigate import navigate_and_assert

pytestmark = pytest.mark.asyncio


@pytest.fixture
def http_with_https_port_url(server_config):
    """
    Creates an HTTP URL with a port for HTTPS that would trigger a HTTPS-First
    upgrade when entered in the url bar.
    """

    def _http_with_https_port_url(path, query="", fragment=""):
        domain = server_config["domains"][""][""]
        port = server_config["ports"]["https"][0]
        return urlunsplit(("http", f"{domain}:{port}", path, query, fragment))

    return _http_with_https_port_url


@pytest.mark.capabilities(
    {
        "moz:firefoxOptions": {
            "prefs": {
                # Allow HTTPS upgrades for localhost and custom ports
                "dom.security.https_first_for_custom_ports": True,
                "dom.security.https_first_for_local_addresses": True,
                "dom.security.https_first_for_unknown_suffixes": True,
            },
        },
    }
)
async def test_no_https_first_upgrade(bidi_session, new_tab, http_with_https_port_url):
    page = http_with_https_port_url("/webdriver/tests/support/html/default.html")

    # A navigation via the WebDriver API should not cause an HTTPS upgrade,
    # so it fails with a neterror page.
    await navigate_and_assert(
        bidi_session,
        new_tab,
        page,
        expected_error=True,
    )

    contexts = await bidi_session.browsing_context.get_tree(
        root=new_tab["context"], max_depth=0
    )

    assert contexts[0]["url"].startswith("http://")
