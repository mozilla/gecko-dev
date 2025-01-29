from urllib.parse import urlunsplit

import pytest
from tests.support.asserts import assert_error


def navigate_to(session, url):
    return session.transport.send(
        "POST", "session/{session_id}/url".format(**vars(session)), {"url": url}
    )


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
        "pageLoadStrategy": "eager",
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
def test_no_https_first_upgrade(session, http_with_https_port_url):
    page = http_with_https_port_url("/webdriver/tests/support/html/default.html")

    # A navigation via the WebDriver API should not cause an HTTPS upgrade,
    # so it fails with a neterror page.
    response = navigate_to(session, page)
    assert_error(response, "unknown error")

    assert session.url.startswith("http://")
