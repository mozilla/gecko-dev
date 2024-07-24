import pytest

URL = "https://www.unimedlitoral.com.br/agendaonline/index.php?c=Authentication"
BLOCKED_CSS = ".navigators-page"
NOT_BLOCKED_CSS = "#loginEmail"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(NOT_BLOCKED_CSS)
    assert not client.find_css(BLOCKED_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(BLOCKED_CSS)
    assert not client.find_css(NOT_BLOCKED_CSS)
