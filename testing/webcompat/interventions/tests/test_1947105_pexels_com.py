import pytest

URL = "https://www.pexels.com/photo/stylish-woman-relaxing-outdoors-in-autumn-fashion-29946756/"

FREE_DOWNLOAD_BUTTON_CSS = (
    "a[class*=Button][href*='dl=pexels-ekoagalarov-29946756.jpg']"
)
POPUP_CSS = ".ReactModal__Content[class*=after-open]"


async def check_popup(client, should_be_offscreen):
    # The issue is intermittent, so we run the test up to 10 times to be relatively confident.
    # If the popup is never offscreen with the intervention on, we pass that check.
    # If the popup is offscreen even once without the intervention, we pass that check.
    for _ in range(10):
        await client.navigate(URL)
        client.soft_click(client.await_css(FREE_DOWNLOAD_BUTTON_CSS, is_displayed=True))
        popup = client.await_css(POPUP_CSS, is_displayed=True)
        is_off = client.execute_script(
            """
            return arguments[0].getBoundingClientRect().top < 0;
          """,
            popup,
        )
        if should_be_offscreen and is_off:
            return True
        elif not should_be_offscreen and is_off:
            return False
    return not should_be_offscreen


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_popup(client, False)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await check_popup(client, True)
