import pytest

URL = "https://m.modanisa.com/en-sevilen-modanisa-markalari12.list"
DESKTOP_VP = "width=320, user-scalable=no, initial-scale=3.0625"
MOBILE_VP = "width=320, user-scalable=no"


async def get_meta_viewport_content(client):
    await client.navigate(URL, wait="none")
    mvp = client.await_css("meta[name=viewport]")
    return client.get_element_attribute(mvp, "content")


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert MOBILE_VP == await get_meta_viewport_content(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert DESKTOP_VP == await get_meta_viewport_content(client)
