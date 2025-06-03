import pytest

URL = "https://www.dublinexpress.ie/"
COOKIE_BANNER_CSS = "#onetrust-consent-sdk"
SELECT_CSS = "select#from-dropdown"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL)
        client.hide_elements(COOKIE_BANNER_CSS)
        select = client.await_css(SELECT_CSS, is_displayed=True)
        return client.test_for_fastclick(select)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_fastclick_active(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_fastclick_active(client)
