import pytest

URL = "https://forum.memurlar.net/konu/2367955/"
DESKTOP_CSS = "#finance-band"
MOBILE_CSS = "header.mobil.grid"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(MOBILE_CSS, is_displayed=True)
    assert not client.find_css(DESKTOP_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(DESKTOP_CSS, is_displayed=True)
    assert not client.find_css(MOBILE_CSS)
