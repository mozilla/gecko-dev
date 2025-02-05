import pytest

URL = "https://www.collinsdictionary.com/us/dictionary/english-pronunciations/valuable"
LOGIN_CSS = "div.play-button"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(LOGIN_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(
        URL,
        await_console_message="TypeError: error loading dynamically imported module: https://openfpcdn.io/botd/v1",
    )
