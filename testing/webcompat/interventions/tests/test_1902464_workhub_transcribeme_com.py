import asyncio

import pytest

URL = "https://workhub.transcribeme.com/Exam"

USERNAME_CSS = "input[name=username]"
PASSWORD_CSS = "input[name=password]"
SIGN_IN_CSS = "#btn-login"
UNSUPPORTED_CSS = "#chromeOnlyPopup"


async def does_unsupported_popup_appear(client, credentials):
    await client.navigate(URL)

    username = client.await_css(USERNAME_CSS)
    password = client.find_css(PASSWORD_CSS)
    sign_in = client.find_css(SIGN_IN_CSS)
    assert client.is_displayed(username)
    assert client.is_displayed(password)
    assert client.is_displayed(sign_in)

    username.send_keys(credentials["username"])
    password.send_keys(credentials["password"])
    sign_in.click()

    client.await_css(UNSUPPORTED_CSS)
    await asyncio.sleep(1)
    return client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, credentials):
    assert not await does_unsupported_popup_appear(client, credentials)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, credentials):
    assert await does_unsupported_popup_appear(client, credentials)
