import pytest

URL = "https://sp.hi.co.kr"

SUPPORTED_URL = "websquare/websquare.html?w2xPath=/common/xml/Login.xml"
UNSUPPORTED_URL = "Wizvera/veraport/install20/install_sample.html"
UNSUPPORTED_ALERT_MSG = "Chrome"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    redirect = await client.promise_navigation_begins(url=SUPPORTED_URL, timeout=10)
    await client.navigate(URL)
    assert await redirect


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    redirect = await client.promise_navigation_begins(url=UNSUPPORTED_URL, timeout=10)
    alert = await client.await_alert(UNSUPPORTED_ALERT_MSG)
    await client.navigate(URL)
    assert await alert
    assert await redirect
