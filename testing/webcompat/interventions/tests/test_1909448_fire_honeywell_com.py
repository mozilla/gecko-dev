import pytest

URL = "https://fire.honeywell.com/"
LANDING_PAGE_CSS = ".landing-page"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="load")
    assert client.await_css(LANDING_PAGE_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="load")
    assert not client.find_css(LANDING_PAGE_CSS)
