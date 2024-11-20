import pytest
from webdriver.error import NoSuchElementException

URL = "https://qceservices.quezoncity.gov.ph/qcvaxeasy"

IFRAME_CSS = "iframe#qcid_iframe"
RECAPTCHA_CSS = "iframe[title*=captcha]"


async def is_iframe_visible(client):
    await client.navigate(URL)

    try:
        client.await_css(RECAPTCHA_CSS, is_displayed=True, timeout=5)
        try:
            print("\a")
            iframe = client.await_css(IFRAME_CSS, is_displayed=True, timeout=120)
        except NoSuchElementException:
            pytest.xfail(
                "Timed out waiting for reCAPTCHA to be completed. Please try again."
            )
            return False
    except NoSuchElementException:
        iframe = client.await_css(IFRAME_CSS, is_displayed=True)
        pass

    assert iframe
    return client.execute_script(
        """
        return arguments[0].getBoundingClientRect().height > 200;
    """,
        iframe,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_iframe_visible(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_iframe_visible(client)
