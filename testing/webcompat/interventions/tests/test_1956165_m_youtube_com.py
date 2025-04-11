import asyncio

import pytest

URL = "https://m.youtube.com/results?search_query=trailer"
FIRST_VIDEO_CSS = "a[href*=watch]"
FULLSCREEN_ICON_CSS = "button.fullscreen-icon"
PLAY_BUTTON_CSS = ".ytp-large-play-button.ytp-button"


async def pip_activates_properly(client):
    await client.make_preload_script("delete navigator.__proto__.webdriver")

    await client.navigate(URL)
    client.await_css(FIRST_VIDEO_CSS, is_displayed=True).click()

    # wait for the video to start playing
    client.execute_async_script(
        """
            const done = arguments[0];
            const i = setInterval(() => {
                const vid = document.querySelector("video");
                if (vid && !vid.paused) {
                    done();
                }
            }, 100);
        """
    )

    client.soft_click(client.await_css(FULLSCREEN_ICON_CSS, is_displayed=True))

    await asyncio.sleep(1)
    with client.using_context("chrome"):
        client.execute_script(
            """
            ChromeUtils.androidMoveTaskToBack();
        """
        )

    await asyncio.sleep(1)
    return client.execute_script(
        """
            return !(document.querySelector("video")?.paused);
        """
    )


@pytest.mark.only_platforms("fenix")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await pip_activates_properly(client)


@pytest.mark.only_platforms("fenix")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await pip_activates_properly(client)
