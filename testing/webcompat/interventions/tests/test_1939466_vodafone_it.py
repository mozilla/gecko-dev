import pytest

URL = "https://www.vodafone.it/eshop/contenuti/rete-vodafone/copertura-mobile-5g.html"
FRAME_CSS = ".iframeContainer iframe[src*='geamaps.dsl.vodafone.it']"
SUCCESS_CSS = "#comuni_provincia_autocomplete"
FAILURE_TEXT = "No right to access page!"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    client.switch_to_frame(client.await_css(FRAME_CSS, is_displayed=True))
    assert client.await_css(SUCCESS_CSS, is_displayed=True)
    assert not client.find_text(FAILURE_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="none")
    client.switch_to_frame(client.await_css(FRAME_CSS, is_displayed=True))
    assert client.await_text(FAILURE_TEXT, is_displayed=True)
    assert not client.find_css(SUCCESS_CSS, is_displayed=True)
