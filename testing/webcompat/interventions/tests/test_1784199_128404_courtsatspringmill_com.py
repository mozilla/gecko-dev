import pytest

URL = "https://www.courtsatspringmill.com/floorplans/"
IFRAME_CSS = "iframe[title='Floor Plans']"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await client.test_entrata_banner_hidden(URL, IFRAME_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await client.test_entrata_banner_hidden(URL, IFRAME_CSS)
