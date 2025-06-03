import pytest

URL = "https://sp.hi.co.kr"

SUPPORTED_URL = "websquare/websquare.html?w2xPath=/common/xml/Login.xml"
UNSUPPORTED_URL = "Wizvera/veraport/install20/install_sample.html"
UNSUPPORTED_ALERT_MSG = "Chrome"

# the site tends to timeout unless viewing it over a VPN.
VPN_MESSAGE = "Please try again using a VPN set to South Korea."


async def do_test(client, redirect_url, alert=None):
    redirect = await client.promise_navigation_begins(url=redirect_url, timeout=10)
    if alert:
        alert = await client.await_alert(alert)
    try:
        await client.navigate(URL, timeout=10, no_skip=True)
    except Exception:
        pytest.skip(VPN_MESSAGE)
    if alert:
        assert await alert
    assert await redirect


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await do_test(client, SUPPORTED_URL)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await do_test(client, UNSUPPORTED_URL, alert=UNSUPPORTED_ALERT_MSG)
