import pytest

URL = "https://conference.amwell.com/"

SUPPORTED_CSS = "#code"
UNSUPPORTED_TITLE = "Incompatible Browser"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="complete")
    client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert UNSUPPORTED_TITLE != client.execute_script("return document.title")


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, wait="complete"):
    await client.navigate(URL)
    assert UNSUPPORTED_TITLE == client.execute_script("return document.title")
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
