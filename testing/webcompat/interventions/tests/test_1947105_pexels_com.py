import pytest

URL = "https://www.pexels.com/photo/stylish-woman-relaxing-outdoors-in-autumn-fashion-29946756/"

FREE_DOWNLOAD_BUTTON_CSS = (
    "a[class*=Button][href*='dl=pexels-ekoagalarov-29946756.jpg']"
)
POPUP_CSS = ".ReactModal__Content[class*=after-open]"


async def is_popup_offscreen(client):
    await client.navigate(URL)
    client.soft_click(client.await_css(FREE_DOWNLOAD_BUTTON_CSS, is_displayed=True))
    popup = client.await_css(POPUP_CSS, is_displayed=True)
    return client.execute_script(
        """
        return arguments[0].getBoundingClientRect().top < 0;
      """,
        popup,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_popup_offscreen(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_popup_offscreen(client)
