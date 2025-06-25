import pytest

URL = "https://www.qq.com/"

LOGIN_CSS = "#qqcom-login .login-button"
CHECKBOX_CSS = ".qqcom-modal-content #web-choose"
QQ_BUTTON_CSS = "#QQ-face"
IFRAME_CSS = "iframe[src*=login_qq_news_web]"
WAIT_LOG_MSG = "load event end at"
REPLACED_CSS = ".accredit_info_op li input[type=checkbox]"


async def does_checkmark_appear(client):
    await client.navigate(URL, wait="none")
    client.await_css(LOGIN_CSS, is_displayed=True).click()
    client.await_css(CHECKBOX_CSS, is_displayed=True).click()
    client.await_css(QQ_BUTTON_CSS, is_displayed=True).click()
    client.switch_frame(client.await_css(IFRAME_CSS))
    # by the time this console message is logged, the checkbox should be ready to check
    await (await client.promise_console_message_listener(WAIT_LOG_MSG))
    return not client.is_one_solid_color(client.await_css(REPLACED_CSS))


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await does_checkmark_appear(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await does_checkmark_appear(client)
