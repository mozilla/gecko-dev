import pytest

URL = "https://www.securityproperties.com/thornton/reserve-at-thornton-i-ii-apartments"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await client.test_entrata_banner_hidden(URL)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await client.test_entrata_banner_hidden(URL)
