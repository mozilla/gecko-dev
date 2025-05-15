import pytest

URL = "https://www3.animeflv.net"
ANIME_WITH_ESTRENO_CSS = ".Anime .Estreno"


async def is_green_estreno_banner_visible(client):
    await client.navigate(URL, wait="none")
    estreno = client.await_css(ANIME_WITH_ESTRENO_CSS, is_displayed=True)
    # if we remove z-index from the parent link then the estreno banner will appear over it
    # as it should, so we can compare screenshots to see if it was over it to begin with.
    pre = estreno.screenshot()
    client.execute_script("arguments[0].parentElement.style.zIndex = 'auto'", estreno)
    post = estreno.screenshot()
    return pre == post


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_green_estreno_banner_visible(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_green_estreno_banner_visible(client)
