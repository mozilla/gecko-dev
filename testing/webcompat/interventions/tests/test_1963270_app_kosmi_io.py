import pytest
from webdriver.error import NoSuchElementException, StaleElementReferenceException

URL = "https://app.kosmi.io"
USERNAME_CSS = "input[placeholder='Username or Email']"
PASSWORD_CSS = "input[placeholder='Password']"
LOGIN_BUTTON_CSS = "[type=submit]"
MY_ROOM_TEXT = "My room"
SELECT_MEDIA_CSS = ".kosmi.mediabutton"
HERO_CSS = "button i.linkify"
LOCAL_FILE_CSS = "button i.file.video"


async def is_local_file_option_shown(client, credentials, platform):
    await client.navigate(URL, wait="none")
    client.await_css(
        "button",
        condition="elem.innerText.replaceAll(' ', '').includes('Login')",
        is_displayed=True,
    ).click()
    client.await_css(
        "button",
        condition="elem.innerText.includes('Login with Email')",
        is_displayed=True,
    ).click()
    client.await_css(USERNAME_CSS, is_displayed=True).send_keys(credentials["username"])
    client.await_css(PASSWORD_CSS, is_displayed=True).send_keys(credentials["password"])
    client.soft_click(client.await_css(LOGIN_BUTTON_CSS, is_displayed=True))
    for _ in range(5):
        try:
            if platform == "android":
                await client.apz_click(
                    element=client.await_css(
                        "div",
                        condition="elem.firstChild?.nodeValue?.includes('\\'s room')",
                        is_displayed=True,
                        timeout=0.5,
                    )
                )
            else:
                client.await_css(
                    "button",
                    condition="elem.firstChild?.nodeValue?.includes('My room')",
                    is_displayed=True,
                    timeout=0.5,
                ).click()
            client.await_css(HERO_CSS, is_displayed=True, timeout=1)
            break
        except (NoSuchElementException, StaleElementReferenceException):
            pass
    for _ in range(5):
        try:
            client.soft_click(client.await_css(SELECT_MEDIA_CSS, is_displayed=True))
            client.await_css(HERO_CSS, is_displayed=True, timeout=0.5)
            break
        except NoSuchElementException:
            pass
    client.await_css(HERO_CSS, is_displayed=True)
    return client.find_css(LOCAL_FILE_CSS)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, credentials, platform):
    assert await is_local_file_option_shown(client, credentials, platform)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, credentials, platform):
    assert not await is_local_file_option_shown(client, credentials, platform)
