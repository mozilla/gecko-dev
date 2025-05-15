import pytest

URL = "https://cesupils.lv/"

WORKING_CSS = "#menu-darba-1"
BROKEN_CSS = "#error-page"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(WORKING_CSS)
    assert not client.find_css(BROKEN_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(BROKEN_CSS, is_displayed=True)
    assert not client.find_css(WORKING_CSS, is_displayed=True)
