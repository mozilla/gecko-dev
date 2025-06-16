import pytest

URL = "https://www.creativebloq.com/"


async def do_test(client, shouldFail):
    # the problem only exhibits on narrow enough viewports
    client.set_screen_size(800, 800)
    await client.navigate(URL, wait="none")
    client.test_future_plc_trending_scrollbar(shouldFail=shouldFail)


@pytest.mark.skip_platforms("android")
@pytest.mark.need_visible_scrollbars
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await do_test(client, False)


@pytest.mark.skip_platforms("android")
@pytest.mark.need_visible_scrollbars
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await do_test(client, True)
