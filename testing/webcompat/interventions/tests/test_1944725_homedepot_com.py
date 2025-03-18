import pytest

URL = "https://www.homedepot.com/s/Square%20recessed%20light?NCNI-5"

DRAWER_CSS = "aside.thd-sliding-drawer"


async def is_drawer_onscreen(client):
    client.set_screen_size(400, 800)
    await client.navigate(URL, wait="none")
    drawer = client.await_css(DRAWER_CSS, is_displayed=True)
    return client.execute_script(
        """
      return arguments[0].getBoundingClientRect().right > 0
    """,
        drawer,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_drawer_onscreen(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_drawer_onscreen(client)
