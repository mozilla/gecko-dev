import asyncio

import pytest

URL = "https://lmiamap.ca/"

FRAME_CSS = "iframe[src*=powerbi]"
MAP_CSS = ".MicrosoftMap"
CANVAS_CSS = "[id='Microsoft.Maps.Imagery.RoadSceneWithoutLabels']"
HERO_CSS = "svg .mapBubbles:not(:empty)"


async def can_zoom_maps(client):
    await client.navigate(URL, wait="none")
    client.switch_frame(client.await_css(FRAME_CSS, is_displayed=True))
    map = client.await_css(MAP_CSS, is_displayed=True)
    canvas = client.await_css(CANVAS_CSS, is_displayed=True)

    # wait for the map to load and settle down (it zooms in and adds SVG annotations)
    client.await_css(HERO_CSS, is_displayed=True)
    await asyncio.sleep(2)

    # now we scroll and see if the canvas' transform changes (it snaps back to its original transform afterward)
    pre = client.execute_script("return arguments[0].style.transform", canvas)
    for i in range(30):
        await client.send_apz_scroll_gesture(4, element=map, offset=[40, 40])
        await asyncio.sleep(0.05)
        if pre != client.execute_script(
            "return arguments[0].style.transform",
            canvas,
        ):
            return True
    return False


# Android is unaffected; Linux may be, but testing scroll gestures doesn't seem to work.


@pytest.mark.skip_platforms("android", "linux")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_zoom_maps(client)


@pytest.mark.skip_platforms("android", "linux")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_zoom_maps(client)
