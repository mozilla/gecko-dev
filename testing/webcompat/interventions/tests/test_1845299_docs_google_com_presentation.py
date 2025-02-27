import pytest

URL = "https://docs.google.com/presentation/d/1Xb82OcbdIFC0LjFok2IA2G_oGdtpueysQXHb78d_iUU/edit"

SCROLL_CONTAINER_CSS = "#speakernotes"


async def is_scrollbar_hidden(client):
    await client.navigate(URL)
    container = client.await_css(SCROLL_CONTAINER_CSS, is_displayed=True)
    return client.execute_script(
        """
       return arguments[0].clientWidth == arguments[0].offsetWidth;
     """,
        container,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.need_visible_scrollbars
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_scrollbar_hidden(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.need_visible_scrollbars
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_scrollbar_hidden(client)
