import pytest
from webdriver.error import NoSuchElementException

URL = "https://login.yahoo.com/"
USERNAME_CSS = "#login-username"
SIGNIN_CSS = "#login-signin"
TOGGLE_CSS = "#password-toggle-button"
RECAPTCHA_CSS = "#recaptcha-challenge"


async def is_password_reveal_toggle_fully_visible(client, in_headless_mode):
    if in_headless_mode:
        pytest.xfail("Skipping as cannot do reCAPTCHA in headless mode.")
        return False
    await client.navigate(URL)
    client.await_css(USERNAME_CSS).send_keys("webcompat")
    client.await_css(SIGNIN_CSS).click()
    client.await_css(RECAPTCHA_CSS, is_displayed=True)
    print("\a")  # beep to let the user know to do the reCAPTCHA
    try:
        toggle = client.await_css(TOGGLE_CSS, timeout=60)
    except NoSuchElementException:
        pytest.xfail(
            "Timed out waiting for reCAPTCHA to be completed. Please try again."
        )
        return False
    return client.execute_script(
        """
        const toggle = arguments[0].getBoundingClientRect();
        return toggle.width >= 16;
    """,
        toggle,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, in_headless_mode):
    assert await is_password_reveal_toggle_fully_visible(client, in_headless_mode)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, in_headless_mode):
    assert not await is_password_reveal_toggle_fully_visible(client, in_headless_mode)
