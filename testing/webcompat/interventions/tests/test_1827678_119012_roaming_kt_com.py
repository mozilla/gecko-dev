import pytest

URL = "https://roaming.kt.com/esim/eng/reserve.asp"

SUPPORTED_ALERT = "collection and use of private information is necessary"
UNSUPPORTED_ALERT = "browser is not supported"


async def shows_unsupported_alert(client):
    # the page loads for a long time; let's not wait unnecessarily
    await client.make_preload_script(
        """
         const int = setInterval(() => {
            if (window.simplepay_view) {
                clearInterval(int);
                window.simplepay_view();
            }
         }, 100);
    """
    )
    which_alert = await client.await_alert([SUPPORTED_ALERT, UNSUPPORTED_ALERT])
    await client.navigate(URL, wait="none")
    return UNSUPPORTED_ALERT in await which_alert


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await shows_unsupported_alert(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await shows_unsupported_alert(client)
