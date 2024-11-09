import pytest

URL = "https://www.nsandi.com/"

CHAT_BUTTON_CSS = "#GCLauncherbtn"
UNSUPPORTED_CSS = "#unsupported-browser"


async def is_warning_shown(client):
    await client.navigate(URL)
    client.await_css(CHAT_BUTTON_CSS, is_displayed=True)
    client.await_css(UNSUPPORTED_CSS)
    return client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_warning_shown(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_warning_shown(client)
