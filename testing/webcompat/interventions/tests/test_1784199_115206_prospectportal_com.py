import pytest

URL = "https://thepointeatcentral.prospectportal.com/Apartments/module/application_authentication?_ga=2.250831814.1585158328.1588776830-1793033216.1588776830"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await client.test_entrata_banner_hidden(URL)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await client.test_entrata_banner_hidden(URL)
