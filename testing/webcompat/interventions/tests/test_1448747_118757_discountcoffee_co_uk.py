import pytest

URL = "https://www.discountcoffee.co.uk/collections/ground-coffee-single-bags"
SELECT_CSS = "select#BrowseBy"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL)
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
