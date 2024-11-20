import pytest

URL = "https://eu-trade.naeu.playblackdesert.com/"

LOGIN_CSS = "#_email"
BAD_TEXT = "This browser is not supported."


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(LOGIN_CSS, is_displayed=True)
    assert not client.find_text(BAD_TEXT, is_displayed=True)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_text(BAD_TEXT, is_displayed=True, timeout=4000)
    assert not client.find_css(LOGIN_CSS, is_displayed=True)
