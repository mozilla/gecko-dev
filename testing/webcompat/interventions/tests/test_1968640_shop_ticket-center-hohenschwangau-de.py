import pytest

URL = "https://shop.ticket-center-hohenschwangau.de/Shop/Index/en/39901"
TICKET_CSS = "#TicketListContainer .RespTicket"
SELECT_CSS = "select#nPlaces"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL)
        client.await_css(TICKET_CSS, is_displayed=True).click()
        return client.test_for_fastclick(client.await_css(SELECT_CSS))


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
