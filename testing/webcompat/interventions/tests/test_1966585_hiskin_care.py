import pytest

URL = "https://hiskin.care/pages/complete-booking"
IFRAME_CSS = "iframe[src='https://booking-v3.hiskin.care/']"


async def is_frame_visible(client):
    await client.navigate(URL, wait="none")
    frame = client.await_css(IFRAME_CSS, is_displayed=True)
    return client.execute_script("return arguments[0].clientHeight > 0", frame)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_frame_visible(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_frame_visible(client)
