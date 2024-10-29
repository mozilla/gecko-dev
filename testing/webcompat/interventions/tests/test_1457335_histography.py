import pytest

URL = "http://histography.io/"
SUPPORT_URL = "http://histography.io/browser_support.htm"
LOADING_CSS = "#loading"
HERO_CSS = "#graph_tip .gotit"
UNSUPPORTED_TEXT = "WE ARE CURRENTLY NOT SUPPORTING YOUR BROWSER"

# Note that this page often stalls while loading


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    assert client.await_css(LOADING_CSS, is_displayed=True)
    client.await_element_hidden(client.css(LOADING_CSS))
    assert client.await_css(HERO_CSS, is_displayed=True)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="none")
    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
