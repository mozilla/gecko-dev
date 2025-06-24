import asyncio

import pytest

URL = "https://m.daum.net/"

APP_CSS = "a.link_daumapp"
TARGET_CHANNEL_CSS = ".tab_menu li[data-tab=channel_sports]"
SPORTS_SUBJECT_TEXT_CSS = "#channel_sports_top .head_tit .txt_head"


async def does_menu_slide_back(client):
    # this apz_click approach only works in RDM, not on an actual device,
    # so we have to limit our test to RDM.
    client.platform_override = "android"
    client.maybe_override_platform()

    # the site ends up sliding their channel list around using a CSS transform,
    # so we can read its transform style after each step to confirm whether it
    # changes as expected (it won't change at all in the final step if the bug
    # isn't being triggered).
    def get_transform(e):
        return client.execute_script(
            "return arguments[0].parentElement.style.transform", e
        )

    await client.navigate(URL)
    client.await_css(APP_CSS, is_displayed=True).click()
    target = client.await_css(TARGET_CHANNEL_CSS, is_displayed=True)
    old = get_transform(target)
    target.click()
    await asyncio.sleep(1)
    new = get_transform(target)
    assert old != new

    # the site flaw manifests by clicking on the main content, so we have to
    # click on an inert element like some plain text.
    neutral = client.await_css(SPORTS_SUBJECT_TEXT_CSS, is_displayed=True)
    await asyncio.sleep(1)
    coords = client.get_element_screen_position(neutral)
    coords = [coords[0], coords[1]]
    await client.apz_click(element=neutral)
    await asyncio.sleep(1)
    return new != get_transform(target)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await does_menu_slide_back(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await does_menu_slide_back(client)
