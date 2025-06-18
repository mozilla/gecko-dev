import pytest

URL = "https://fire.honeywell.com/"
LANDING_PAGE_CSS = ".landing-page"


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_regression(client):
    await client.navigate(URL, wait="load")
    assert client.find_css(LANDING_PAGE_CSS)
