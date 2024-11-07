import pytest

URL = "https://www.cwb.digital/apps/cwbDigital/#_frmLogin"
LOGIN_CSS = "#okta_login"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(LOGIN_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message="ReferenceError: $ is not defined")
