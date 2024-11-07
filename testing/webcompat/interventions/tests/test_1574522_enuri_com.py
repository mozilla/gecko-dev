import pytest

URL = "http://enuri.com"
DESKTOP_VP = "width=1280"
MOBILE_VP = "width=device-width,initial-scale=1.0"


def get_meta_viewport_content(client):
    mvp = client.await_css("meta[name=viewport]")
    return client.get_element_attribute(mvp, "content")


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert MOBILE_VP == get_meta_viewport_content(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert DESKTOP_VP == get_meta_viewport_content(client)
