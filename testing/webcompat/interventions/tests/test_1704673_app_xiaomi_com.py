import pytest
from webdriver.bidi.error import UnknownErrorException

# This site fails with a redirect loop with a Firefox UA

URL = "http://app.xiaomi.com/"
REDIR_FAILURE_TEXT = "ERROR_REDIRECT_LOOP"
SUCCESS_CSS = "#J_mingleList"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUCCESS_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    # We will either get a static error page with the text, or
    # an UnknownErrorException from WebDriver.
    try:
        await client.navigate(URL, wait="none")
        assert client.await_text(REDIR_FAILURE_TEXT, timeout=30)
    except UnknownErrorException:
        assert True
