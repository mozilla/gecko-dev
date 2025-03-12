import pytest

URL = "https://www.nytimes.com/interactive/projects/modern-love/36-questions/?ref=redirector"

START_BUTTON_CSS = "#intro .button.full-width"
UNSUPPORTED_TEXT = "Android Chrome"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(START_BUTTON_CSS)
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
