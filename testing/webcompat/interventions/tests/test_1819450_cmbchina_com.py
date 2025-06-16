import pytest

URL = "https://www.cmbchina.com/"
DESKTOP_CSS = "#aspnetForm"
MOBILE_CSS = "#ice-container"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    assert client.await_css(MOBILE_CSS, is_displayed=True)
    assert not client.find_css(DESKTOP_CSS, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="none")
    assert client.await_css(DESKTOP_CSS, is_displayed=True)
    assert not client.find_css(MOBILE_CSS, is_displayed=True)
