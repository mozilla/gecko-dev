import pytest

URL = "https://chaturbate.com/"
AGREE_CSS = "#close_entrance_terms"
FIRST_ROOM_CSS = "a.room_thumbnail_container[href]"
FS_CSS = "[data-testid='mobile-fullscreen-button']"
VID_CSS = ".videoPlayerDiv"


async def is_requestFullscreen_called(client):
    await client.navigate(URL)
    client.await_css(AGREE_CSS, is_displayed=True).click()
    client.await_css(FIRST_ROOM_CSS, is_displayed=True).click()
    vid = client.await_css(VID_CSS, is_displayed=True)
    fs = client.await_css(FS_CSS, is_displayed=True)
    height = client.execute_script(
        """
      Element.prototype.requestFullscreen = () => window.fs_pressed = true
      return arguments[0].getBoundingClientRect().height;
    """,
        vid,
    )
    fs.click()
    return client.execute_async_script(
        """
      const [vidSel, height, done] = arguments;
      setInterval(() => {
        if (document.querySelector(vidSel)?.getBoundingClientRect().height > height) {
          done(window.fs_pressed);
        }
      }, 100);
    """,
        VID_CSS,
        height,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_requestFullscreen_called(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_requestFullscreen_called(client)
