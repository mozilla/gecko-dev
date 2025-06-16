import pytest
from webdriver.error import NoSuchElementException

URL = "https://eksiseyler.com/evrimin-kisa-surede-de-yasanabilecegini-kanitlayan-1971-hirvatistan-kertenkele-deneyi"
CAPTCHA_TEXT = "Verifying you are human"
IMAGE_CSS = ".content-heading .cover-img img"
ERROR_MSG = "loggingEnabled is not defined"
INFINITE_CAPTCHA_MSG = (
    "Seem to be stuck in an infinite Captcha; please test this page manually."
)


async def visit_site(client):
    await client.navigate(URL)
    try:
        # Unfortunately, the site tends to show an infinitely-repeating non-interactive Cloudflare Captcha now.
        client.await_text(CAPTCHA_TEXT, is_displayed=True, timeout=4)
        pytest.skip(INFINITE_CAPTCHA_MSG)
        return False
    except NoSuchElementException:
        return True


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    if await visit_site(client):
        client.await_css(IMAGE_CSS, condition="!elem.src.includes('placeholder')")


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    console_msg = await client.promise_console_message_listener(ERROR_MSG)
    if await visit_site(client):
        await console_msg
        client.await_css(IMAGE_CSS, condition="elem.src.includes('placeholder')")
