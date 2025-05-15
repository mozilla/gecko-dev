import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.td.com/ca/en/about-td/diversity-and-inclusion/black-communities-in-canada?cm_sp=c000-20-2404"

COOKIES_CLOSE_CSS = ".onetrust-close-btn-handler"
VIDEO_CSS = "a.cmp-banner-container__video-container-link"
CONTAINER_CSS = ".videoModal.container .cmp-dialog__details"


async def is_scrollbar_visible(client):
    await client.navigate(URL)

    try:
        client.await_css(COOKIES_CLOSE_CSS, is_displayed=True, timeout=4).click()
        client.await_element_hidden(client.css(COOKIES_CLOSE_CSS))
    except NoSuchElementException:
        pass

    client.execute_script("HTMLMediaElement.prototype.play = () => {}")
    client.await_css(VIDEO_CSS, is_displayed=True).click()
    container = client.await_css(CONTAINER_CSS, is_displayed=True)
    return client.execute_script(
        """
      const container = arguments[0];
      const { width, height } = container.getBoundingClientRect();
      return container.scrollWidth !== Math.round(width) || container.scrollHeight !== Math.round(height);
    """,
        container,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.need_visible_scrollbars
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_scrollbar_visible(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.need_visible_scrollbars
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_scrollbar_visible(client)
