import pytest

URL = "https://www.prudential.com.hk/scws/member?locale=en"

SUPPORTED_CSS = "input#loginform-idNum"
UNSUPPORTED_TEXT = "browser is incompatible"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
