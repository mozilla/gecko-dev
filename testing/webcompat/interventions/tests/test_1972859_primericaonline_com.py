import pytest

URL = "https://login.primericaonline.com/"
SUCCESS_CSS = "#login-content"
ERROR_MSG = "ancestorOrigins is undefined"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    assert client.await_css(SUCCESS_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
