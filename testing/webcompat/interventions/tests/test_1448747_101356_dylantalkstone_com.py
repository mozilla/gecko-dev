import time

import pytest

URL = "https://dylantalkstone.com/collections/tele-pickups/products/flat-6-tele-pickups"
SELECT_CSS = "select#productSelect-option-0"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        # The page endlessly stalls while loading, but if we force-stop it then
        # that triggers it to show the broken selector right away.
        await client.navigate(URL, wait="none")
        time.sleep(3)
        client.execute_script("window.stop()")
        return client.test_for_fastclick(
            client.await_css(SELECT_CSS, is_displayed=True)
        )


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
